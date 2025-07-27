#include "continuousoptimizer.h"
#include "performancemonitor.h"
#include "memoryoptimizer.h"
#include "performanceconfigmanager.h"
#include "../ui/uiupdateoptimizer.h"
#include "../communication/communicationbufferpool.h"
#include <QDebug>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QtMath>
#include <QCoreApplication>

ContinuousOptimizer::ContinuousOptimizer(QObject *parent)
    : QObject(parent)
    , m_performanceMonitor(nullptr)
    , m_memoryOptimizer(nullptr)
    , m_uiOptimizer(nullptr)
    , m_bufferPool(nullptr)
    , m_configManager(nullptr)
    , m_optimizationTimer(new QTimer(this))
    , m_metricsTimer(new QTimer(this))
    , m_isRunning(false)
    , m_strategy(OptimizationStrategy::Balanced)
    , m_optimizationInterval(30000)  // 30秒
    , m_metricsInterval(5000)        // 5秒
    , m_historySize(100)
    , m_performanceThreshold(0.8)
    , m_totalOptimizations(0)
    , m_successfulOptimizations(0)
{
    // 连接定时器信号
    connect(m_optimizationTimer, &QTimer::timeout, this, &ContinuousOptimizer::performOptimizationAnalysis);
    connect(m_metricsTimer, &QTimer::timeout, this, &ContinuousOptimizer::collectPerformanceMetrics);
    
    qDebug() << "[ContinuousOptimizer] 持续优化管理器已创建";
}

ContinuousOptimizer::~ContinuousOptimizer()
{
    stopOptimization();
    qDebug() << "[ContinuousOptimizer] 持续优化管理器已销毁";
}

bool ContinuousOptimizer::initialize(PerformanceMonitor* performanceMonitor,
                                   MemoryOptimizer* memoryOptimizer,
                                   UIUpdateOptimizer* uiOptimizer,
                                   CommunicationBufferPool* bufferPool,
                                   PerformanceConfigManager* configManager)
{
    if (!performanceMonitor || !memoryOptimizer || !configManager) {
        qWarning() << "[ContinuousOptimizer] 初始化失败：必要组件为空";
        return false;
    }

    m_performanceMonitor = performanceMonitor;
    m_memoryOptimizer = memoryOptimizer;
    m_uiOptimizer = uiOptimizer;
    m_bufferPool = bufferPool;
    m_configManager = configManager;

    // 从配置管理器加载设置
    m_optimizationInterval = m_configManager->getConfigValue("continuous_optimization.optimization_interval", 30000).toInt();
    m_metricsInterval = m_configManager->getConfigValue("continuous_optimization.metrics_interval", 5000).toInt();
    m_historySize = m_configManager->getConfigValue("continuous_optimization.history_size", 100).toInt();
    m_performanceThreshold = m_configManager->getConfigValue("continuous_optimization.performance_threshold", 0.8).toDouble();
    
    QString strategyStr = m_configManager->getConfigValue("continuous_optimization.strategy", "balanced").toString();
    if (strategyStr == "conservative") {
        m_strategy = OptimizationStrategy::Conservative;
    } else if (strategyStr == "aggressive") {
        m_strategy = OptimizationStrategy::Aggressive;
    } else if (strategyStr == "adaptive") {
        m_strategy = OptimizationStrategy::Adaptive;
    } else {
        m_strategy = OptimizationStrategy::Balanced;
    }

    qDebug() << "[ContinuousOptimizer] 初始化成功，策略:" << static_cast<int>(m_strategy)
             << "优化间隔:" << m_optimizationInterval << "ms"
             << "指标间隔:" << m_metricsInterval << "ms";
    
    return true;
}

void ContinuousOptimizer::startOptimization()
{
    if (m_isRunning) {
        qDebug() << "[ContinuousOptimizer] 持续优化已在运行中";
        return;
    }

    m_isRunning = true;
    m_startTime = QDateTime::currentDateTime();
    
    // 启动定时器
    m_metricsTimer->start(m_metricsInterval);
    m_optimizationTimer->start(m_optimizationInterval);
    
    // 立即收集一次指标
    collectPerformanceMetrics();
    
    qDebug() << "[ContinuousOptimizer] 持续优化已启动";
}

