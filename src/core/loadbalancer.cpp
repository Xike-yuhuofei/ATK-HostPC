#include "loadbalancer.h"
#include "continuousoptimizer.h"
#include "intelligentanalyzer.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QStandardPaths>
#include <QDir>
#include <QtMath>
#include <QRandomGenerator>
#include <QCoreApplication>
#include <QRunnable>
#include <algorithm>

// 任务执行器类
class TaskRunner : public QRunnable
{
public:
    TaskRunner(const LoadBalancer::TaskInfo& task, LoadBalancer* balancer)
        : m_task(task), m_balancer(balancer)
    {
        setAutoDelete(true);
    }
    
    void run() override
    {
        try {
            if (m_task.task) {
                m_task.task();
            }
        } catch (const std::exception& e) {
            qWarning() << "[TaskRunner] 任务执行异常:" << e.what();
        } catch (...) {
            qWarning() << "[TaskRunner] 任务执行未知异常";
        }
    }
    
private:
    LoadBalancer::TaskInfo m_task;
    LoadBalancer* m_balancer;
};

LoadBalancer::LoadBalancer(QObject *parent)
    : QObject(parent)
    , m_optimizer(nullptr)
    , m_analyzer(nullptr)
    , m_balancingTimer(new QTimer(this))
    , m_monitoringTimer(new QTimer(this))
    , m_metricsTimer(new QTimer(this))
    , m_cleanupTimer(new QTimer(this))
    , m_threadPool(new QThreadPool(this))
    , m_strategy(BalancingStrategy::Adaptive)
    , m_maxWorkers(QThread::idealThreadCount())
    , m_taskTimeout(30000)
    , m_balancingInterval(5000)
    , m_monitoringInterval(2000)
    , m_metricsInterval(10000)
    , m_cleanupInterval(60000)
    , m_maxQueueSize(1000)
    , m_maxRetries(3)
    , m_resourceThreshold(80.0)
    , m_isRunning(false)
    , m_taskIdCounter(0)
{
    // 连接定时器
    connect(m_balancingTimer, &QTimer::timeout, this, &LoadBalancer::processTaskQueue);
    connect(m_monitoringTimer, &QTimer::timeout, this, &LoadBalancer::monitorWorkers);
    connect(m_metricsTimer, &QTimer::timeout, this, &LoadBalancer::updateMetrics);
    connect(m_cleanupTimer, &QTimer::timeout, this, &LoadBalancer::cleanupCompletedTasks);
    
    // 设置线程池
    m_threadPool->setMaxThreadCount(m_maxWorkers);
    
    // 初始化指标
    m_startTime = QDateTime::currentDateTime();
    m_metrics = BalancingMetrics();
    
    // 初始化资源监控
    setupResourceMonitoring();
    
    qDebug() << "[LoadBalancer] 负载均衡器已创建，最大工作线程:" << m_maxWorkers;
}

LoadBalancer::~LoadBalancer()
{
    stopBalancing();
    saveBalancingState();
    qDebug() << "[LoadBalancer] 负载均衡器已销毁";
}

bool LoadBalancer::initialize(ContinuousOptimizer* optimizer, IntelligentAnalyzer* analyzer)
{
    m_optimizer = optimizer;
    m_analyzer = analyzer;
    
    // 加载状态
    loadBalancingState();
    
    // 初始化默认工作线程
    initializeDefaultWorkers();
    
    qDebug() << "[LoadBalancer] 初始化完成";
    return true;
}

void LoadBalancer::setBalancingStrategy(BalancingStrategy strategy)
{
    m_strategy = strategy;
    
    // 根据策略调整参数
    switch (strategy) {
    case BalancingStrategy::RoundRobin:
        m_balancingInterval = 3000;
        break;
    case BalancingStrategy::LeastLoaded:
        m_balancingInterval = 2000;
        break;
    case BalancingStrategy::WeightedRoundRobin:
        m_balancingInterval = 4000;
        break;
    case BalancingStrategy::ResourceBased:
        m_balancingInterval = 1500;
        break;
    case BalancingStrategy::Adaptive:
        m_balancingInterval = 5000;
        break;
    }
    
    if (m_isRunning) {
        m_balancingTimer->setInterval(m_balancingInterval);
    }
    
    qDebug() << "[LoadBalancer] 负载均衡策略已设置为:" << static_cast<int>(strategy);
}

void LoadBalancer::setMaxWorkers(int maxWorkers)
{
    m_maxWorkers = qMax(1, qMin(maxWorkers, 32));
    m_threadPool->setMaxThreadCount(m_maxWorkers);
    
    qDebug() << "[LoadBalancer] 最大工作线程数已设置为:" << m_maxWorkers;
}

void LoadBalancer::setTaskTimeout(int timeoutMs)
{
    m_taskTimeout = qMax(1000, timeoutMs);
    qDebug() << "[LoadBalancer] 任务超时时间已设置为:" << m_taskTimeout << "ms";
}

bool LoadBalancer::addWorker(const QString& workerId, const QHash<ResourceType, double>& capabilities)
{
    QMutexLocker locker(&m_workersMutex);
    
    if (workerId.isEmpty() || m_workers.contains(workerId)) {
        qWarning() << "[LoadBalancer] 工作线程ID无效或已存在:" << workerId;
        return false;
    }
    
    WorkerInfo worker;
    worker.id = workerId;
    worker.thread = nullptr;
    worker.busy = false;
    worker.cpuLoad = 0.0;
    worker.memoryUsage = 0.0;
    worker.activeTasks = 0;
    worker.completedTasks = 0;
    worker.failedTasks = 0;
    worker.lastTaskCompleted = QDateTime::currentDateTime();
    worker.capabilities = capabilities;
    worker.efficiency = 1.0;
    worker.enabled = true;
    
    m_workers[workerId] = worker;
    
    emit workerAdded(workerId);
    
    qDebug() << "[LoadBalancer] 工作线程已添加:" << workerId;
    return true;
}

