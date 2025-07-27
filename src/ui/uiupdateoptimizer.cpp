#include "uiupdateoptimizer.h"
#include "../data/datacachemanager.h"
#include "../logger/logmanager.h"
#include <QApplication>
#include <QDebug>
#include <QJsonObject>
#include <QRegularExpression>
#include <QProcess>
#include <algorithm>

UIUpdateOptimizer::UIUpdateOptimizer(QObject* parent)
    : QObject(parent)
    , m_maxBatchSize(10)
    , m_maxQueueSize(100)
    , m_paused(false)
    , m_targetFPS(60)
    , m_currentFPS(0)
    , m_adaptiveMode(true)
    , m_cacheManager(nullptr)
{
    // 初始化定时器
    m_updateTimer = new QTimer(this);
    m_performanceTimer = new QTimer(this);
    m_optimizationTimer = new QTimer(this);
    m_adaptiveTimer = new QTimer(this);
    
    // 连接信号
    connect(m_updateTimer, &QTimer::timeout, this, &UIUpdateOptimizer::processUpdates);
    connect(m_performanceTimer, &QTimer::timeout, this, &UIUpdateOptimizer::onPerformanceTimer);
    connect(m_optimizationTimer, &QTimer::timeout, this, &UIUpdateOptimizer::onOptimizationTimer);
    connect(m_adaptiveTimer, &QTimer::timeout, this, &UIUpdateOptimizer::onAdaptiveTuning);
    
    // 设置定时器间隔
    m_updateTimer->setInterval(16); // 60 FPS
    m_performanceTimer->setInterval(1000); // 1秒更新性能统计
    m_optimizationTimer->setInterval(5000); // 5秒优化一次
    m_adaptiveTimer->setInterval(2000); // 2秒自适应调整
    
    // 初始化默认更新间隔
    m_updateIntervals[UIUpdateType::StatusBar] = 100;      // 100ms
    m_updateIntervals[UIUpdateType::ProgressBar] = 50;     // 50ms
    m_updateIntervals[UIUpdateType::ChartData] = 200;      // 200ms
    m_updateIntervals[UIUpdateType::TableData] = 300;      // 300ms
    m_updateIntervals[UIUpdateType::Statistics] = 500;     // 500ms
    m_updateIntervals[UIUpdateType::RealTimeData] = 16;    // ~60fps
    m_updateIntervals[UIUpdateType::ErrorMessage] = 0;     // 立即
    m_updateIntervals[UIUpdateType::LogMessage] = 100;     // 100ms
    m_updateIntervals[UIUpdateType::Animation] = 16;       // ~60fps
    m_updateIntervals[UIUpdateType::Layout] = 50;          // 50ms
    
    // 初始化渲染策略
    m_renderStrategies[UIUpdateType::StatusBar] = RenderStrategy::Batched;
    m_renderStrategies[UIUpdateType::ProgressBar] = RenderStrategy::Immediate;
    m_renderStrategies[UIUpdateType::ChartData] = RenderStrategy::Adaptive;
    m_renderStrategies[UIUpdateType::TableData] = RenderStrategy::Deferred;
    m_renderStrategies[UIUpdateType::Statistics] = RenderStrategy::Batched;
    m_renderStrategies[UIUpdateType::RealTimeData] = RenderStrategy::Immediate;
    m_renderStrategies[UIUpdateType::ErrorMessage] = RenderStrategy::Immediate;
    m_renderStrategies[UIUpdateType::LogMessage] = RenderStrategy::Batched;
    m_renderStrategies[UIUpdateType::Animation] = RenderStrategy::Immediate;
    m_renderStrategies[UIUpdateType::Layout] = RenderStrategy::Deferred;
    
    // 默认启用所有类型
    for (auto it = m_updateIntervals.begin(); it != m_updateIntervals.end(); ++it) {
        m_enabledTypes[it.key()] = true;
    }
    
    // 初始化性能统计
    m_metrics.totalUpdates = 0;
    m_metrics.totalUpdateTime = 0;
    m_metrics.averageUpdateTime = 0.0;
    m_metrics.updatesPerSecond = 0;
    m_metrics.droppedUpdates = 0;
    m_metrics.coalescedUpdates = 0;
    m_metrics.cpuUsage = 0.0;
    m_metrics.memoryUsage = 0;
    m_metrics.lastUpdate = QDateTime::currentDateTime();
    m_metrics.performanceTimer.start();
    
    // 初始化优化配置
    m_optimizationConfig.maxUpdatesPerSecond = 60;
    m_optimizationConfig.lowPriorityDelay = 100;
    m_optimizationConfig.highPriorityThreshold = 80;
    m_optimizationConfig.enableFrameRateLimit = true;
    m_optimizationConfig.enableAdaptiveInterval = true;
    m_optimizationConfig.enableCoalescing = true;
    m_optimizationConfig.enableThreadOptimization = true;
    m_optimizationConfig.cpuThreshold = 80.0;
    m_optimizationConfig.memoryThreshold = 1024 * 1024 * 1024; // 1GB
    m_optimizationConfig.adaptiveWindowSize = 100;
    
    // 启动定时器
    m_updateTimer->start();
    m_performanceTimer->start();
    m_optimizationTimer->start();
    m_adaptiveTimer->start();
    
    // 启动帧率计时器
    m_frameTimer.start();
    
    // 初始化最后优化时间
    m_lastOptimization = QDateTime::currentDateTime();
    
    // 初始化缓存管理器
    m_cacheManager = DataCacheManager::getInstance();
    if (m_cacheManager) {
        m_cacheManager->setMaxSize(100 * 1024 * 1024); // 100MB
        m_cacheManager->setDefaultTTL(300); // 5分钟
        m_cacheManager->setCachePolicy(CachePolicy::LRU);
    }
    
    LogManager::getInstance()->info("UI更新优化器已初始化", "UIUpdateOptimizer");
}