void ContinuousOptimizer::stopOptimization()
{
    if (!m_isRunning) {
        return;
    }

    m_isRunning = false;
    
    // 停止定时器
    m_metricsTimer->stop();
    m_optimizationTimer->stop();
    
    qDebug() << "[ContinuousOptimizer] 持续优化已停止";
}

void ContinuousOptimizer::setOptimizationStrategy(OptimizationStrategy strategy)
{
    m_strategy = strategy;
    
    // 根据策略调整间隔
    switch (strategy) {
    case OptimizationStrategy::Conservative:
        m_optimizationInterval = 60000;  // 1分钟
        m_performanceThreshold = 0.9;
        break;
    case OptimizationStrategy::Balanced:
        m_optimizationInterval = 30000;  // 30秒
        m_performanceThreshold = 0.8;
        break;
    case OptimizationStrategy::Aggressive:
        m_optimizationInterval = 15000;  // 15秒
        m_performanceThreshold = 0.7;
        break;
    case OptimizationStrategy::Adaptive:
        // 自适应策略会根据当前性能动态调整
        break;
    }
    
    if (m_isRunning && strategy != OptimizationStrategy::Adaptive) {
        m_optimizationTimer->setInterval(m_optimizationInterval);
    }
    
    qDebug() << "[ContinuousOptimizer] 优化策略已设置为:" << static_cast<int>(strategy);
}

ContinuousOptimizer::PerformanceMetrics ContinuousOptimizer::getCurrentMetrics() const
{
    QMutexLocker locker(&m_metricsMutex);
    
    if (m_metricsHistory.isEmpty()) {
        return PerformanceMetrics{};
    }
    
    return m_metricsHistory.last();
}

QList<ContinuousOptimizer::OptimizationRecommendation> ContinuousOptimizer::getOptimizationRecommendations() const
{
    return m_lastRecommendations;
}

int ContinuousOptimizer::applyOptimizationRecommendations(const QList<OptimizationRecommendation>& recommendations)
{
    int appliedCount = 0;
    
    for (const auto& recommendation : recommendations) {
        bool applied = false;
        
        if (recommendation.component == "memory") {
            applied = applyMemoryOptimization(recommendation);
        } else if (recommendation.component == "ui") {
            applied = applyUIOptimization(recommendation);
        } else if (recommendation.component == "communication") {
            applied = applyCommunicationOptimization(recommendation);
        }
        
        if (applied) {
            appliedCount++;
            qDebug() << "[ContinuousOptimizer] 已应用优化:" << recommendation.component 
                     << recommendation.parameter << "->" << recommendation.recommendedValue;
        }
    }
    
    m_totalOptimizations += recommendations.size();
    m_successfulOptimizations += appliedCount;
    
    emit optimizationApplied(appliedCount, recommendations.size());
    
    qDebug() << "[ContinuousOptimizer] 应用优化完成:" << appliedCount << "/" << recommendations.size();
    
    return appliedCount;
}

