#include "performanceanalyzer.h"
#include "performancemonitor.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QtMath>
#include <algorithm>

PerformanceAnalyzer::PerformanceAnalyzer(QObject* parent)
    : QObject(parent)
    , m_analysisTimer(new QTimer(this))
    , m_currentScore(0.0)
    , m_smartAlertsEnabled(true)
    , m_memoryUsageThreshold(80.0)
    , m_cpuUsageThreshold(75.0)
    , m_diskUsageThreshold(85.0)
    , m_responseTimeThreshold(100.0)
{
    // 连接性能监控器
    connect(PerformanceMonitor::getInstance(), &PerformanceMonitor::metricsUpdated,
            this, &PerformanceAnalyzer::onMetricsUpdated);
    
    // 设置分析定时器 (每5分钟执行一次分析)
    m_analysisTimer->setInterval(300000); // 5分钟
    connect(m_analysisTimer, &QTimer::timeout, this, &PerformanceAnalyzer::performAnalysis);
    m_analysisTimer->start();
    
    qDebug() << "性能分析器已初始化";
}

PerformanceAnalyzer::~PerformanceAnalyzer()
{
    m_analysisTimer->stop();
}

QList<PerformanceAnalyzer::PerformanceTrend> PerformanceAnalyzer::analyzeTrends(int hoursBack) const
{
    QList<PerformanceTrend> trends;
    
    // 分析各项指标的趋势
    QStringList metrics = {"cpuUsage", "memoryUsage", "diskUsage", "appMemoryUsage"};
    
    for (const QString& metric : metrics) {
        PerformanceTrend trend = getMetricTrend(metric, hoursBack);
        if (!trend.metric.isEmpty()) {
            trends.append(trend);
        }
    }
    
    return trends;
}

PerformanceAnalyzer::PerformanceTrend PerformanceAnalyzer::getMetricTrend(const QString& metric, int hoursBack) const
{
    PerformanceTrend trend;
    trend.metric = metric;
    
    QList<double> values = getMetricValues(metric, hoursBack);
    if (values.size() < 10) {
        return trend; // 数据不足
    }
    
    trend.trend = calculateTrend(values);
    trend.changeRate = calculateChangeRate(values);
    trend.startTime = QDateTime::currentDateTime().addSecs(-hoursBack * 3600);
    trend.endTime = QDateTime::currentDateTime();
    
    // 生成描述
    switch (trend.trend) {
    case TrendType::Stable:
        trend.description = QString("%1 保持稳定，变化率: %2%").arg(metric).arg(trend.changeRate, 0, 'f', 2);
        break;
    case TrendType::Increasing:
        trend.description = QString("%1 呈上升趋势，增长率: %2%").arg(metric).arg(trend.changeRate, 0, 'f', 2);
        break;
    case TrendType::Decreasing:
        trend.description = QString("%1 呈下降趋势，下降率: %2%").arg(metric).arg(qAbs(trend.changeRate), 0, 'f', 2);
        break;
    case TrendType::Volatile:
        trend.description = QString("%1 波动较大，标准差: %2").arg(metric).arg(trend.changeRate, 0, 'f', 2);
        break;
    }
    
    return trend;
}

void PerformanceAnalyzer::enableSmartAlerts(bool enabled)
{
    m_smartAlertsEnabled = enabled;
    if (!enabled) {
        m_activeAlerts.clear();
    }
}

QList<PerformanceAnalyzer::SmartAlert> PerformanceAnalyzer::getActiveAlerts() const
{
    return m_activeAlerts;
}

void PerformanceAnalyzer::resolveAlert(const QString& alertId)
{
    for (auto& alert : m_activeAlerts) {
        if (alert.id == alertId) {
            alert.resolved = true;
            break;
        }
    }
}

QStringList PerformanceAnalyzer::getOptimizationSuggestions() const
{
    QStringList suggestions;
    
    PerformanceMetrics current = PerformanceMonitor::getInstance()->getCurrentMetrics();
    
    // 内存优化建议
    if (current.memoryUsage > 80.0) {
        suggestions << "内存使用率较高，建议清理不必要的对象和缓存";
        suggestions << "考虑启用内存优化器的自动清理功能";
    }
    
    // CPU优化建议
    if (current.cpuUsage > 75.0) {
        suggestions << "CPU使用率偏高，检查是否有死循环或耗时操作";
        suggestions << "考虑将耗时任务移至后台线程执行";
    }
    
    // 磁盘优化建议
    if (current.diskUsage > 85.0) {
        suggestions << "磁盘空间不足，清理临时文件和日志";
        suggestions << "启用日志自动轮转和归档功能";
    }
    
    // 线程优化建议
    if (current.threadCount > 50) {
        suggestions << "线程数量较多，检查线程池配置和线程泄露";
    }
    
    return suggestions;
}