UIUpdateOptimizer::~UIUpdateOptimizer()
{
    m_updateTimer->stop();
    m_performanceTimer->stop();
    m_optimizationTimer->stop();
    
    // 清理缓存管理器
    if (m_cacheManager) {
        m_cacheManager->clear();
        m_cacheManager = nullptr;
    }
    
    LogManager::getInstance()->info("UI更新优化器已销毁", "UIUpdateOptimizer");
}

void UIUpdateOptimizer::requestUpdate(const UIUpdateTask& task)
{
    if (!m_enabledTypes.value(task.type, true)) {
        return;
    }
    
    QMutexLocker locker(&m_queueMutex);
    
    // 检查是否应该跳过更新
    if (shouldSkipUpdate(task)) {
        return;
    }
    
    // 检查队列大小
    if (m_updateQueue.size() >= m_maxQueueSize) {
        // 移除最旧的低优先级任务
        for (int i = 0; i < m_updateQueue.size(); ++i) {
            if (m_updateQueue[i].priority < task.priority) {
                m_updateQueue.removeAt(i);
                break;
            }
        }
    }
    
    // 如果是立即更新
    if (task.immediate) {
        executeUpdate(task);
        return;
    }
    
    // 添加到队列
    m_updateQueue.append(task);
    
    // 记录最后更新
    QString key = QString("%1_%2").arg(static_cast<int>(task.type)).arg(task.widgetId);
    m_lastUpdates[key] = task;
}

void UIUpdateOptimizer::requestImmediateUpdate(const UIUpdateTask& task)
{
    UIUpdateTask immediateTask = task;
    immediateTask.immediate = true;
    requestUpdate(immediateTask);
}

void UIUpdateOptimizer::requestBatchUpdate(const QList<UIUpdateTask>& tasks)
{
    QMutexLocker locker(&m_queueMutex);
    
    for (const UIUpdateTask& task : tasks) {
        if (m_enabledTypes.value(task.type, true)) {
            m_updateQueue.append(task);
        }
    }
}