bool LoadBalancer::removeWorker(const QString& workerId)
{
    QMutexLocker locker(&m_workersMutex);
    
    if (!m_workers.contains(workerId)) {
        qWarning() << "[LoadBalancer] 工作线程不存在:" << workerId;
        return false;
    }
    
    auto& worker = m_workers[workerId];
    
    // 等待当前任务完成
    if (worker.busy && worker.activeTasks > 0) {
        qWarning() << "[LoadBalancer] 工作线程正在执行任务，无法移除:" << workerId;
        return false;
    }
    
    m_workers.remove(workerId);
    
    emit workerRemoved(workerId);
    
    qDebug() << "[LoadBalancer] 工作线程已移除:" << workerId;
    return true;
}

void LoadBalancer::enableWorker(const QString& workerId, bool enabled)
{
    QMutexLocker locker(&m_workersMutex);
    
    if (m_workers.contains(workerId)) {
        m_workers[workerId].enabled = enabled;
        qDebug() << "[LoadBalancer] 工作线程" << workerId << (enabled ? "已启用" : "已禁用");
    }
}

QList<LoadBalancer::WorkerInfo> LoadBalancer::getWorkers() const
{
    QMutexLocker locker(&m_workersMutex);
    return m_workers.values();
}

QString LoadBalancer::submitTask(const QString& taskName, TaskPriority priority, 
                                ResourceType primaryResource, 
                                const QHash<ResourceType, double>& requirements,
                                std::function<void()> task)
{
    QMutexLocker locker(&m_tasksMutex);
    
    if (m_taskQueue.size() >= m_maxQueueSize) {
        qWarning() << "[LoadBalancer] 任务队列已满，无法提交任务:" << taskName;
        return QString();
    }
    
    TaskInfo taskInfo;
    taskInfo.id = generateTaskId();
    taskInfo.name = taskName;
    taskInfo.priority = priority;
    taskInfo.primaryResource = primaryResource;
    taskInfo.resourceRequirements = requirements;
    taskInfo.task = task;
    taskInfo.submittedAt = QDateTime::currentDateTime();
    taskInfo.retryCount = 0;
    taskInfo.completed = false;
    taskInfo.failed = false;
    taskInfo.estimatedDuration = 1000.0; // 默认1秒
    taskInfo.actualDuration = 0.0;
    
    // 按优先级插入队列
    bool inserted = false;
    for (int i = 0; i < m_taskQueue.size(); ++i) {
        if (static_cast<int>(priority) > static_cast<int>(m_taskQueue[i].priority)) {
            m_taskQueue.insert(i, taskInfo);
            inserted = true;
            break;
        }
    }
    
    if (!inserted) {
        m_taskQueue.enqueue(taskInfo);
    }
    
    m_metrics.totalTasks++;
    m_metrics.queuedTasks++;
    m_metrics.tasksByPriority[priority]++;
    
    emit taskSubmitted(taskInfo.id, taskName);
    
    qDebug() << "[LoadBalancer] 任务已提交:" << taskInfo.id << taskName;
    
    return taskInfo.id;
}

bool LoadBalancer::cancelTask(const QString& taskId)
{
    QMutexLocker locker(&m_tasksMutex);
    
    // 从队列中移除
    for (int i = 0; i < m_taskQueue.size(); ++i) {
        if (m_taskQueue[i].id == taskId) {
            m_taskQueue.removeAt(i);
            m_metrics.queuedTasks--;
            qDebug() << "[LoadBalancer] 任务已从队列中取消:" << taskId;
            return true;
        }
    }
    
    // 检查是否在执行中
    if (m_activeTasks.contains(taskId)) {
        qWarning() << "[LoadBalancer] 任务正在执行中，无法取消:" << taskId;
        return false;
    }
    
    qWarning() << "[LoadBalancer] 任务不存在:" << taskId;
    return false;
}

bool LoadBalancer::retryTask(const QString& taskId)
{
    QMutexLocker locker(&m_tasksMutex);
    
    if (!m_completedTasks.contains(taskId)) {
        qWarning() << "[LoadBalancer] 任务不存在或未完成:" << taskId;
        return false;
    }
    
    auto taskInfo = m_completedTasks[taskId];
    
    if (!taskInfo.failed) {
        qWarning() << "[LoadBalancer] 任务未失败，无需重试:" << taskId;
        return false;
    }
    
    if (taskInfo.retryCount >= m_maxRetries) {
        qWarning() << "[LoadBalancer] 任务重试次数已达上限:" << taskId;
        return false;
    }
    
    // 重新提交任务
    taskInfo.id = generateTaskId();
    taskInfo.retryCount++;
    taskInfo.failed = false;
    taskInfo.completed = false;
    taskInfo.errorMessage.clear();
    taskInfo.submittedAt = QDateTime::currentDateTime();
    
    m_taskQueue.enqueue(taskInfo);
    m_metrics.queuedTasks++;
    
    qDebug() << "[LoadBalancer] 任务已重新提交:" << taskInfo.id << "(重试" << taskInfo.retryCount << "次)";
    
    return true;
}

LoadBalancer::TaskInfo LoadBalancer::getTaskInfo(const QString& taskId) const
{
    QMutexLocker locker(&m_tasksMutex);
    
    // 检查活动任务
    if (m_activeTasks.contains(taskId)) {
        return m_activeTasks[taskId];
    }
    
    // 检查已完成任务
    if (m_completedTasks.contains(taskId)) {
        return m_completedTasks[taskId];
    }
    
    // 检查队列中的任务
    for (const auto& task : m_taskQueue) {
        if (task.id == taskId) {
            return task;
        }
    }
    
    return TaskInfo();
}

