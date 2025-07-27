#ifndef INTELLIGENTANALYZER_H
#define INTELLIGENTANALYZER_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QTimer>
#include <QMutex>
#include <QVector>
#include <QPair>
#include <memory>

/**
 * @brief 智能性能分析器
 * 
 * 使用机器学习算法分析性能数据，预测性能趋势，
 * 提供智能化的优化建议和异常检测
 */
class IntelligentAnalyzer : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 性能数据点
     */
    struct DataPoint {
        QDateTime timestamp;
        double cpuUsage;
        double memoryUsage;
        double dbResponseTime;
        double uiResponseTime;
        double communicationLatency;
        int errorCount;
        double performanceScore;
    };

    /**
     * @brief 趋势分析结果
     */
    struct TrendAnalysis {
        QString metric;           // 指标名称
        double currentValue;      // 当前值
        double predictedValue;    // 预测值
        double trend;            // 趋势系数 (-1到1)
        double confidence;       // 置信度 (0到1)
        QString interpretation;  // 趋势解释
    };

    /**
     * @brief 异常检测结果
     */
    struct AnomalyDetection {
        QString metric;          // 指标名称
        double value;           // 异常值
        double threshold;       // 阈值
        double severity;        // 严重程度 (0到1)
        QString description;    // 异常描述
        QDateTime detectedAt;   // 检测时间
    };

    /**
     * @brief 智能优化建议
     */
    struct IntelligentRecommendation {
        QString category;        // 优化类别
        QString action;         // 建议动作
        QJsonObject parameters; // 参数设置
        double priority;        // 优先级 (0到1)
        double expectedGain;    // 预期收益
        QString reasoning;      // 推理过程
        QStringList prerequisites; // 前置条件
    };

    /**
     * @brief 性能预测结果
     */
    struct PerformancePrediction {
        QDateTime timeHorizon;   // 预测时间点
        QVector<DataPoint> predictedData; // 预测数据
        double confidence;       // 预测置信度
        QStringList riskFactors; // 风险因素
        QStringList opportunities; // 优化机会
    };

    explicit IntelligentAnalyzer(QObject *parent = nullptr);
    ~IntelligentAnalyzer();

    /**
     * @brief 初始化分析器
     * @return 是否初始化成功
     */
    bool initialize();

    /**
     * @brief 添加性能数据点
     * @param dataPoint 数据点
     */
    void addDataPoint(const DataPoint& dataPoint);

    /**
     * @brief 执行趋势分析
     * @param metric 指标名称（空表示所有指标）
     * @return 趋势分析结果
     */
    QList<TrendAnalysis> analyzeTrends(const QString& metric = QString()) const;

    /**
     * @brief 检测异常
     * @return 异常检测结果
     */
    QList<AnomalyDetection> detectAnomalies() const;

    /**
     * @brief 生成智能优化建议
     * @return 智能优化建议列表
     */
    QList<IntelligentRecommendation> generateIntelligentRecommendations() const;

    /**
     * @brief 预测性能趋势
     * @param hoursAhead 预测时间（小时）
     * @return 性能预测结果
     */
    PerformancePrediction predictPerformance(int hoursAhead = 24) const;

    /**
     * @brief 计算性能健康度
     * @return 健康度评分 (0-100)
     */
    double calculateHealthScore() const;

    /**
     * @brief 获取性能洞察
     * @return JSON格式的洞察报告
     */
    QJsonObject getPerformanceInsights() const;

    /**
     * @brief 设置学习参数
     * @param learningRate 学习率
     * @param windowSize 分析窗口大小
     * @param sensitivityThreshold 敏感度阈值
     */
    void setLearningParameters(double learningRate, int windowSize, double sensitivityThreshold);

    /**
     * @brief 训练模型
     * @return 训练是否成功
     */
    bool trainModel();

    /**
     * @brief 保存模型
     * @param filePath 保存路径
     * @return 是否保存成功
     */
    bool saveModel(const QString& filePath) const;

    /**
     * @brief 加载模型
     * @param filePath 模型文件路径
     * @return 是否加载成功
     */
    bool loadModel(const QString& filePath);

public slots:
    /**
     * @brief 启动智能分析
     */
    void startAnalysis();

    /**
     * @brief 停止智能分析
     */
    void stopAnalysis();

    /**
     * @brief 重置学习数据
     */
    void resetLearningData();

signals:
    /**
     * @brief 趋势分析完成信号
     * @param trends 趋势分析结果
     */
    void trendsAnalyzed(const QList<TrendAnalysis>& trends);

    /**
     * @brief 异常检测信号
     * @param anomalies 检测到的异常
     */
    void anomaliesDetected(const QList<AnomalyDetection>& anomalies);

    /**
     * @brief 智能建议生成信号
     * @param recommendations 智能建议
     */
    void intelligentRecommendationsGenerated(const QList<IntelligentRecommendation>& recommendations);

    /**
     * @brief 性能预测完成信号
     * @param prediction 预测结果
     */
    void performancePredicted(const PerformancePrediction& prediction);

    /**
     * @brief 健康度更新信号
     * @param healthScore 健康度评分
     * @param trend 健康度趋势
     */
    void healthScoreUpdated(double healthScore, double trend);

private slots:
    /**
     * @brief 执行定期分析
     */
    void performPeriodicAnalysis();