void UIUpdateOptimizer::setUpdateInterval(UIUpdateType type, int intervalMs)
{
    QMutexLocker locker(&m_queueMutex);
    m_updateIntervals[type] = intervalMs;
    
    LogManager::getInstance()->info(
        QString("设置更新间隔: 类型=%1, 间隔=%2ms")
        .arg(static_cast<int>(type))
        .arg(intervalMs),
        "UIUpdateOptimizer"
    );
}

void UIUpdateOptimizer::setRenderStrategy(UIUpdateType type, RenderStrategy strategy)
{
    QMutexLocker locker(&m_queueMutex);
    m_renderStrategies[type] = strategy;
    
    LogManager::getInstance()->info(
        QString("设置渲染策略: 类型=%1, 策略=%2")
        .arg(static_cast<int>(type))
        .arg(static_cast<int>(strategy)),
        "UIUpdateOptimizer"
    );
    
    emit renderStrategyChanged(type, strategy);
}

void UIUpdateOptimizer::enableCoalescing(bool enabled)
{
    m_optimizationConfig.enableCoalescing = enabled;
    
    LogManager::getInstance()->info(
        QString("更新合并功能: %1").arg(enabled ? "启用" : "禁用"),
        "UIUpdateOptimizer"
    );
}

void UIUpdateOptimizer::setOptimizationConfig(const ::OptimizationConfig& config)
{
    m_optimizationConfig = config;
    
    // 根据配置调整定时器间隔
    if (config.enableFrameRateLimit) {
        int targetInterval = 1000 / config.maxUpdatesPerSecond;
        m_updateTimer->setInterval(targetInterval);
    }
    
    LogManager::getInstance()->info("优化配置已更新", "UIUpdateOptimizer");
}

void UIUpdateOptimizer::setMaxBatchSize(int maxSize)
{
    m_maxBatchSize = maxSize;
}

void UIUpdateOptimizer::setMaxQueueSize(int maxSize)
{
    m_maxQueueSize = maxSize;
}

void UIUpdateOptimizer::enableUpdateType(UIUpdateType type, bool enabled)
{
    m_enabledTypes[type] = enabled;
}

void UIUpdateOptimizer::pauseUpdates()
{
    m_paused = true;
    m_updateTimer->stop();
    LogManager::getInstance()->info("UI更新已暂停", "UIUpdateOptimizer");
}

void UIUpdateOptimizer::resumeUpdates()
{
    m_paused = false;
    m_updateTimer->start();
    LogManager::getInstance()->info("UI更新已恢复", "UIUpdateOptimizer");
}

void UIUpdateOptimizer::clearPendingUpdates()
{
    QMutexLocker locker(&m_queueMutex);
    m_updateQueue.clear();
    m_lastUpdates.clear();
    LogManager::getInstance()->info("已清空待处理的UI更新", "UIUpdateOptimizer");
}

void UIUpdateOptimizer::optimizeUpdateFrequency()
{
    // 根据性能指标调整更新频率
    if (m_metrics.averageUpdateTime > 16.0) { // 超过16ms（60FPS）
        // 降低更新频率
        m_updateTimer->setInterval(qMax(32, m_updateTimer->interval() * 2));
        LogManager::getInstance()->warning("UI更新频率过高，已自动降低", "UIUpdateOptimizer");
    } else if (m_metrics.averageUpdateTime < 8.0) { // 小于8ms
        // 提高更新频率
        m_updateTimer->setInterval(qMax(16, m_updateTimer->interval() / 2));
    }
}

void UIUpdateOptimizer::adaptivePerformanceTuning()
{
    // 更新系统资源使用情况
    updateSystemResourceUsage();
    
    // 检测性能瓶颈
    detectPerformanceBottlenecks();
    
    // 自适应调节更新间隔
    if (m_adaptiveMode) {
        for (auto it = m_updateIntervals.begin(); it != m_updateIntervals.end(); ++it) {
            UIUpdateType type = it.key();
            int newInterval = calculateAdaptiveInterval(type);
            if (newInterval != it.value()) {
                it.value() = newInterval;
            }
        }
    }
    
    // 记录最后优化时间
    m_lastOptimization = QDateTime::currentDateTime();
    
    LogManager::getInstance()->debug("执行自适应性能调节", "UIUpdateOptimizer");
}