QList<LoadBalancer::TaskInfo> LoadBalancer::getQueuedTasks() const
{
    QMutexLocker locker(&m_tasksMutex);
    return m_taskQueue.toList();
}

QList<LoadBalancer::TaskInfo> LoadBalancer::getActiveTasks() const
{
    QMutexLocker locker(&m_tasksMutex);
    return m_activeTasks.values();
}

QList<LoadBalancer::TaskInfo> LoadBalancer::getCompletedTasks(int limit) const
{
    QMutexLocker locker(&m_tasksMutex);
    
    auto tasks = m_completedTasks.values();
    
    // 按完成时间排序
    std::sort(tasks.begin(), tasks.end(), [](const TaskInfo& a, const TaskInfo& b) {
        return a.completedAt > b.completedAt;
    });
    
    if (limit > 0 && tasks.size() > limit) {
        tasks = tasks.mid(0, limit);
    }
    
    return tasks;
}

void LoadBalancer::startBalancing()
{
    if (m_isRunning) {
        qDebug() << "[LoadBalancer] 负载均衡已在运行中";
        return;
    }
    
    m_isRunning = true;
    
    // 启动定时器
    m_balancingTimer->start(m_balancingInterval);
    m_monitoringTimer->start(m_monitoringInterval);
    m_metricsTimer->start(m_metricsInterval);
    m_cleanupTimer->start(m_cleanupInterval);
    
    qDebug() << "[LoadBalancer] 负载均衡已启动";
}

void LoadBalancer::stopBalancing()
{
    if (!m_isRunning) {
        return;
    }
    
    m_isRunning = false;
    
    // 停止定时器
    m_balancingTimer->stop();
    m_monitoringTimer->stop();
    m_metricsTimer->stop();
    m_cleanupTimer->stop();
    
    // 等待所有任务完成
    m_threadPool->waitForDone(5000);
    
    qDebug() << "[LoadBalancer] 负载均衡已停止";
}

bool LoadBalancer::isBalancing() const
{
    return m_isRunning;
}

void LoadBalancer::updateResourceUsage(ResourceType type, double usage)
{
    updateResourceMetrics(type, usage, true);
    
    // 检查阈值
    if (usage > m_resourceThreshold) {
        emit resourceThresholdExceeded(type, usage);
    }
}

LoadBalancer::ResourceMetrics LoadBalancer::getResourceMetrics(ResourceType type) const
{
    QMutexLocker locker(&m_resourceMutex);
    
    if (m_resourceMetrics.contains(type)) {
        return m_resourceMetrics[type];
    }
    
    return ResourceMetrics();
}

QHash<LoadBalancer::ResourceType, LoadBalancer::ResourceMetrics> LoadBalancer::getAllResourceMetrics() const
{
    QMutexLocker locker(&m_resourceMutex);
    return m_resourceMetrics;
}

LoadBalancer::BalancingMetrics LoadBalancer::getBalancingMetrics() const
{
    QMutexLocker locker(&m_tasksMutex);
    
    BalancingMetrics metrics = m_metrics;
    metrics.lastUpdated = QDateTime::currentDateTime();
    
    // 计算实时指标
    if (metrics.completedTasks > 0) {
        double totalTime = m_startTime.msecsTo(QDateTime::currentDateTime()) / 1000.0;
        metrics.throughput = metrics.completedTasks / qMax(1.0, totalTime);
        metrics.efficiency = calculateSystemEfficiency();
    }
    
    return metrics;
}

