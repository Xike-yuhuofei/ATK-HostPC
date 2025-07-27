#pragma once

#include <QObject>
#include <QTimer>
#include <QMap>
#include <QDateTime>
#include <QStringList>
#include "performancemonitor.h"

/**
 * @brief 性能分析器 - 新增功能
 * 
 * 提供高级性能分析功能，包括：
 * - 性能趋势分析
 * - 智能性能预警
 * - 自动性能优化建议
 * - 性能报告生成
 */
class PerformanceAnalyzer : public QObject
{
    Q_OBJECT

public:
    enum class TrendType {
        Stable,     // 稳定
        Increasing, // 上升
        Decreasing, // 下降
        Volatile    // 波动
    };

    enum class AlertLevel {
        Info,       // 信息
        Warning,    // 警告
        Critical    // 严重
    };

    struct PerformanceTrend {
        QString metric;
        TrendType trend;
        double changeRate;      // 变化率 (%)
        QDateTime startTime;
        QDateTime endTime;
        QString description;
    };

    struct SmartAlert {
        QString id;
        QString message;
        AlertLevel level;
        QDateTime timestamp;
        QStringList suggestions;
        bool resolved;
    };

    explicit PerformanceAnalyzer(QObject* parent = nullptr);
    ~PerformanceAnalyzer();

    // 趋势分析
    QList<PerformanceTrend> analyzeTrends(int hoursBack = 24) const;
    PerformanceTrend getMetricTrend(const QString& metric, int hoursBack = 24) const;
    
    // 智能预警
    void enableSmartAlerts(bool enabled);
    QList<SmartAlert> getActiveAlerts() const;
    void resolveAlert(const QString& alertId);
    
    // 性能优化建议
    QStringList getOptimizationSuggestions() const;
    QStringList getPredictiveWarnings() const;
    
    // 性能评分
    double calculatePerformanceScore() const;
    QString getPerformanceGrade() const; // A, B, C, D, F
    
    // 报告生成
    QString generatePerformanceReport(int hoursBack = 24) const;
    bool exportReport(const QString& filename, int hoursBack = 24) const;
    
    // 健康检查
    bool isSystemHealthy() const;
    QStringList getHealthIssues() const;

signals:
    void trendDetected(const PerformanceTrend& trend);
    void smartAlertTriggered(const SmartAlert& alert);
    void performanceScoreChanged(double score);
    void healthStatusChanged(bool healthy);

private slots:
    void onMetricsUpdated(const PerformanceMetrics& metrics);
    void performAnalysis();

private:
    // 分析方法
    TrendType calculateTrend(const QList<double>& values) const;
    double calculateChangeRate(const QList<double>& values) const;
    void detectAnomalies();
    void updatePerformanceScore();
    
    // 预警逻辑
    void checkMemoryLeaks();
    void checkCpuSpikes();
    void checkDiskSpace();
    void checkResponseTime();
    
    // 数据处理
    QList<double> getMetricValues(const QString& metric, int hoursBack) const;
    double getAverageValue(const QList<double>& values) const;
    double getStandardDeviation(const QList<double>& values) const;

    QTimer* m_analysisTimer;
    QList<SmartAlert> m_activeAlerts;
    double m_currentScore;
    bool m_smartAlertsEnabled;
    QDateTime m_lastAnalysis;
    
    // 阈值配置
    double m_memoryUsageThreshold;
    double m_cpuUsageThreshold;
    double m_diskUsageThreshold;
    double m_responseTimeThreshold;
};