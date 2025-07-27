#ifndef ADAPTIVECONFIGMANAGER_H
#define ADAPTIVECONFIGMANAGER_H

#include <QObject>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>
#include <QMutex>
#include <QDateTime>
#include <QVariant>
#include <QHash>
#include <memory>

class ContinuousOptimizer;
class IntelligentAnalyzer;

/**
 * @brief 自适应配置管理器
 * 
 * 根据系统性能状态和智能分析结果，
 * 动态调整各种优化参数，实现自适应优化
 */
class AdaptiveConfigManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 配置参数类型
     */
    enum class ParameterType {
        Memory,          // 内存相关参数
        Database,        // 数据库相关参数
        UI,             // UI相关参数
        Communication,   // 通信相关参数
        Performance,     // 性能监控参数
        System          // 系统级参数
    };

    /**
     * @brief 调整策略
     */
    enum class AdjustmentStrategy {
        Conservative,    // 保守调整 - 小幅度变化
        Moderate,       // 适中调整 - 中等幅度变化
        Aggressive,     // 激进调整 - 大幅度变化
        Adaptive        // 自适应调整 - 根据效果动态调整
    };

    /**
     * @brief 配置参数定义
     */
    struct ParameterDefinition {
        QString name;           // 参数名称
        ParameterType type;     // 参数类型
        QVariant defaultValue;  // 默认值
        QVariant minValue;      // 最小值
        QVariant maxValue;      // 最大值
        QVariant currentValue;  // 当前值
        QString description;    // 参数描述
        double sensitivity;     // 敏感度 (0-1)
        bool autoAdjust;       // 是否自动调整
    };

    /**
     * @brief 配置调整记录
     */
    struct AdjustmentRecord {
        QString parameterName;  // 参数名称
        QVariant oldValue;      // 调整前值
        QVariant newValue;      // 调整后值
        QString reason;         // 调整原因
        QDateTime timestamp;    // 调整时间
        double expectedImprovement; // 预期改善
        double actualImprovement;   // 实际改善
        bool successful;        // 是否成功
    };

    /**
     * @brief 性能状态
     */
    struct PerformanceState {
        double cpuUsage;        // CPU使用率
        double memoryUsage;     // 内存使用量
        double dbResponseTime;  // 数据库响应时间
        double uiResponseTime;  // UI响应时间
        double communicationLatency; // 通信延迟
        double overallScore;    // 综合评分
        QDateTime timestamp;    // 时间戳
    };

    explicit AdaptiveConfigManager(QObject *parent = nullptr);
    ~AdaptiveConfigManager();

    /**
     * @brief 初始化配置管理器
     * @param optimizer 持续优化器
     * @param analyzer 智能分析器
     * @return 是否初始化成功
     */
    bool initialize(ContinuousOptimizer* optimizer, IntelligentAnalyzer* analyzer);

    /**
     * @brief 注册配置参数
     * @param definition 参数定义
     * @return 是否注册成功
     */
    bool registerParameter(const ParameterDefinition& definition);

    /**
     * @brief 获取参数值
     * @param name 参数名称
     * @return 参数值
     */
    QVariant getParameterValue(const QString& name) const;

    /**
     * @brief 设置参数值
     * @param name 参数名称
     * @param value 参数值
     * @param reason 设置原因
     * @return 是否设置成功
     */
    bool setParameterValue(const QString& name, const QVariant& value, const QString& reason = QString());

    /**
     * @brief 启动自适应调整
     */
    void startAdaptiveAdjustment();

    /**
     * @brief 停止自适应调整
     */
    void stopAdaptiveAdjustment();

    /**
     * @brief 设置调整策略
     * @param strategy 调整策略
     */
    void setAdjustmentStrategy(AdjustmentStrategy strategy);

    /**
     * @brief 更新性能状态
     * @param state 性能状态
     */
    void updatePerformanceState(const PerformanceState& state);

    /**
     * @brief 手动触发参数调整
     * @param parameterType 参数类型（空表示所有类型）
     * @return 调整的参数数量
     */
    int triggerParameterAdjustment(ParameterType parameterType = ParameterType::System);

    /**
     * @brief 获取调整历史
     * @param parameterName 参数名称（空表示所有参数）
     * @param limit 返回记录数限制
     * @return 调整记录列表
     */
    QList<AdjustmentRecord> getAdjustmentHistory(const QString& parameterName = QString(), int limit = 50) const;

    /**
     * @brief 获取参数统计信息
     * @return JSON格式的统计信息
     */
    QJsonObject getParameterStatistics() const;

    /**
     * @brief 导出配置
     * @param filePath 导出文件路径
     * @return 是否导出成功
     */
    bool exportConfiguration(const QString& filePath) const;

    /**
     * @brief 导入配置
     * @param filePath 配置文件路径
     * @return 是否导入成功
     */
    bool importConfiguration(const QString& filePath);

    /**
     * @brief 重置所有参数到默认值
     */
    void resetToDefaults();

    /**
     * @brief 获取优化建议
     * @return 优化建议列表
     */
    QStringList getOptimizationSuggestions() const;

    /**
     * @brief 是否正在运行
     * @return 运行状态
     */
    bool isRunning() const { return m_isRunning; }