QJsonObject ContinuousOptimizer::generatePerformanceReport() const
{
    QJsonObject report;
    
    // 基本信息
    report["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    report["running_time"] = m_startTime.secsTo(QDateTime::currentDateTime());
    report["strategy"] = static_cast<int>(m_strategy);
    
    // 统计信息
    QJsonObject stats;
    stats["total_optimizations"] = m_totalOptimizations;
    stats["successful_optimizations"] = m_successfulOptimizations;
    stats["success_rate"] = m_totalOptimizations > 0 ? 
        static_cast<double>(m_successfulOptimizations) / m_totalOptimizations : 0.0;
    report["statistics"] = stats;
    
    // 当前性能指标
    auto currentMetrics = getCurrentMetrics();
    QJsonObject metricsObj;
    metricsObj["cpu_usage"] = currentMetrics.cpuUsage;
    metricsObj["memory_usage"] = currentMetrics.memoryUsage;
    metricsObj["db_response_time"] = currentMetrics.dbResponseTime;
    metricsObj["ui_response_time"] = currentMetrics.uiResponseTime;
    metricsObj["communication_latency"] = currentMetrics.communicationLatency;
    metricsObj["error_count"] = currentMetrics.errorCount;
    metricsObj["performance_score"] = calculatePerformanceScore(currentMetrics);
    report["current_metrics"] = metricsObj;
    
    // 历史趋势
    QJsonArray historyArray;
    QMutexLocker locker(&m_metricsMutex);
    int startIndex = qMax(0, m_metricsHistory.size() - 10); // 最近10个数据点
    for (int i = startIndex; i < m_metricsHistory.size(); ++i) {
        const auto& metrics = m_metricsHistory[i];
        QJsonObject historyPoint;
        historyPoint["timestamp"] = metrics.timestamp.toString(Qt::ISODate);
        historyPoint["performance_score"] = calculatePerformanceScore(metrics);
        historyArray.append(historyPoint);
    }
    report["performance_history"] = historyArray;
    
    // 当前建议
    QJsonArray recommendationsArray;
    for (const auto& rec : m_lastRecommendations) {
        QJsonObject recObj;
        recObj["component"] = rec.component;
        recObj["parameter"] = rec.parameter;
        recObj["current_value"] = rec.currentValue.toString();
        recObj["recommended_value"] = rec.recommendedValue.toString();
        recObj["reason"] = rec.reason;
        recObj["expected_improvement"] = rec.expectedImprovement;
        recommendationsArray.append(recObj);
    }
    report["recommendations"] = recommendationsArray;
    
    return report;
}

void ContinuousOptimizer::triggerOptimizationAnalysis()
{
    if (!m_isRunning) {
        qWarning() << "[ContinuousOptimizer] 优化器未运行，无法触发分析";
        return;
    }
    
    performOptimizationAnalysis();
}

void ContinuousOptimizer::resetOptimizationStats()
{
    m_totalOptimizations = 0;
    m_successfulOptimizations = 0;
    m_startTime = QDateTime::currentDateTime();
    
    QMutexLocker locker(&m_metricsMutex);
    m_metricsHistory.clear();
    
    qDebug() << "[ContinuousOptimizer] 优化统计已重置";
}

void ContinuousOptimizer::performOptimizationAnalysis()
{
    if (!m_performanceMonitor) {
        return;
    }
    
    // 收集当前性能指标
    auto currentMetrics = getCurrentMetrics();
    
    // 检测性能异常
    if (detectPerformanceAnomalies(currentMetrics)) {
        emit performanceWarning("system", "检测到性能异常，正在分析优化方案", 3);
    }
    
    // 生成优化建议
    auto recommendations = generateRecommendations(currentMetrics);
    m_lastRecommendations = recommendations;
    
    if (!recommendations.isEmpty()) {
        emit optimizationRecommendationsGenerated(recommendations);
        
        // 根据策略决定是否自动应用
        bool autoApply = false;
        switch (m_strategy) {
        case OptimizationStrategy::Conservative:
            autoApply = false; // 保守策略不自动应用
            break;
        case OptimizationStrategy::Balanced:
            autoApply = recommendations.size() <= 3; // 平衡策略只在建议较少时自动应用
            break;
        case OptimizationStrategy::Aggressive:
            autoApply = true; // 激进策略自动应用所有建议
            break;
        case OptimizationStrategy::Adaptive:
            // 自适应策略根据性能评分决定
            autoApply = calculatePerformanceScore(currentMetrics) < m_performanceThreshold;
            break;
        }
        
        if (autoApply) {
            applyOptimizationRecommendations(recommendations);
        }
    }
    
    // 分析性能趋势
    analyzePerformanceTrends();
    
    qDebug() << "[ContinuousOptimizer] 优化分析完成，生成" << recommendations.size() << "个建议";
}

void ContinuousOptimizer::collectPerformanceMetrics()
{
    if (!m_performanceMonitor) {
        return;
    }
    
    PerformanceMetrics metrics;
    metrics.timestamp = QDateTime::currentDateTime();
    
    // 从性能监控器获取指标
    // 注意：这里需要根据实际的PerformanceMonitor接口调整
    metrics.cpuUsage = 0.0;  // TODO: 实现CPU使用率获取
    metrics.memoryUsage = 0.0; // TODO: 实现内存使用率获取
    metrics.dbResponseTime = 0.0; // TODO: 实现数据库响应时间获取
    metrics.uiResponseTime = 0.0; // TODO: 实现UI响应时间获取
    metrics.communicationLatency = 0.0; // TODO: 实现通信延迟获取
    metrics.errorCount = 0; // TODO: 实现错误计数获取
    
    // 模拟一些基本指标（实际应用中应该从真实监控数据获取）
    static int counter = 0;
    counter++;
    metrics.cpuUsage = 20.0 + (counter % 30); // 模拟CPU使用率
    metrics.memoryUsage = 1500.0 + (counter % 200); // 模拟内存使用(MB)
    metrics.dbResponseTime = 5.0 + (counter % 10); // 模拟数据库响应时间(ms)
    metrics.uiResponseTime = 0.5 + (counter % 2); // 模拟UI响应时间(ms)
    metrics.communicationLatency = 1.0 + (counter % 3); // 模拟通信延迟(ms)
    metrics.errorCount = counter % 10 == 0 ? 1 : 0; // 偶尔模拟错误
    
    // 保存指标
    savePerformanceHistory(metrics);
    
    emit metricsUpdated(metrics);
}

void ContinuousOptimizer::analyzePerformanceTrends()
{
    QMutexLocker locker(&m_metricsMutex);
    
    if (m_metricsHistory.size() < 5) {
        return; // 数据不足，无法分析趋势
    }
    
    // 分析最近的性能趋势
    int recentCount = qMin(10, m_metricsHistory.size());
    double avgCpu = 0, avgMemory = 0, avgDbTime = 0;
    
    for (int i = m_metricsHistory.size() - recentCount; i < m_metricsHistory.size(); ++i) {
        const auto& metrics = m_metricsHistory[i];
        avgCpu += metrics.cpuUsage;
        avgMemory += metrics.memoryUsage;
        avgDbTime += metrics.dbResponseTime;
    }
    
    avgCpu /= recentCount;
    avgMemory /= recentCount;
    avgDbTime /= recentCount;
    
    // 检查趋势并发出警告
    if (avgCpu > 80.0) {
        emit performanceWarning("cpu", QString("CPU使用率持续偏高: %1%").arg(avgCpu, 0, 'f', 1), 4);
    }
    
    if (avgMemory > 2000.0) {
        emit performanceWarning("memory", QString("内存使用量持续偏高: %1MB").arg(avgMemory, 0, 'f', 1), 4);
    }
    
    if (avgDbTime > 20.0) {
        emit performanceWarning("database", QString("数据库响应时间持续偏高: %1ms").arg(avgDbTime, 0, 'f', 1), 4);
    }
}

QList<ContinuousOptimizer::OptimizationRecommendation> ContinuousOptimizer::generateRecommendations(const PerformanceMetrics& metrics)
{
    QList<OptimizationRecommendation> recommendations;
    
    // 内存优化建议
    if (metrics.memoryUsage > 1800.0) {
        OptimizationRecommendation rec;
        rec.component = "memory";
        rec.parameter = "cleanup_threshold";
        rec.currentValue = 1800;
        rec.recommendedValue = 1500;
        rec.reason = "内存使用量过高，建议降低清理阈值";
        rec.expectedImprovement = 0.15;
        recommendations.append(rec);
    }
    
    // UI优化建议
    if (metrics.uiResponseTime > 2.0) {
        OptimizationRecommendation rec;
        rec.component = "ui";
        rec.parameter = "update_interval";
        rec.currentValue = 100;
        rec.recommendedValue = 150;
        rec.reason = "UI响应时间过长，建议增加更新间隔";
        rec.expectedImprovement = 0.20;
        recommendations.append(rec);
    }
    
    // 数据库优化建议
    if (metrics.dbResponseTime > 15.0) {
        OptimizationRecommendation rec;
        rec.component = "database";
        rec.parameter = "connection_pool_size";
        rec.currentValue = 5;
        rec.recommendedValue = 8;
        rec.reason = "数据库响应时间过长，建议增加连接池大小";
        rec.expectedImprovement = 0.25;
        recommendations.append(rec);
    }
    
    // 通信优化建议
    if (metrics.communicationLatency > 3.0) {
        OptimizationRecommendation rec;
        rec.component = "communication";
        rec.parameter = "buffer_size";
        rec.currentValue = 1024;
        rec.recommendedValue = 2048;
        rec.reason = "通信延迟过高，建议增加缓冲区大小";
        rec.expectedImprovement = 0.18;
        recommendations.append(rec);
    }
    
    return recommendations;
}

bool ContinuousOptimizer::applyMemoryOptimization(const OptimizationRecommendation& recommendation)
{
    if (!m_memoryOptimizer) {
        return false;
    }
    
    // 根据参数类型应用优化
    if (recommendation.parameter == "cleanup_threshold") {
        // TODO: 实现内存清理阈值设置
        qDebug() << "[ContinuousOptimizer] 应用内存优化:" << recommendation.parameter;
        return true;
    }
    
    return false;
}

bool ContinuousOptimizer::applyUIOptimization(const OptimizationRecommendation& recommendation)
{
    if (!m_uiOptimizer) {
        return false;
    }
    
    // 根据参数类型应用优化
    if (recommendation.parameter == "update_interval") {
        // TODO: 实现UI更新间隔设置
        qDebug() << "[ContinuousOptimizer] 应用UI优化:" << recommendation.parameter;
        return true;
    }
    
    return false;
}

bool ContinuousOptimizer::applyCommunicationOptimization(const OptimizationRecommendation& recommendation)
{
    if (!m_bufferPool) {
        return false;
    }
    
    // 根据参数类型应用优化
    if (recommendation.parameter == "buffer_size") {
        // TODO: 实现缓冲区大小设置
        qDebug() << "[ContinuousOptimizer] 应用通信优化:" << recommendation.parameter;
        return true;
    }
    
    return false;
}

double ContinuousOptimizer::calculatePerformanceScore(const PerformanceMetrics& metrics) const
{
    // 计算综合性能评分 (0-100)
    double cpuScore = qMax(0.0, 100.0 - metrics.cpuUsage);
    double memoryScore = qMax(0.0, 100.0 - metrics.memoryUsage / 30.0); // 假设3GB为满分
    double dbScore = qMax(0.0, 100.0 - metrics.dbResponseTime * 2.0); // 50ms为0分
    double uiScore = qMax(0.0, 100.0 - metrics.uiResponseTime * 20.0); // 5ms为0分
    double commScore = qMax(0.0, 100.0 - metrics.communicationLatency * 10.0); // 10ms为0分
    double errorScore = metrics.errorCount == 0 ? 100.0 : qMax(0.0, 100.0 - metrics.errorCount * 10.0);
    
    // 加权平均
    double totalScore = (cpuScore * 0.2 + memoryScore * 0.2 + dbScore * 0.25 + 
                        uiScore * 0.15 + commScore * 0.1 + errorScore * 0.1);
    
    return qBound(0.0, totalScore, 100.0);
}

bool ContinuousOptimizer::detectPerformanceAnomalies(const PerformanceMetrics& metrics) const
{
    // 检测性能异常
    if (metrics.cpuUsage > 90.0 || 
        metrics.memoryUsage > 2500.0 || 
        metrics.dbResponseTime > 50.0 || 
        metrics.uiResponseTime > 5.0 || 
        metrics.communicationLatency > 10.0 || 
        metrics.errorCount > 5) {
        return true;
    }
    
    return false;
}

void ContinuousOptimizer::savePerformanceHistory(const PerformanceMetrics& metrics)
{
    QMutexLocker locker(&m_metricsMutex);
    
    m_metricsHistory.append(metrics);
    
    // 限制历史数据大小
    while (m_metricsHistory.size() > m_historySize) {
        m_metricsHistory.removeFirst();
    }
}