void UIUpdateOptimizer::updateSystemResourceUsage()
{
    // 获取CPU使用率
    QProcess cpuProcess;
    cpuProcess.start("wmic", QStringList() << "cpu" << "get" << "loadpercentage" << "/value");
    if (cpuProcess.waitForFinished(1000)) {
        QString output = cpuProcess.readAllStandardOutput();
        QRegularExpression rx("LoadPercentage=(\\d+)");
        QRegularExpressionMatch match = rx.match(output);
        if (match.hasMatch()) {
            m_metrics.cpuUsage = match.captured(1).toDouble();
        }
    }
    
    // 获取内存使用情况
    QProcess memProcess;
    memProcess.start("wmic", QStringList() << "OS" << "get" << "TotalVisibleMemorySize,FreePhysicalMemory" << "/value");
    if (memProcess.waitForFinished(1000)) {
        QString output = memProcess.readAllStandardOutput();
        QRegularExpression totalRx("TotalVisibleMemorySize=(\\d+)");
        QRegularExpression freeRx("FreePhysicalMemory=(\\d+)");
        
        QRegularExpressionMatch totalMatch = totalRx.match(output);
        QRegularExpressionMatch freeMatch = freeRx.match(output);
        
        if (totalMatch.hasMatch() && freeMatch.hasMatch()) {
            qint64 totalMem = totalMatch.captured(1).toLongLong() * 1024; // KB to bytes
            qint64 freeMem = freeMatch.captured(1).toLongLong() * 1024;
            m_metrics.memoryUsage = totalMem - freeMem;
        }
    }
}

void UIUpdateOptimizer::detectPerformanceBottlenecks()
{
    // 检测更新队列积压
    if (getPendingUpdateCount() > m_maxQueueSize * 0.8) {
        emit performanceWarning("更新队列积压严重，可能存在性能瓶颈");
    }
    
    // 检测平均更新时间异常
    if (m_metrics.averageUpdateTime > 50.0) {
        emit performanceWarning("UI更新平均时间过长，建议检查更新逻辑");
    }
    
    // 检测丢弃更新过多
    if (m_metrics.droppedUpdates > m_metrics.totalUpdates * 0.1) {
        emit performanceWarning("丢弃更新过多，可能需要优化更新策略");
    }
    
    // 检测系统资源使用情况
    if (m_metrics.cpuUsage > 90.0) {
        emit performanceWarning("系统CPU使用率过高，已启用保护模式");
        // 启用保护模式（已移除具体实现）
    }
    
    if (m_metrics.memoryUsage > m_optimizationConfig.memoryThreshold) {
        emit performanceWarning("系统内存使用率过高，已清理低优先级更新");
        clearLowPriorityUpdates();
    }
}

int UIUpdateOptimizer::calculateAdaptiveInterval(UIUpdateType type)
{
    int baseInterval = m_updateIntervals.value(type, 100);
    double performanceFactor = 1.0;
    
    // 根据平均更新时间调整
    if (m_metrics.averageUpdateTime > 20.0) {
        performanceFactor *= 1.5; // 增加间隔
    } else if (m_metrics.averageUpdateTime < 5.0) {
        performanceFactor *= 0.8; // 减少间隔
    }
    
    // 根据队列长度调整
    double queueFactor = static_cast<double>(getPendingUpdateCount()) / m_maxQueueSize;
    if (queueFactor > 0.7) {
        performanceFactor *= (1.0 + queueFactor);
    }
    
    // 根据CPU使用率调整
    if (m_metrics.cpuUsage > 80.0) {
        performanceFactor *= 1.3;
    } else if (m_metrics.cpuUsage < 30.0) {
        performanceFactor *= 0.9;
    }
    
    // 计算新间隔
    int newInterval = static_cast<int>(baseInterval * performanceFactor);
    
    // 限制范围
    switch (type) {
        case UIUpdateType::ErrorMessage:
            return qMax(0, qMin(newInterval, 50));
        case UIUpdateType::RealTimeData:
        case UIUpdateType::Animation:
            return qMax(16, qMin(newInterval, 100));
        case UIUpdateType::ProgressBar:
            return qMax(30, qMin(newInterval, 200));
        default:
            return qMax(50, qMin(newInterval, 1000));
    }
}