QStringList PerformanceAnalyzer::getPredictiveWarnings() const
{
    QStringList warnings;
    
    // 基于趋势预测潜在问题
    QList<PerformanceTrend> trends = analyzeTrends(6); // 分析6小时趋势
    
    for (const auto& trend : trends) {
        if (trend.trend == TrendType::Increasing && trend.changeRate > 10.0) {
            if (trend.metric == "memoryUsage") {
                warnings << "预警: 内存使用率持续上升，可能存在内存泄露";
            } else if (trend.metric == "cpuUsage") {
                warnings << "预警: CPU使用率持续上升，系统负载可能过高";
            }
        }
    }
    
    return warnings;
}

double PerformanceAnalyzer::calculatePerformanceScore() const
{
    PerformanceMetrics current = PerformanceMonitor::getInstance()->getCurrentMetrics();
    
    double score = 100.0;
    
    // 内存评分 (权重: 30%)
    double memoryScore = 100.0 - current.memoryUsage;
    score -= (100.0 - memoryScore) * 0.3;
    
    // CPU评分 (权重: 30%)
    double cpuScore = 100.0 - current.cpuUsage;
    score -= (100.0 - cpuScore) * 0.3;
    
    // 磁盘评分 (权重: 20%)
    double diskScore = 100.0 - current.diskUsage;
    score -= (100.0 - diskScore) * 0.2;
    
    // 响应性评分 (权重: 20%)
    double responseScore = 100.0;
    if (current.customMetrics.contains("responseTime")) {
        double responseTime = current.customMetrics["responseTime"];
        if (responseTime > m_responseTimeThreshold) {
            responseScore = qMax(0.0, 100.0 - (responseTime - m_responseTimeThreshold));
        }
    }
    score -= (100.0 - responseScore) * 0.2;
    
    return qMax(0.0, qMin(100.0, score));
}

QString PerformanceAnalyzer::getPerformanceGrade() const
{
    double score = calculatePerformanceScore();
    
    if (score >= 90) return "A";
    if (score >= 80) return "B";
    if (score >= 70) return "C";
    if (score >= 60) return "D";
    return "F";
}

QString PerformanceAnalyzer::generatePerformanceReport(int hoursBack) const
{
    QString report;
    QTextStream stream(&report);
    
    stream << "=== 性能分析报告 ===\n";
    stream << "生成时间: " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "\n";
    stream << "分析时间范围: " << hoursBack << " 小时\n\n";
    
    // 当前性能评分
    stream << "当前性能评分: " << calculatePerformanceScore() << " (" << getPerformanceGrade() << ")\n\n";
    
    // 性能趋势
    stream << "性能趋势分析:\n";
    QList<PerformanceTrend> trends = analyzeTrends(hoursBack);
    for (const auto& trend : trends) {
        stream << "  - " << trend.description << "\n";
    }
    stream << "\n";
    
    // 优化建议
    stream << "优化建议:\n";
    QStringList suggestions = getOptimizationSuggestions();
    for (const QString& suggestion : suggestions) {
        stream << "  - " << suggestion << "\n";
    }
    stream << "\n";
    
    // 预警信息
    QStringList warnings = getPredictiveWarnings();
    if (!warnings.isEmpty()) {
        stream << "预警信息:\n";
        for (const QString& warning : warnings) {
            stream << "  - " << warning << "\n";
        }
        stream << "\n";
    }
    
    // 活跃告警
    if (!m_activeAlerts.isEmpty()) {
        stream << "活跃告警:\n";
        for (const auto& alert : m_activeAlerts) {
            if (!alert.resolved) {
                stream << "  - " << alert.message << " (级别: ";
                switch (alert.level) {
                case AlertLevel::Info: stream << "信息"; break;
                case AlertLevel::Warning: stream << "警告"; break;
                case AlertLevel::Critical: stream << "严重"; break;
                }
                stream << ")\n";
            }
        }
    }
    
    return report;
}

bool PerformanceAnalyzer::exportReport(const QString& filename, int hoursBack) const
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream out(&file);
    out << generatePerformanceReport(hoursBack);
    
    return true;
}

bool PerformanceAnalyzer::isSystemHealthy() const
{
    double score = calculatePerformanceScore();
    return score >= 70.0 && m_activeAlerts.size() < 5;
}

QStringList PerformanceAnalyzer::getHealthIssues() const
{
    QStringList issues;
    
    if (calculatePerformanceScore() < 70.0) {
        issues << "系统性能评分过低";
    }
    
    if (m_activeAlerts.size() >= 5) {
        issues << "活跃告警数量过多";
    }
    
    // 添加其他健康检查...
    
    return issues;
}

void PerformanceAnalyzer::onMetricsUpdated(const PerformanceMetrics& metrics)
{
    // 实时检查是否需要触发智能告警
    if (m_smartAlertsEnabled) {
        detectAnomalies();
    }
    
    // 更新性能评分
    double newScore = calculatePerformanceScore();
    if (qAbs(newScore - m_currentScore) > 5.0) {
        m_currentScore = newScore;
        emit performanceScoreChanged(m_currentScore);
    }
}