private:
    /**
     * @brief 计算移动平均
     * @param values 数值序列
     * @param windowSize 窗口大小
     * @return 移动平均序列
     */
    QVector<double> calculateMovingAverage(const QVector<double>& values, int windowSize) const;

    /**
     * @brief 计算线性回归
     * @param x X轴数据
     * @param y Y轴数据
     * @return 回归系数 (斜率, 截距)
     */
    QPair<double, double> calculateLinearRegression(const QVector<double>& x, const QVector<double>& y) const;

    /**
     * @brief 检测单个指标的异常
     * @param values 指标值序列
     * @param metricName 指标名称
     * @return 异常检测结果
     */
    QList<AnomalyDetection> detectMetricAnomalies(const QVector<double>& values, const QString& metricName) const;

    /**
     * @brief 计算Z-Score
     * @param values 数值序列
     * @return Z-Score序列
     */
    QVector<double> calculateZScore(const QVector<double>& values) const;

    /**
     * @brief 应用指数平滑
     * @param values 原始数据
     * @param alpha 平滑系数
     * @return 平滑后的数据
     */
    QVector<double> applyExponentialSmoothing(const QVector<double>& values, double alpha) const;

    /**
     * @brief 分析季节性模式
     * @param values 数值序列
     * @param period 周期长度
     * @return 季节性强度
     */
    double analyzeSeasonality(const QVector<double>& values, int period) const;

    /**
     * @brief 计算相关性矩阵
     * @return 指标间相关性
     */
    QJsonObject calculateCorrelationMatrix() const;

    /**
     * @brief 识别性能瓶颈
     * @return 瓶颈分析结果
     */
    QStringList identifyBottlenecks() const;

    /**
     * @brief 生成优化路径
     * @param currentState 当前状态
     * @param targetState 目标状态
     * @return 优化路径
     */
    QList<IntelligentRecommendation> generateOptimizationPath(
        const DataPoint& currentState, const DataPoint& targetState) const;

    /**
     * @brief 评估优化效果
     * @param beforeData 优化前数据
     * @param afterData 优化后数据
     * @return 效果评估结果
     */
    QJsonObject evaluateOptimizationEffect(
        const QVector<DataPoint>& beforeData, const QVector<DataPoint>& afterData) const;

    /**
     * @brief 计算趋势置信度
     * @param values 数值序列
     * @param slope 斜率
     * @param intercept 截距
     * @return 置信度
     */
    double calculateTrendConfidence(const QVector<double>& values, double slope, double intercept) const;

    /**
     * @brief 解释趋势
     * @param trend 趋势值
     * @param metric 指标名称
     * @return 趋势解释
     */
    QString interpretTrend(double trend, const QString& metric) const;

    /**
     * @brief 计算预期收益
     * @param trend 趋势分析结果
     * @return 预期收益
     */
    double calculateExpectedGain(const TrendAnalysis& trend) const;

    /**
     * @brief 为趋势生成推理
     * @param trend 趋势分析结果
     * @return 推理说明
     */
    QString generateReasoningForTrend(const TrendAnalysis& trend) const;

    /**
     * @brief 生成优化动作
     * @param metric 指标名称
     * @param direction 优化方向
     * @return 优化动作描述
     */
    QString generateOptimizationAction(const QString& metric, const QString& direction) const;

    /**
     * @brief 生成异常修复动作
     * @param anomaly 异常检测结果
     * @return 修复动作描述
     */
    QString generateAnomalyFixAction(const AnomalyDetection& anomaly) const;

    /**
     * @brief 基于相关性生成建议
     * @param correlations 相关性矩阵
     * @return 建议列表
     */
    QList<IntelligentRecommendation> generateCorrelationBasedRecommendations(const QJsonObject& correlations) const;

    /**
     * @brief 计算预测置信度
     * @param hoursAhead 预测小时数
     * @return 置信度
     */
    double calculatePredictionConfidence(int hoursAhead) const;

    /**
     * @brief 识别风险因素
     * @param predictedData 预测数据
     * @return 风险因素列表
     */
    QStringList identifyRiskFactors(const QVector<DataPoint>& predictedData) const;

    /**
     * @brief 识别优化机会
     * @param predictedData 预测数据
     * @return 优化机会列表
     */
    QStringList identifyOptimizationOpportunities(const QVector<DataPoint>& predictedData) const;

    /**
     * @brief 在线更新模型
     * @param dataPoint 新数据点
     */
    void updateModelOnline(const DataPoint& dataPoint);

private:
    // 数据存储
    QVector<DataPoint> m_dataHistory;
    mutable QMutex m_dataMutex;
    
    // 分析参数
    double m_learningRate;
    int m_windowSize;
    double m_sensitivityThreshold;
    int m_maxHistorySize;
    
    // 模型参数
    QJsonObject m_modelParameters;
    bool m_modelTrained;
    
    // 定时器
    QTimer* m_analysisTimer;
    bool m_isRunning;
    
    // 缓存的分析结果
    QList<TrendAnalysis> m_lastTrends;
    QList<AnomalyDetection> m_lastAnomalies;
    QList<IntelligentRecommendation> m_lastRecommendations;
    double m_lastHealthScore;
    
    // 统计信息
    int m_totalAnalyses;
    int m_anomaliesDetected;
    int m_recommendationsGenerated;
    QDateTime m_lastAnalysisTime;
};

#endif // INTELLIGENTANALYZER_H