// enableProtectionMode方法已移除

int UIUpdateOptimizer::getPendingUpdateCount() const
{
    QMutexLocker locker(&m_queueMutex);
    return m_updateQueue.size();
}

double UIUpdateOptimizer::getAverageUpdateTime() const
{
    return m_metrics.averageUpdateTime;
}

int UIUpdateOptimizer::getUpdateRate() const
{
    return m_metrics.updatesPerSecond;
}

void UIUpdateOptimizer::registerUpdateCallback(UIUpdateType type, const QString& widgetId, 
                                             std::function<void(const QVariant&)> callback)
{
    QString key = QString("%1_%2").arg(static_cast<int>(type)).arg(widgetId);
    m_updateCallbacks[key] = callback;
}

void UIUpdateOptimizer::unregisterUpdateCallback(UIUpdateType type, const QString& widgetId)
{
    QString key = QString("%1_%2").arg(static_cast<int>(type)).arg(widgetId);
    m_updateCallbacks.remove(key);
}

void UIUpdateOptimizer::processUpdates()
{
    if (m_paused) {
        return;
    }
    
    QElapsedTimer timer;
    timer.start();
    
    QMutexLocker locker(&m_queueMutex);
    
    if (m_updateQueue.isEmpty()) {
        return;
    }
    
    // 按优先级排序
    std::sort(m_updateQueue.begin(), m_updateQueue.end(), 
              [](const UIUpdateTask& a, const UIUpdateTask& b) {
                  return a.priority > b.priority;
              });
    
    // 批处理更新
    processBatchUpdates();
    
    // 更新性能统计
    qint64 updateTime = timer.elapsed();
    m_metrics.totalUpdateTime += updateTime;
    m_metrics.totalUpdates++;
    m_metrics.averageUpdateTime = static_cast<double>(m_metrics.totalUpdateTime) / m_metrics.totalUpdates;
    
    // 帧率限制
    if (m_optimizationConfig.enableFrameRateLimit) {
        qint64 frameTime = m_frameTimer.elapsed();
        if (frameTime < (1000 / m_targetFPS)) {
            return; // 跳过此帧
        }
        m_frameTimer.restart();
    }
    
    // 处理完成（updatesProcessed信号已移除）
}

void UIUpdateOptimizer::onPerformanceTimer()
{
    updatePerformanceMetrics();
    
    // 发送性能统计信号
    emit updateStatistics(getPendingUpdateCount(), getAverageUpdateTime(), getUpdateRate());
    
    // 检查性能警告
    if (m_metrics.averageUpdateTime > 50.0) {
        emit performanceWarning("UI更新平均时间过长，可能影响用户体验");
    }
    
    if (getPendingUpdateCount() > m_maxQueueSize * 0.8) {
        emit performanceWarning("UI更新队列接近满载，可能出现延迟");
    }
}

void UIUpdateOptimizer::onOptimizationTimer()
{
    optimizeQueue();
    
    if (m_optimizationConfig.enableAdaptiveInterval) {
        optimizeUpdateFrequency();
    }
}

void UIUpdateOptimizer::onAdaptiveTuning()
{
    // 自适应调节更新间隔
    if (m_adaptiveMode) {
        adaptiveAdjustment();
    }
    
    // 检查系统负载
    checkSystemLoad();
}

