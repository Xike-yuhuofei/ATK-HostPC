#ifndef CONTINUOUSOPTIMIZER_H
#define CONTINUOUSOPTIMIZER_H

#include <QObject>
#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDateTime>
#include <QMutex>
#include <QThread>
#include <memory>

class PerformanceMonitor;
class MemoryOptimizer;
class UIUpdateOptimizer;
class CommunicationBufferPool;
class PerformanceConfigManager;

/**
 * @brief 持续优化管理器
 * 
 * 负责实时监控系统性能，自动调整优化参数，
 * 实施智能优化策略，确保系统持续高效运行
 */
class ContinuousOptimizer : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 优化策略类型
     */
    enum class OptimizationStrategy {
        Conservative,    // 保守策略 - 稳定优先
        Balanced,       // 平衡策略 - 性能与稳定并重
        Aggressive,     // 激进策略 - 性能优先
        Adaptive        // 自适应策略 - 根据负载动态调整
    };

    /**
     * @brief 性能指标结构
     */
    struct PerformanceMetrics {
        double cpuUsage;           // CPU使用率
        double memoryUsage;        // 内存使用率
        double dbResponseTime;     // 数据库响应时间
        double uiResponseTime;     // UI响应时间
        double communicationLatency; // 通信延迟
        int errorCount;            // 错误计数
        QDateTime timestamp;       // 时间戳
    };

    /**
     * @brief 优化建议结构
     */
    struct OptimizationRecommendation {
        QString component;         // 组件名称
        QString parameter;         // 参数名称
        QVariant currentValue;     // 当前值
        QVariant recommendedValue; // 建议值
        QString reason;            // 建议原因
        double expectedImprovement; // 预期改善程度
    };

    explicit ContinuousOptimizer(QObject *parent = nullptr);
    ~ContinuousOptimizer();

    /**
     * @brief 初始化持续优化器
     * @param performanceMonitor 性能监控器
     * @param memoryOptimizer 内存优化器
     * @param uiOptimizer UI优化器
     * @param bufferPool 通信缓冲池
     * @param configManager 配置管理器
     * @return 是否初始化成功
     */
    bool initialize(PerformanceMonitor* performanceMonitor,
                   MemoryOptimizer* memoryOptimizer,
                   UIUpdateOptimizer* uiOptimizer,
                   CommunicationBufferPool* bufferPool,
                   PerformanceConfigManager* configManager);

    /**
     * @brief 启动持续优化
     */
    void startOptimization();

    /**
     * @brief 停止持续优化
     */
    void stopOptimization();

    /**
     * @brief 设置优化策略
     * @param strategy 优化策略
     */
    void setOptimizationStrategy(OptimizationStrategy strategy);

    /**
     * @brief 获取当前性能指标
     * @return 性能指标
     */
    PerformanceMetrics getCurrentMetrics() const;

    /**
     * @brief 获取优化建议
     * @return 优化建议列表
     */
    QList<OptimizationRecommendation> getOptimizationRecommendations() const;

    /**
     * @brief 应用优化建议
     * @param recommendations 要应用的建议列表
     * @return 成功应用的建议数量
     */
    int applyOptimizationRecommendations(const QList<OptimizationRecommendation>& recommendations);

    /**
     * @brief 生成性能报告
     * @return JSON格式的性能报告
     */
    QJsonObject generatePerformanceReport() const;

    /**
     * @brief 是否正在运行
     * @return 运行状态
     */
    bool isRunning() const { return m_isRunning; }

public slots:
    /**
     * @brief 手动触发优化分析
     */
    void triggerOptimizationAnalysis();

    /**
     * @brief 重置优化统计
     */
    void resetOptimizationStats();

signals:
    /**
     * @brief 性能指标更新信号
     * @param metrics 新的性能指标
     */
    void metricsUpdated(const PerformanceMetrics& metrics);

    /**
     * @brief 优化建议生成信号
     * @param recommendations 优化建议列表
     */
    void optimizationRecommendationsGenerated(const QList<OptimizationRecommendation>& recommendations);

    /**
     * @brief 优化应用完成信号
     * @param appliedCount 应用的优化数量
     * @param totalCount 总优化数量
     */
    void optimizationApplied(int appliedCount, int totalCount);

    /**
     * @brief 性能警告信号
     * @param component 组件名称
     * @param message 警告消息
     * @param severity 严重程度 (1-5)
     */
    void performanceWarning(const QString& component, const QString& message, int severity);

private slots:
    /**
     * @brief 定时优化分析
     */
    void performOptimizationAnalysis();

    /**
     * @brief 收集性能指标
     */
    void collectPerformanceMetrics();

    /**
     * @brief 分析性能趋势
     */
    void analyzePerformanceTrends();

private:
    /**
     * @brief 生成优化建议
     * @param metrics 当前性能指标
     * @return 优化建议列表
     */
    QList<OptimizationRecommendation> generateRecommendations(const PerformanceMetrics& metrics);

    /**
     * @brief 应用内存优化建议
     * @param recommendation 优化建议
     * @return 是否成功应用
     */
    bool applyMemoryOptimization(const OptimizationRecommendation& recommendation);

    /**
     * @brief 应用UI优化建议
     * @param recommendation 优化建议
     * @return 是否成功应用
     */
    bool applyUIOptimization(const OptimizationRecommendation& recommendation);

    /**
     * @brief 应用通信优化建议
     * @param recommendation 优化建议
     * @return 是否成功应用
     */
    bool applyCommunicationOptimization(const OptimizationRecommendation& recommendation);

    /**
     * @brief 计算性能评分
     * @param metrics 性能指标
     * @return 性能评分 (0-100)
     */
    double calculatePerformanceScore(const PerformanceMetrics& metrics) const;

    /**
     * @brief 检测性能异常
     * @param metrics 性能指标
     * @return 是否存在异常
     */
    bool detectPerformanceAnomalies(const PerformanceMetrics& metrics) const;

    /**
     * @brief 保存性能历史
     * @param metrics 性能指标
     */
    void savePerformanceHistory(const PerformanceMetrics& metrics);

private:
    // 组件引用
    PerformanceMonitor* m_performanceMonitor;
    MemoryOptimizer* m_memoryOptimizer;
    UIUpdateOptimizer* m_uiOptimizer;
    CommunicationBufferPool* m_bufferPool;
    PerformanceConfigManager* m_configManager;

    // 定时器
    QTimer* m_optimizationTimer;
    QTimer* m_metricsTimer;

    // 状态
    bool m_isRunning;
    OptimizationStrategy m_strategy;

    // 性能数据
    QList<PerformanceMetrics> m_metricsHistory;
    QList<OptimizationRecommendation> m_lastRecommendations;
    mutable QMutex m_metricsMutex;

    // 配置参数
    int m_optimizationInterval;     // 优化分析间隔(ms)
    int m_metricsInterval;          // 指标收集间隔(ms)
    int m_historySize;              // 历史数据保留数量
    double m_performanceThreshold;  // 性能阈值

    // 统计信息
    int m_totalOptimizations;
    int m_successfulOptimizations;
    QDateTime m_startTime;
};

#endif // CONTINUOUSOPTIMIZER_H