public slots:
    /**
     * @brief 应用智能分析建议
     * @param recommendations 智能建议
     */
    void applyIntelligentRecommendations(const QJsonArray& recommendations);

    /**
     * @brief 回滚最近的调整
     * @param count 回滚的调整数量
     * @return 成功回滚的数量
     */
    int rollbackRecentAdjustments(int count = 1);

signals:
    /**
     * @brief 参数值变化信号
     * @param name 参数名称
     * @param oldValue 旧值
     * @param newValue 新值
     * @param reason 变化原因
     */
    void parameterChanged(const QString& name, const QVariant& oldValue, 
                         const QVariant& newValue, const QString& reason);

    /**
     * @brief 自适应调整完成信号
     * @param adjustedCount 调整的参数数量
     * @param totalParameters 总参数数量
     */
    void adaptiveAdjustmentCompleted(int adjustedCount, int totalParameters);

    /**
     * @brief 性能改善信号
     * @param parameterName 参数名称
     * @param improvement 改善程度
     */
    void performanceImproved(const QString& parameterName, double improvement);

    /**
     * @brief 调整失败信号
     * @param parameterName 参数名称
     * @param reason 失败原因
     */
    void adjustmentFailed(const QString& parameterName, const QString& reason);

private slots:
    /**
     * @brief 执行定期调整
     */
    void performPeriodicAdjustment();

    /**
     * @brief 评估调整效果
     */
    void evaluateAdjustmentEffects();

private:
    /**
     * @brief 计算参数调整值
     * @param parameter 参数定义
     * @param performanceState 性能状态
     * @return 建议的新值
     */
    QVariant calculateAdjustmentValue(const ParameterDefinition& parameter, 
                                     const PerformanceState& performanceState) const;

    /**
     * @brief 验证参数值
     * @param parameter 参数定义
     * @param value 要验证的值
     * @return 是否有效
     */
    bool validateParameterValue(const ParameterDefinition& parameter, const QVariant& value) const;

    /**
     * @brief 应用参数调整
     * @param name 参数名称
     * @param newValue 新值
     * @param reason 调整原因
     * @return 是否应用成功
     */
    bool applyParameterAdjustment(const QString& name, const QVariant& newValue, const QString& reason);

    /**
     * @brief 计算性能改善
     * @param beforeState 调整前状态
     * @param afterState 调整后状态
     * @return 改善程度 (-1到1)
     */
    double calculatePerformanceImprovement(const PerformanceState& beforeState, 
                                          const PerformanceState& afterState) const;

    /**
     * @brief 分析参数影响
     * @param parameterName 参数名称
     * @return 影响分析结果
     */
    QJsonObject analyzeParameterImpact(const QString& parameterName) const;

    /**
     * @brief 生成调整建议
     * @param parameterType 参数类型
     * @return 调整建议列表
     */
    QList<QPair<QString, QVariant>> generateAdjustmentSuggestions(ParameterType parameterType) const;

    /**
     * @brief 学习调整模式
     */
    void learnAdjustmentPatterns();

    /**
     * @brief 预测调整效果
     * @param parameterName 参数名称
     * @param newValue 新值
     * @return 预测的改善程度
     */
    double predictAdjustmentEffect(const QString& parameterName, const QVariant& newValue) const;

    /**
     * @brief 保存配置状态
     */
    void saveConfigurationState();

    /**
     * @brief 加载配置状态
     */
    void loadConfigurationState();

    /**
     * @brief 获取策略因子
     * @return 策略调整因子
     */
    double getStrategyFactor() const;

    /**
     * @brief 注册默认参数
     */
    void registerDefaultParameters();

private:
    // 组件引用
    ContinuousOptimizer* m_optimizer;
    IntelligentAnalyzer* m_analyzer;
    
    // 参数管理
    QHash<QString, ParameterDefinition> m_parameters;
    mutable QMutex m_parametersMutex;
    
    // 调整历史
    QList<AdjustmentRecord> m_adjustmentHistory;
    mutable QMutex m_historyMutex;
    
    // 性能状态历史
    QList<PerformanceState> m_performanceHistory;
    mutable QMutex m_performanceMutex;
    
    // 定时器
    QTimer* m_adjustmentTimer;
    QTimer* m_evaluationTimer;
    
    // 状态
    bool m_isRunning;
    AdjustmentStrategy m_strategy;
    
    // 配置参数
    int m_adjustmentInterval;      // 调整间隔(ms)
    int m_evaluationInterval;      // 评估间隔(ms)
    int m_maxHistorySize;          // 最大历史记录数
    double m_improvementThreshold; // 改善阈值
    
    // 学习参数
    double m_learningRate;         // 学习率
    double m_adaptationFactor;     // 适应因子
    
    // 统计信息
    int m_totalAdjustments;
    int m_successfulAdjustments;
    int m_failedAdjustments;
    QDateTime m_lastAdjustmentTime;
};

#endif // ADAPTIVECONFIGMANAGER_H