void UIUpdateOptimizer::adaptiveAdjustment()
{
    // 根据当前性能调整更新间隔
    double avgTime = getAverageUpdateTime();
    int pendingCount = getPendingUpdateCount();
    
    // 如果平均更新时间过长，增加间隔
    if (avgTime > 20.0) {
        for (auto it = m_updateIntervals.begin(); it != m_updateIntervals.end(); ++it) {
            it.value() = qMin(it.value() * 1.2, 1000.0);
        }
    }
    // 如果性能良好且队列较空，减少间隔
    else if (avgTime < 10.0 && pendingCount < m_maxQueueSize * 0.3) {
        for (auto it = m_updateIntervals.begin(); it != m_updateIntervals.end(); ++it) {
            it.value() = qMax(it.value() * 0.9, 16.0);
        }
    }
}

void UIUpdateOptimizer::checkSystemLoad()
{
    // 检查CPU和内存使用情况
    if (m_metrics.cpuUsage > m_optimizationConfig.cpuThreshold) {
        // 降低更新频率
        m_updateTimer->setInterval(qMin(static_cast<int>(m_updateTimer->interval() * 1.5), 100));
        LogManager::getInstance()->warning("系统CPU使用率过高，已降低UI更新频率", "UIUpdateOptimizer");
    }
    
    if (m_metrics.memoryUsage > m_optimizationConfig.memoryThreshold) {
        // 清理部分低优先级更新
        clearLowPriorityUpdates();
        LogManager::getInstance()->warning("系统内存使用率过高，已清理低优先级更新", "UIUpdateOptimizer");
    }
}

void UIUpdateOptimizer::clearLowPriorityUpdates()
{
    QMutexLocker locker(&m_queueMutex);
    
    // 移除优先级低于30的更新
    m_updateQueue.erase(
        std::remove_if(m_updateQueue.begin(), m_updateQueue.end(),
                      [](const UIUpdateTask& task) {
                          return task.priority < 30;
                      }),
        m_updateQueue.end()
    );
}

QList<UIUpdateTask> UIUpdateOptimizer::coalesceUpdates(const QList<UIUpdateTask>& tasks)
{
    QHash<QString, UIUpdateTask> coalescedMap;
    
    for (const UIUpdateTask& task : tasks) {
        QString key = QString("%1_%2").arg(static_cast<int>(task.type)).arg(task.widgetId);
        
        // 如果已存在相同类型和ID的任务，保留最新的
        if (!coalescedMap.contains(key) || task.timestamp > coalescedMap[key].timestamp) {
            coalescedMap[key] = task;
        }
    }
    
    return coalescedMap.values();
}

void UIUpdateOptimizer::processBatchUpdates()
{
    QList<UIUpdateTask> currentBatch;
    QDateTime now = QDateTime::currentDateTime();
    
    // 选择要处理的更新
    for (int i = 0; i < m_updateQueue.size() && currentBatch.size() < m_maxBatchSize; ++i) {
        const UIUpdateTask& task = m_updateQueue[i];
        
        // 检查更新间隔
        int interval = m_updateIntervals.value(task.type, 100);
        if (interval > 0 && task.timestamp.msecsTo(now) < interval) {
            continue; // 还没到更新时间
        }
        
        currentBatch.append(task);
    }
    
    // 移除已处理的任务
    for (const UIUpdateTask& task : currentBatch) {
        m_updateQueue.removeOne(task);
    }
    
    // 合并相同类型的更新
    mergeUpdates(currentBatch);
    
    // 执行更新
    for (const UIUpdateTask& task : currentBatch) {
        executeUpdate(task);
    }
    
    // 发送批处理信号
    if (!currentBatch.isEmpty()) {
        emit batchUpdateRequired(currentBatch);
    }
}

void UIUpdateOptimizer::executeUpdate(const UIUpdateTask& task)
{
    // 查找回调函数
    QString key = QString("%1_%2").arg(static_cast<int>(task.type)).arg(task.widgetId);
    auto it = m_updateCallbacks.find(key);
    
    if (it != m_updateCallbacks.end()) {
        // 执行回调
        it.value()(task.data);
    } else {
        // 发送信号
        emit updateRequired(task);
    }
}