QJsonObject LoadBalancer::getPerformanceReport() const
{
    QJsonObject report;
    
    auto metrics = getBalancingMetrics();
    
    // 基本指标
    report["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    report["total_tasks"] = metrics.totalTasks;
    report["completed_tasks"] = metrics.completedTasks;
    report["failed_tasks"] = metrics.failedTasks;
    report["queued_tasks"] = metrics.queuedTasks;
    report["success_rate"] = metrics.totalTasks > 0 ? 
        static_cast<double>(metrics.completedTasks) / metrics.totalTasks : 0.0;
    report["throughput"] = metrics.throughput;
    report["efficiency"] = metrics.efficiency;
    report["average_wait_time"] = metrics.averageWaitTime;
    report["average_execution_time"] = metrics.averageExecutionTime;
    
    // 工作线程统计
    QJsonObject workerStats;
    QMutexLocker workerLocker(&m_workersMutex);
    
    int totalWorkers = m_workers.size();
    int busyWorkers = 0;
    double avgEfficiency = 0.0;
    
    for (const auto& worker : m_workers) {
        if (worker.busy) busyWorkers++;
        avgEfficiency += worker.efficiency;
    }
    
    if (totalWorkers > 0) {
        avgEfficiency /= totalWorkers;
    }
    
    workerStats["total_workers"] = totalWorkers;
    workerStats["busy_workers"] = busyWorkers;
    workerStats["utilization_rate"] = totalWorkers > 0 ? 
        static_cast<double>(busyWorkers) / totalWorkers : 0.0;
    workerStats["average_efficiency"] = avgEfficiency;
    
    report["worker_statistics"] = workerStats;
    
    // 资源使用情况
    QJsonObject resourceStats;
    QMutexLocker resourceLocker(&m_resourceMutex);
    
    for (auto it = m_resourceMetrics.begin(); it != m_resourceMetrics.end(); ++it) {
        QJsonObject resObj;
        const auto& metrics = it.value();
        
        resObj["current_usage"] = metrics.currentUsage;
        resObj["average_usage"] = metrics.averageUsage;
        resObj["peak_usage"] = metrics.peakUsage;
        resObj["utilization_rate"] = metrics.utilizationRate;
        
        QString resourceName;
        switch (it.key()) {
        case ResourceType::CPU: resourceName = "cpu"; break;
        case ResourceType::Memory: resourceName = "memory"; break;
        case ResourceType::IO: resourceName = "io"; break;
        case ResourceType::Network: resourceName = "network"; break;
        case ResourceType::Database: resourceName = "database"; break;
        }
        
        resourceStats[resourceName] = resObj;
    }
    
    report["resource_statistics"] = resourceStats;
    
    return report;
}

QJsonObject LoadBalancer::getWorkerStatistics() const
{
    QJsonObject stats;
    QJsonArray workers;
    
    QMutexLocker locker(&m_workersMutex);
    
    for (const auto& worker : m_workers) {
        QJsonObject workerObj;
        workerObj["id"] = worker.id;
        workerObj["busy"] = worker.busy;
        workerObj["cpu_load"] = worker.cpuLoad;
        workerObj["memory_usage"] = worker.memoryUsage;
        workerObj["active_tasks"] = worker.activeTasks;
        workerObj["completed_tasks"] = worker.completedTasks;
        workerObj["failed_tasks"] = worker.failedTasks;
        workerObj["efficiency"] = worker.efficiency;
        workerObj["enabled"] = worker.enabled;
        workerObj["last_task_completed"] = worker.lastTaskCompleted.toString(Qt::ISODate);
        
        workers.append(workerObj);
    }
    
    stats["workers"] = workers;
    stats["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    return stats;
}

QStringList LoadBalancer::getOptimizationSuggestions() const
{
    QStringList suggestions;
    
    auto metrics = getBalancingMetrics();
    
    // 基于性能指标生成建议
    if (metrics.efficiency < 0.7) {
        suggestions.append("系统效率较低，建议优化任务分配策略");
    }
    
    if (metrics.averageWaitTime > 5000.0) {
        suggestions.append("任务等待时间过长，建议增加工作线程数量");
    }
    
    if (metrics.failedTasks > metrics.completedTasks * 0.1) {
        suggestions.append("任务失败率较高，建议检查任务执行环境");
    }
    
    QMutexLocker workerLocker(&m_workersMutex);
    
    int busyWorkers = 0;
    for (const auto& worker : m_workers) {
        if (worker.busy) busyWorkers++;
    }
    
    double utilizationRate = m_workers.size() > 0 ? 
        static_cast<double>(busyWorkers) / m_workers.size() : 0.0;
    
    if (utilizationRate > 0.9) {
        suggestions.append("工作线程利用率过高，建议增加工作线程数量");
    } else if (utilizationRate < 0.3) {
        suggestions.append("工作线程利用率较低，建议减少工作线程数量或优化任务分配");
    }
    
    // 基于资源使用情况生成建议
    QMutexLocker resourceLocker(&m_resourceMutex);
    
    for (auto it = m_resourceMetrics.begin(); it != m_resourceMetrics.end(); ++it) {
        const auto& resMetrics = it.value();
        
        if (resMetrics.utilizationRate > 0.9) {
            QString resourceName;
            switch (it.key()) {
            case ResourceType::CPU: resourceName = "CPU"; break;
            case ResourceType::Memory: resourceName = "内存"; break;
            case ResourceType::IO: resourceName = "IO"; break;
            case ResourceType::Network: resourceName = "网络"; break;
            case ResourceType::Database: resourceName = "数据库"; break;
            }
            
            suggestions.append(QString("%1资源使用率过高，建议优化相关任务").arg(resourceName));
        }
    }
    
    if (suggestions.isEmpty()) {
        suggestions.append("当前负载均衡运行良好，无需特殊优化");
    }
    
    return suggestions;
}

void LoadBalancer::applyOptimizationRecommendations(const QJsonArray& recommendations)
{
    int appliedCount = 0;
    
    for (const auto& recValue : recommendations) {
        QJsonObject rec = recValue.toObject();
        
        QString type = rec["type"].toString();
        
        if (type == "strategy") {
            int strategyValue = rec["value"].toInt();
            if (strategyValue >= 0 && strategyValue <= 4) {
                setBalancingStrategy(static_cast<BalancingStrategy>(strategyValue));
                appliedCount++;
            }
        } else if (type == "workers") {
            int workerCount = rec["value"].toInt();
            if (workerCount > 0 && workerCount <= 32) {
                setMaxWorkers(workerCount);
                appliedCount++;
            }
        } else if (type == "timeout") {
            int timeout = rec["value"].toInt();
            if (timeout >= 1000) {
                setTaskTimeout(timeout);
                appliedCount++;
            }
        }
    }
    
    qDebug() << "[LoadBalancer] 应用优化建议完成:" << appliedCount << "/" << recommendations.size();
}

bool LoadBalancer::exportConfiguration(const QString& filePath) const
{
    QJsonObject config;
    
    // 基本配置
    config["strategy"] = static_cast<int>(m_strategy);
    config["max_workers"] = m_maxWorkers;
    config["task_timeout"] = m_taskTimeout;
    config["balancing_interval"] = m_balancingInterval;
    config["monitoring_interval"] = m_monitoringInterval;
    config["metrics_interval"] = m_metricsInterval;
    config["cleanup_interval"] = m_cleanupInterval;
    config["max_queue_size"] = m_maxQueueSize;
    config["max_retries"] = m_maxRetries;
    config["resource_threshold"] = m_resourceThreshold;
    
    // 工作线程配置
    QJsonArray workers;
    QMutexLocker workerLocker(&m_workersMutex);
    
    for (const auto& worker : m_workers) {
        QJsonObject workerObj;
        workerObj["id"] = worker.id;
        workerObj["enabled"] = worker.enabled;
        
        QJsonObject capabilities;
        for (auto it = worker.capabilities.begin(); it != worker.capabilities.end(); ++it) {
            capabilities[QString::number(static_cast<int>(it.key()))] = it.value();
        }
        workerObj["capabilities"] = capabilities;
        
        workers.append(workerObj);
    }
    
    config["workers"] = workers;
    config["exported_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    QJsonDocument doc(config);
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "[LoadBalancer] 无法导出配置到" << filePath;
        return false;
    }
    
    file.write(doc.toJson());
    file.close();
    
    qDebug() << "[LoadBalancer] 配置已导出到" << filePath;
    return true;
}

bool LoadBalancer::importConfiguration(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[LoadBalancer] 无法导入配置从" << filePath;
        return false;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "[LoadBalancer] 配置文件格式错误";
        return false;
    }
    
    QJsonObject config = doc.object();
    
    // 导入基本配置
    if (config.contains("strategy")) {
        setBalancingStrategy(static_cast<BalancingStrategy>(config["strategy"].toInt()));
    }
    if (config.contains("max_workers")) {
        setMaxWorkers(config["max_workers"].toInt());
    }
    if (config.contains("task_timeout")) {
        setTaskTimeout(config["task_timeout"].toInt());
    }
    if (config.contains("balancing_interval")) {
        m_balancingInterval = config["balancing_interval"].toInt();
    }
    if (config.contains("monitoring_interval")) {
        m_monitoringInterval = config["monitoring_interval"].toInt();
    }
    if (config.contains("metrics_interval")) {
        m_metricsInterval = config["metrics_interval"].toInt();
    }
    if (config.contains("cleanup_interval")) {
        m_cleanupInterval = config["cleanup_interval"].toInt();
    }
    if (config.contains("max_queue_size")) {
        m_maxQueueSize = config["max_queue_size"].toInt();
    }
    if (config.contains("max_retries")) {
        m_maxRetries = config["max_retries"].toInt();
    }
    if (config.contains("resource_threshold")) {
        m_resourceThreshold = config["resource_threshold"].toDouble();
    }
    
    qDebug() << "[LoadBalancer] 配置已导入从" << filePath;
    return true;
}

void LoadBalancer::resetToDefaults()
{
    stopBalancing();
    
    // 重置配置
    m_strategy = BalancingStrategy::Adaptive;
    m_maxWorkers = QThread::idealThreadCount();
    m_taskTimeout = 30000;
    m_balancingInterval = 5000;
    m_monitoringInterval = 2000;
    m_metricsInterval = 10000;
    m_cleanupInterval = 60000;
    m_maxQueueSize = 1000;
    m_maxRetries = 3;
    m_resourceThreshold = 80.0;
    
    // 清空队列和任务
    QMutexLocker taskLocker(&m_tasksMutex);
    m_taskQueue.clear();
    m_activeTasks.clear();
    m_completedTasks.clear();
    
    // 重置指标
    m_metrics = BalancingMetrics();
    m_taskIdCounter = 0;
    
    qDebug() << "[LoadBalancer] 已重置为默认配置";
}

// 槽函数实现
void LoadBalancer::optimizeBalancing()
{
    if (!m_isRunning) {
        return;
    }
    
    // 优化工作线程分配
    optimizeWorkerAllocation();
    
    // 调整负载均衡策略
    adjustBalancingStrategy();
    
    // 预测资源需求
    predictResourceNeeds();
    
    qDebug() << "[LoadBalancer] 负载均衡优化完成";
}

void LoadBalancer::rebalanceTasks()
{
    // 重新分配任务到最优工作线程
    QMutexLocker taskLocker(&m_tasksMutex);
    QMutexLocker workerLocker(&m_workersMutex);
    
    // 简化的重新平衡逻辑
    for (auto it = m_activeTasks.begin(); it != m_activeTasks.end(); ++it) {
        auto& task = it.value();
        
        // 检查当前工作线程是否仍然是最优选择
        WorkerInfo* optimalWorker = selectWorker(task);
        
        if (optimalWorker && !optimalWorker->busy) {
            // 可以考虑迁移任务（简化实现中暂不支持）
        }
    }
    
    qDebug() << "[LoadBalancer] 任务重新平衡完成";
}

void LoadBalancer::updateWorkerMetrics()
{
    QMutexLocker locker(&m_workersMutex);
    
    for (auto& worker : m_workers) {
        // 更新工作线程效率
        if (worker.completedTasks > 0) {
            double successRate = static_cast<double>(worker.completedTasks) / 
                               (worker.completedTasks + worker.failedTasks);
            worker.efficiency = successRate;
        }
        
        // 更新负载信息（简化实现）
        if (worker.busy) {
            worker.cpuLoad = qMin(100.0, worker.cpuLoad + 5.0);
        } else {
            worker.cpuLoad = qMax(0.0, worker.cpuLoad - 2.0);
        }
    }
}

void LoadBalancer::cleanupCompletedTasks()
{
    QMutexLocker locker(&m_tasksMutex);
    
    // 清理超过一定时间的已完成任务
    QDateTime cutoffTime = QDateTime::currentDateTime().addSecs(-3600); // 1小时前
    
    auto it = m_completedTasks.begin();
    while (it != m_completedTasks.end()) {
        if (it.value().completedAt < cutoffTime) {
            it = m_completedTasks.erase(it);
        } else {
            ++it;
        }
    }
    
    qDebug() << "[LoadBalancer] 已清理过期的已完成任务";
}

void LoadBalancer::handleResourceAlert(ResourceType type, double usage)
{
    QString resourceName;
    switch (type) {
    case ResourceType::CPU: resourceName = "CPU"; break;
    case ResourceType::Memory: resourceName = "内存"; break;
    case ResourceType::IO: resourceName = "IO"; break;
    case ResourceType::Network: resourceName = "网络"; break;
    case ResourceType::Database: resourceName = "数据库"; break;
    }
    
    QString message = QString("%1资源使用率过高: %2%").arg(resourceName).arg(usage, 0, 'f', 1);
    
    int severity = 1; // 警告
    if (usage > 95.0) {
        severity = 2; // 严重
    }
    
    emit performanceAlert(message, severity);
    
    qWarning() << "[LoadBalancer]" << message;
}

// 私有槽函数实现
void LoadBalancer::processTaskQueue()
{
    QMutexLocker taskLocker(&m_tasksMutex);
    
    if (m_taskQueue.isEmpty()) {
        return;
    }
    
    // 处理队列中的任务
    while (!m_taskQueue.isEmpty()) {
        TaskInfo task = m_taskQueue.dequeue();
        m_metrics.queuedTasks--;
        
        // 选择工作线程
        WorkerInfo* worker = selectWorker(task);
        
        if (worker) {
            executeTask(task, worker);
        } else {
            // 没有可用工作线程，重新放回队列
            m_taskQueue.prepend(task);
            m_metrics.queuedTasks++;
            break;
        }
    }
}

void LoadBalancer::monitorWorkers()
{
    updateWorkerMetrics();
    
    // 检查工作线程健康状态
    QMutexLocker locker(&m_workersMutex);
    
    for (const auto& worker : m_workers) {
        if (worker.enabled && worker.efficiency < 0.5) {
            QString message = QString("工作线程 %1 效率较低: %2")
                            .arg(worker.id).arg(worker.efficiency, 0, 'f', 2);
            emit performanceAlert(message, 1);
        }
    }
}

void LoadBalancer::updateMetrics()
{
    QMutexLocker locker(&m_tasksMutex);
    
    // 更新平均等待时间和执行时间
    if (m_completedTasks.size() > 0) {
        double totalWaitTime = 0.0;
        double totalExecutionTime = 0.0;
        
        for (const auto& task : m_completedTasks) {
            if (task.completed) {
                totalWaitTime += task.submittedAt.msecsTo(task.startedAt);
                totalExecutionTime += task.actualDuration;
            }
        }
        
        m_metrics.averageWaitTime = totalWaitTime / m_completedTasks.size();
        m_metrics.averageExecutionTime = totalExecutionTime / m_completedTasks.size();
    }
    
    m_metrics.lastUpdated = QDateTime::currentDateTime();
}

void LoadBalancer::handleTaskTimeout()
{
    QMutexLocker locker(&m_tasksMutex);
    
    QDateTime timeoutThreshold = QDateTime::currentDateTime().addMSecs(-m_taskTimeout);
    
    // 检查超时的活动任务
    auto it = m_activeTasks.begin();
    while (it != m_activeTasks.end()) {
        auto& task = it.value();
        
        if (task.startedAt < timeoutThreshold) {
            // 任务超时
            task.failed = true;
            task.completed = true;
            task.completedAt = QDateTime::currentDateTime();
            task.errorMessage = "任务执行超时";
            
            m_completedTasks[task.id] = task;
            m_metrics.failedTasks++;
            
            emit taskFailed(task.id, task.errorMessage);
            
            it = m_activeTasks.erase(it);
        } else {
            ++it;
        }
    }
}

void LoadBalancer::analyzePerformance()
{
    // 生成性能洞察
    generatePerformanceInsights();
    
    // 如果有智能分析器，提供数据
    if (m_analyzer) {
        // 简化的性能数据提供
        auto metrics = getBalancingMetrics();
        
        // 可以将指标转换为分析器需要的格式
        // m_analyzer->addDataPoint(...);
    }
}

// 私有方法实现
QString LoadBalancer::generateTaskId()
{
    return QString("task_%1_%2")
           .arg(QDateTime::currentMSecsSinceEpoch())
           .arg(++m_taskIdCounter);
}

LoadBalancer::WorkerInfo* LoadBalancer::selectWorker(const TaskInfo& task)
{
    QMutexLocker locker(&m_workersMutex);
    
    switch (m_strategy) {
    case BalancingStrategy::RoundRobin:
        return selectWorkerRoundRobin();
    case BalancingStrategy::LeastLoaded:
        return selectWorkerLeastLoaded();
    case BalancingStrategy::WeightedRoundRobin:
        return selectWorkerWeighted();
    case BalancingStrategy::ResourceBased:
        return selectWorkerResourceBased(task);
    case BalancingStrategy::Adaptive:
        return selectWorkerAdaptive(task);
    default:
        return selectWorkerLeastLoaded();
    }
}

LoadBalancer::WorkerInfo* LoadBalancer::selectWorkerRoundRobin()
{
    static int lastWorkerIndex = 0;
    
    auto workers = m_workers.values();
    
    for (int i = 0; i < workers.size(); ++i) {
        int index = (lastWorkerIndex + i) % workers.size();
        auto& worker = workers[index];
        
        if (worker.enabled && !worker.busy) {
            lastWorkerIndex = (index + 1) % workers.size();
            return &m_workers[worker.id];
        }
    }
    
    return nullptr;
}

LoadBalancer::WorkerInfo* LoadBalancer::selectWorkerLeastLoaded()
{
    WorkerInfo* bestWorker = nullptr;
    double minLoad = std::numeric_limits<double>::max();
    
    for (auto& worker : m_workers) {
        if (worker.enabled && !worker.busy) {
            double load = worker.cpuLoad + worker.memoryUsage * 0.5 + worker.activeTasks * 10.0;
            
            if (load < minLoad) {
                minLoad = load;
                bestWorker = &worker;
            }
        }
    }
    
    return bestWorker;
}

LoadBalancer::WorkerInfo* LoadBalancer::selectWorkerWeighted()
{
    WorkerInfo* bestWorker = nullptr;
    double bestScore = -1.0;
    
    for (auto& worker : m_workers) {
        if (worker.enabled && !worker.busy) {
            double score = worker.efficiency * (1.0 - worker.cpuLoad / 100.0);
            
            if (score > bestScore) {
                bestScore = score;
                bestWorker = &worker;
            }
        }
    }
    
    return bestWorker;
}

LoadBalancer::WorkerInfo* LoadBalancer::selectWorkerResourceBased(const TaskInfo& task)
{
    WorkerInfo* bestWorker = nullptr;
    double bestScore = -1.0;
    
    for (auto& worker : m_workers) {
        if (worker.enabled && !worker.busy) {
            double score = calculateWorkerScore(worker, task);
            
            if (score > bestScore) {
                bestScore = score;
                bestWorker = &worker;
            }
        }
    }
    
    return bestWorker;
}

LoadBalancer::WorkerInfo* LoadBalancer::selectWorkerAdaptive(const TaskInfo& task)
{
    // 自适应策略结合多种因素
    WorkerInfo* bestWorker = nullptr;
    double bestScore = -1.0;
    
    for (auto& worker : m_workers) {
        if (worker.enabled && !worker.busy) {
            // 综合评分
            double resourceScore = calculateWorkerScore(worker, task);
            double efficiencyScore = worker.efficiency;
            double loadScore = 1.0 - (worker.cpuLoad / 100.0);
            
            double totalScore = resourceScore * 0.4 + efficiencyScore * 0.3 + loadScore * 0.3;
            
            if (totalScore > bestScore) {
                bestScore = totalScore;
                bestWorker = &worker;
            }
        }
    }
    
    return bestWorker;
}

void LoadBalancer::executeTask(const TaskInfo& task, WorkerInfo* worker)
{
    if (!worker) {
        return;
    }
    
    // 更新任务状态
    TaskInfo activeTask = task;
    activeTask.startedAt = QDateTime::currentDateTime();
    
    QMutexLocker taskLocker(&m_tasksMutex);
    m_activeTasks[activeTask.id] = activeTask;
    
    // 更新工作线程状态
    QMutexLocker workerLocker(&m_workersMutex);
    worker->busy = true;
    worker->activeTasks++;
    
    emit taskStarted(activeTask.id, worker->id);
    
    // 创建任务执行器
    TaskRunner* runner = new TaskRunner(activeTask, this);
    
    // 提交到线程池
    m_threadPool->start(runner);
    
    qDebug() << "[LoadBalancer] 任务已分配给工作线程:" << activeTask.id << "->" << worker->id;
}

void LoadBalancer::updateWorkerLoad(const QString& workerId, const QHash<ResourceType, double>& load)
{
    QMutexLocker locker(&m_workersMutex);
    
    if (m_workers.contains(workerId)) {
        m_workers[workerId].currentLoad = load;
    }
}

void LoadBalancer::updateTaskMetrics(const TaskInfo& task)
{
    QMutexLocker locker(&m_tasksMutex);
    
    if (task.completed) {
        if (task.failed) {
            m_metrics.failedTasks++;
        } else {
            m_metrics.completedTasks++;
        }
    }
}

void LoadBalancer::updateResourceMetrics(ResourceType type, double usage, bool updateHistory)
{
    QMutexLocker locker(&m_resourceMutex);
    
    if (!m_resourceMetrics.contains(type)) {
        ResourceMetrics metrics;
        metrics.type = type;
        metrics.totalCapacity = 100.0;
        metrics.currentUsage = 0.0;
        metrics.averageUsage = 0.0;
        metrics.peakUsage = 0.0;
        metrics.utilizationRate = 0.0;
        metrics.lastUpdated = QDateTime::currentDateTime();
        
        m_resourceMetrics[type] = metrics;
    }
    
    auto& metrics = m_resourceMetrics[type];
    metrics.currentUsage = usage;
    metrics.peakUsage = qMax(metrics.peakUsage, usage);
    metrics.utilizationRate = usage / metrics.totalCapacity;
    metrics.lastUpdated = QDateTime::currentDateTime();
    
    if (updateHistory) {
        metrics.usageHistory.append(usage);
        
        // 限制历史记录大小
        while (metrics.usageHistory.size() > 100) {
            metrics.usageHistory.removeFirst();
        }
        
        // 计算平均使用率
        if (!metrics.usageHistory.isEmpty()) {
            double sum = 0.0;
            for (double value : metrics.usageHistory) {
                sum += value;
            }
            metrics.averageUsage = sum / metrics.usageHistory.size();
        }
    }
}

double LoadBalancer::calculateWorkerScore(const WorkerInfo& worker, const TaskInfo& task) const
{
    double score = 0.0;
    
    // 基于工作线程能力和任务需求计算匹配度
    if (worker.capabilities.contains(task.primaryResource)) {
        double capability = worker.capabilities[task.primaryResource];
        double requirement = task.resourceRequirements.value(task.primaryResource, 1.0);
        
        if (capability >= requirement) {
            score += 0.5; // 基础匹配分
            score += (capability - requirement) * 0.1; // 超出需求的奖励分
        } else {
            score -= (requirement - capability) * 0.2; // 不足的惩罚分
        }
    }
    
    // 考虑工作线程效率
    score += worker.efficiency * 0.3;
    
    // 考虑当前负载
    score -= worker.cpuLoad * 0.01;
    score -= worker.activeTasks * 0.05;
    
    return qMax(0.0, score);
}

double LoadBalancer::calculateResourceUtilization(ResourceType type) const
{
    QMutexLocker locker(&m_resourceMutex);
    
    if (m_resourceMetrics.contains(type)) {
        return m_resourceMetrics[type].utilizationRate;
    }
    
    return 0.0;
}

double LoadBalancer::calculateSystemEfficiency() const
{
    if (m_metrics.totalTasks == 0) {
        return 1.0;
    }
    
    double successRate = static_cast<double>(m_metrics.completedTasks) / m_metrics.totalTasks;
    double resourceEfficiency = 0.0;
    
    QMutexLocker locker(&m_resourceMutex);
    
    if (!m_resourceMetrics.isEmpty()) {
        double totalUtilization = 0.0;
        for (const auto& metrics : m_resourceMetrics) {
            totalUtilization += metrics.utilizationRate;
        }
        resourceEfficiency = totalUtilization / m_resourceMetrics.size() / 100.0;
    }
    
    return (successRate * 0.7 + resourceEfficiency * 0.3);
}

void LoadBalancer::optimizeWorkerAllocation()
{
    // 简化的工作线程分配优化
    QMutexLocker locker(&m_workersMutex);
    
    // 分析工作线程性能
    for (auto& worker : m_workers) {
        if (worker.efficiency < 0.5 && worker.enabled) {
            // 暂时禁用效率低的工作线程
            worker.enabled = false;
            qDebug() << "[LoadBalancer] 暂时禁用低效工作线程:" << worker.id;
        } else if (worker.efficiency > 0.8 && !worker.enabled) {
            // 重新启用高效工作线程
            worker.enabled = true;
            qDebug() << "[LoadBalancer] 重新启用高效工作线程:" << worker.id;
        }
    }
}

void LoadBalancer::adjustBalancingStrategy()
{
    // 根据当前性能自动调整策略
    auto metrics = getBalancingMetrics();
    
    if (metrics.efficiency < 0.6) {
        // 效率低，尝试更智能的策略
        if (m_strategy != BalancingStrategy::Adaptive) {
            setBalancingStrategy(BalancingStrategy::Adaptive);
            emit balancingOptimized("切换到自适应策略", 0.1);
        }
    } else if (metrics.efficiency > 0.9) {
        // 效率高，可以使用简单策略
        if (m_strategy == BalancingStrategy::Adaptive) {
            setBalancingStrategy(BalancingStrategy::LeastLoaded);
            emit balancingOptimized("切换到最少负载策略", 0.05);
        }
    }
}

void LoadBalancer::predictResourceNeeds()
{
    // 简化的资源需求预测
    QMutexLocker resourceLocker(&m_resourceMutex);
    
    for (auto& metrics : m_resourceMetrics) {
        if (metrics.usageHistory.size() >= 10) {
            // 计算趋势
            double recentAvg = 0.0;
            int recentCount = qMin(5, metrics.usageHistory.size());
            
            for (int i = metrics.usageHistory.size() - recentCount; i < metrics.usageHistory.size(); ++i) {
                recentAvg += metrics.usageHistory[i];
            }
            recentAvg /= recentCount;
            
            if (recentAvg > metrics.averageUsage * 1.2) {
                // 使用率上升趋势
                QString resourceName;
                switch (metrics.type) {
                case ResourceType::CPU: resourceName = "CPU"; break;
                case ResourceType::Memory: resourceName = "内存"; break;
                case ResourceType::IO: resourceName = "IO"; break;
                case ResourceType::Network: resourceName = "网络"; break;
                case ResourceType::Database: resourceName = "数据库"; break;
                }
                
                QString message = QString("预测%1资源需求将增加").arg(resourceName);
                emit performanceAlert(message, 0);
            }
        }
    }
}

void LoadBalancer::generatePerformanceInsights()
{
    // 生成性能洞察报告
    auto metrics = getBalancingMetrics();
    
    QStringList insights;
    
    if (metrics.throughput > 0) {
        insights.append(QString("当前吞吐量: %1 任务/秒").arg(metrics.throughput, 0, 'f', 2));
    }
    
    if (metrics.efficiency < 0.7) {
        insights.append("系统效率偏低，建议优化任务分配");
    }
    
    if (metrics.averageWaitTime > 5000) {
        insights.append("任务等待时间较长，建议增加工作线程");
    }
    
    for (const QString& insight : insights) {
        qDebug() << "[LoadBalancer] 性能洞察:" << insight;
    }
}

void LoadBalancer::saveBalancingState()
{
    QString statePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/load_balancer_state.json";
    
    QDir().mkpath(QFileInfo(statePath).absolutePath());
    
    exportConfiguration(statePath);
}

void LoadBalancer::loadBalancingState()
{
    QString statePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/load_balancer_state.json";
    
    if (QFile::exists(statePath)) {
        importConfiguration(statePath);
    }
}

void LoadBalancer::initializeDefaultWorkers()
{
    // 创建默认工作线程
    int workerCount = qMin(m_maxWorkers, QThread::idealThreadCount());
    
    for (int i = 0; i < workerCount; ++i) {
        QString workerId = QString("worker_%1").arg(i + 1);
        
        QHash<ResourceType, double> capabilities;
        capabilities[ResourceType::CPU] = 1.0;
        capabilities[ResourceType::Memory] = 1.0;
        capabilities[ResourceType::IO] = 1.0;
        capabilities[ResourceType::Network] = 1.0;
        capabilities[ResourceType::Database] = 1.0;
        
        addWorker(workerId, capabilities);
    }
    
    qDebug() << "[LoadBalancer] 已初始化" << workerCount << "个默认工作线程";
}

void LoadBalancer::setupResourceMonitoring()
{
    // 初始化资源监控
    QList<ResourceType> resourceTypes = {
        ResourceType::CPU,
        ResourceType::Memory,
        ResourceType::IO,
        ResourceType::Network,
        ResourceType::Database
    };
    
    for (ResourceType type : resourceTypes) {
        updateResourceMetrics(type, 0.0, false);
    }
    
    qDebug() << "[LoadBalancer] 资源监控已设置";
}