void PerformanceAnalyzer::performAnalysis()
{
    qDebug() << "执行性能分析...";
    
    // 执行各种分析
    detectAnomalies();
    updatePerformanceScore();
    
    // 检查系统健康状态
    bool healthy = isSystemHealthy();
    emit healthStatusChanged(healthy);
    
    m_lastAnalysis = QDateTime::currentDateTime();
}

// 私有方法实现...
PerformanceAnalyzer::TrendType PerformanceAnalyzer::calculateTrend(const QList<double>& values) const
{
    if (values.size() < 3) return TrendType::Stable;
    
    double stdDev = getStandardDeviation(values);
    double mean = getAverageValue(values);
    
    // 如果标准差相对于均值过大，认为是波动
    if (stdDev / mean > 0.3) {
        return TrendType::Volatile;
    }
    
    // 计算线性趋势
    double changeRate = calculateChangeRate(values);
    
    if (qAbs(changeRate) < 5.0) {
        return TrendType::Stable;
    } else if (changeRate > 0) {
        return TrendType::Increasing;
    } else {
        return TrendType::Decreasing;
    }
}

double PerformanceAnalyzer::calculateChangeRate(const QList<double>& values) const
{
    if (values.size() < 2) return 0.0;
    
    double first = values.first();
    double last = values.last();
    
    if (first == 0.0) return 0.0;
    
    return ((last - first) / first) * 100.0;
}

void PerformanceAnalyzer::detectAnomalies()
{
    // 实现异常检测逻辑
    checkMemoryLeaks();
    checkCpuSpikes();
    checkDiskSpace();
    checkResponseTime();
}

void PerformanceAnalyzer::updatePerformanceScore()
{
    double newScore = calculatePerformanceScore();
    if (qAbs(newScore - m_currentScore) > 1.0) {
        m_currentScore = newScore;
        emit performanceScoreChanged(m_currentScore);
    }
}

void PerformanceAnalyzer::checkMemoryLeaks()
{
    // 检查内存泄露的逻辑
    QList<double> memoryValues = getMetricValues("memoryUsage", 2); // 2小时内的数据
    if (memoryValues.size() > 10) {
        TrendType trend = calculateTrend(memoryValues);
        double changeRate = calculateChangeRate(memoryValues);
        
        if (trend == TrendType::Increasing && changeRate > 15.0) {
            SmartAlert alert;
            alert.id = "memory_leak_" + QDateTime::currentDateTime().toString("yyyyMMddhhmmss");
            alert.message = "检测到可能的内存泄露";
            alert.level = AlertLevel::Warning;
            alert.timestamp = QDateTime::currentDateTime();
            alert.suggestions << "检查对象生命周期管理" << "启用内存优化器";
            alert.resolved = false;
            
            m_activeAlerts.append(alert);
            emit smartAlertTriggered(alert);
        }
    }
}

void PerformanceAnalyzer::checkCpuSpikes()
{
    // CPU峰值检测逻辑
}

void PerformanceAnalyzer::checkDiskSpace()
{
    // 磁盘空间检测逻辑
}

void PerformanceAnalyzer::checkResponseTime()
{
    // 响应时间检测逻辑
}

QList<double> PerformanceAnalyzer::getMetricValues(const QString& metric, int hoursBack) const
{
    QList<double> values;
    
    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-hoursBack * 3600);
    QList<PerformanceMetrics> history = PerformanceMonitor::getInstance()->getHistoryMetrics(1000);
    
    for (const auto& metrics : history) {
        if (metrics.timestamp < cutoff) continue;
        
        if (metric == "cpuUsage") {
            values.append(metrics.cpuUsage);
        } else if (metric == "memoryUsage") {
            values.append(metrics.memoryUsage);
        } else if (metric == "diskUsage") {
            values.append(metrics.diskUsage);
        } else if (metric == "appMemoryUsage") {
            values.append(metrics.appMemoryUsage / (1024.0 * 1024.0)); // 转换为MB
        }
    }
    
    return values;
}

double PerformanceAnalyzer::getAverageValue(const QList<double>& values) const
{
    if (values.isEmpty()) return 0.0;
    
    double sum = 0.0;
    for (double value : values) {
        sum += value;
    }
    
    return sum / values.size();
}

double PerformanceAnalyzer::getStandardDeviation(const QList<double>& values) const
{
    if (values.size() < 2) return 0.0;
    
    double mean = getAverageValue(values);
    double sumSquaredDiff = 0.0;
    
    for (double value : values) {
        double diff = value - mean;
        sumSquaredDiff += diff * diff;
    }
    
    return qSqrt(sumSquaredDiff / (values.size() - 1));
}