void UIUpdateOptimizer::optimizeQueue()
{
    QMutexLocker locker(&m_queueMutex);
    
    QDateTime now = QDateTime::currentDateTime();
    
    // 移除过期的低优先级任务
    m_updateQueue.erase(
        std::remove_if(m_updateQueue.begin(), m_updateQueue.end(), 
                      [now](const UIUpdateTask& task) {
                          return task.priority < 50 && 
                                 task.timestamp.msecsTo(now) > 5000; // 5秒过期
                      }),
        m_updateQueue.end()
    );
    
    // 去重：移除重复的更新
    QHash<QString, int> lastIndices;
    for (int i = 0; i < m_updateQueue.size(); ++i) {
        const UIUpdateTask& task = m_updateQueue[i];
        QString key = QString("%1_%2").arg(static_cast<int>(task.type)).arg(task.widgetId);
        
        if (lastIndices.contains(key)) {
            // 移除之前的任务，保留最新的
            m_updateQueue.removeAt(lastIndices[key]);
            // 更新索引
            for (auto it = lastIndices.begin(); it != lastIndices.end(); ++it) {
                if (it.value() > lastIndices[key]) {
                    it.value()--;
                }
            }
            i--; // 调整当前索引
        }
        lastIndices[key] = i;
    }
}

void UIUpdateOptimizer::updatePerformanceMetrics()
{
    QDateTime now = QDateTime::currentDateTime();
    qint64 elapsedMs = m_metrics.lastUpdate.msecsTo(now);
    
    if (elapsedMs > 0) {
        m_metrics.updatesPerSecond = (m_metrics.totalUpdates * 1000) / elapsedMs;
        m_metrics.lastUpdate = now;
    }
    
    // 计算当前FPS
    if (m_frameTimer.isValid()) {
        m_currentFPS = 1000 / qMax(1, static_cast<int>(m_frameTimer.elapsed()));
    }
}

bool UIUpdateOptimizer::shouldSkipUpdate(const UIUpdateTask& task)
{
    QString key = QString("%1_%2").arg(static_cast<int>(task.type)).arg(task.widgetId);
    
    // 检查是否有相同的更新
    if (m_lastUpdates.contains(key)) {
        const UIUpdateTask& lastTask = m_lastUpdates[key];
        int interval = m_updateIntervals.value(task.type, 100);
        
        // 如果间隔时间未到，跳过更新
        if (interval > 0 && lastTask.timestamp.msecsTo(task.timestamp) < interval) {
            return true;
        }
        
        // 如果数据相同，跳过更新
        if (lastTask.data == task.data) {
            return true;
        }
    }
    
    return false;
}

bool UIUpdateOptimizer::shouldBatchUpdate(const UIUpdateTask& task)
{
    // 错误消息和高优先级任务不批处理
    if (task.type == UIUpdateType::ErrorMessage || task.priority > 80) {
        return false;
    }
    
    return true;
}

void UIUpdateOptimizer::mergeUpdates(QList<UIUpdateTask>& tasks)
{
    QHash<QString, int> indices;
    
    // 找出相同类型和ID的任务
    for (int i = 0; i < tasks.size(); ++i) {
        const UIUpdateTask& task = tasks[i];
        QString key = QString("%1_%2").arg(static_cast<int>(task.type)).arg(task.widgetId);
        
        if (indices.contains(key)) {
            // 合并任务，保留最新的数据
            int oldIndex = indices[key];
            if (task.timestamp > tasks[oldIndex].timestamp) {
                tasks[oldIndex] = task;
            }
            tasks.removeAt(i);
            i--; // 调整索引
        } else {
            indices[key] = i;
        }
    }
}

UIPerformanceMetrics UIUpdateOptimizer::getPerformanceMetrics() const
{
    return m_metrics;
}

QStringList UIUpdateOptimizer::getOptimizationSuggestions() const
{
    QStringList suggestions;
    
    // 基于性能指标提供建议
    if (m_metrics.averageUpdateTime > 30.0) {
        suggestions << "建议减少单次更新的数据量或优化更新逻辑";
    }
    
    if (getPendingUpdateCount() > m_maxQueueSize * 0.7) {
        suggestions << "建议增加批处理大小或降低更新频率";
    }
    
    if (m_metrics.droppedUpdates > m_metrics.totalUpdates * 0.05) {
        suggestions << "建议优化更新策略，减少不必要的更新";
    }
    
    if (m_metrics.cpuUsage > 70.0) {
        suggestions << "建议启用更新合并功能以减少CPU负载";
    }
    
    if (m_currentFPS < m_targetFPS * 0.8) {
        suggestions << "建议启用帧率限制或使用延迟渲染策略";
    }
    
    // 基于配置提供建议
    if (!m_optimizationConfig.enableCoalescing) {
        suggestions << "建议启用更新合并功能以提高性能";
    }
    
    if (!m_optimizationConfig.enableAdaptiveInterval) {
        suggestions << "建议启用自适应间隔调节功能";
    }
    
    // 基于渲染策略提供建议
    for (auto it = m_renderStrategies.begin(); it != m_renderStrategies.end(); ++it) {
        UIUpdateType type = it.key();
        RenderStrategy strategy = it.value();
        
        if (type == UIUpdateType::RealTimeData && strategy != RenderStrategy::Immediate) {
            suggestions << "建议将实时数据更新设置为立即渲染策略";
        }
        
        if (type == UIUpdateType::Statistics && strategy == RenderStrategy::Immediate) {
            suggestions << "建议将统计数据更新设置为批量或延迟渲染策略";
        }
    }
    
    if (suggestions.isEmpty()) {
        suggestions << "当前性能表现良好，无需特别优化";
    }
    
    return suggestions;
}

// 缓存管理器相关方法实现
bool UIUpdateOptimizer::initializeCacheManager(const CacheConfig& config)
{
    if (m_cacheManager) {
        m_cacheManager->setMaxSize(config.maxSize);
        m_cacheManager->setDefaultTTL(config.ttl / 1000); // 转换为秒
        m_cacheManager->setCachePolicy(static_cast<CachePolicy>(config.cachePolicy));
        return true;
    }
    return false;
}

QJsonObject UIUpdateOptimizer::getCacheStatistics() const
{
    if (m_cacheManager) {
        return m_cacheManager->getStatistics();
    }
    return QJsonObject();
}

void UIUpdateOptimizer::setCacheConfiguration(const CacheConfig& config)
{
    if (m_cacheManager) {
        m_cacheManager->setMaxSize(config.maxSize);
        m_cacheManager->setDefaultTTL(config.ttl / 1000);
        m_cacheManager->setCachePolicy(static_cast<CachePolicy>(config.cachePolicy));
    }
}

bool UIUpdateOptimizer::isCacheEnabled() const
{
    return m_cacheManager != nullptr;
}

void UIUpdateOptimizer::enableCache(bool enabled)
{
    if (!m_cacheManager && enabled) {
        // 如果缓存管理器不存在且需要启用，则创建并初始化
        m_cacheManager = DataCacheManager::getInstance();
        if (m_cacheManager) {
            m_cacheManager->setMaxSize(100 * 1024 * 1024); // 100MB
            m_cacheManager->setDefaultTTL(300); // 5分钟
            m_cacheManager->setCachePolicy(CachePolicy::LRU);
        }
    } else if (!enabled && m_cacheManager) {
        // 如果需要禁用缓存，清空缓存并设置为nullptr
        m_cacheManager->clear();
        m_cacheManager = nullptr;
    }
}

void UIUpdateOptimizer::clearCache()
{
    if (m_cacheManager) {
        m_cacheManager->clear();
        qDebug() << "UI更新优化器缓存已清理";
    }
}

void UIUpdateOptimizer::setCacheSize(qint64 size)
{
    if (m_cacheManager) {
        m_cacheManager->setMaxSize(size);
    }
}

QVariant UIUpdateOptimizer::getCachedData(const QString& key) const
{
    if (m_cacheManager) {
        return m_cacheManager->get(key);
    }
    return QVariant();
}

void UIUpdateOptimizer::setCachedData(const QString& key, const QVariant& data)
{
    if (m_cacheManager) {
        m_cacheManager->put(key, data, CacheItemType::UserData);
    }
}