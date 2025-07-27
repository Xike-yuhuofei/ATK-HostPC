#ifndef MLPERFORMANCEPREDICTOR_H
#define MLPERFORMANCEPREDICTOR_H

#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QVector>
#include <QHash>
#include <QQueue>
#include <functional>

class ContinuousOptimizer;
class IntelligentAnalyzer;
class LoadBalancer;

class MLPerformancePredictor : public QObject
{
    Q_OBJECT

public:
    enum class PredictionType {
        Performance = 0,
        ResourceUsage = 1,
        Bottleneck = 2,
        Optimization = 3,
        Anomaly = 4
    };
    
    enum class ModelType {
        LinearRegression = 0,
        PolynomialRegression = 1,
        MovingAverage = 2,
        ExponentialSmoothing = 3,
        NeuralNetwork = 4,
        Ensemble = 5
    };
    
    enum class FeatureType {
        Temporal = 0,
        Statistical = 1,
        Trend = 2,
        Seasonal = 3,
        Correlation = 4
    };
    
    struct DataPoint {
        QDateTime timestamp;
        QHash<QString, double> features;
        QHash<QString, double> targets;
        double weight;
        bool validated;
        QString source;
    };
    
    struct FeatureDefinition {
        QString name;
        FeatureType type;
        double importance;
        double minValue;
        double maxValue;
        double meanValue;
        double stdDeviation;
        bool normalized;
        QString description;
    };
    
    struct ModelConfiguration {
        ModelType type;
        QString name;
        QHash<QString, QVariant> parameters;
        QStringList inputFeatures;
        QStringList outputTargets;
        double accuracy;
        double confidence;
        QDateTime trainedAt;
        QDateTime lastUsed;
        int trainingDataSize;
        bool enabled;
    };
    
    struct PredictionResult {
        PredictionType type;
        QDateTime timestamp;
        QDateTime predictedFor;
        QHash<QString, double> predictions;
        QHash<QString, double> confidence;
        QHash<QString, double> bounds;
        QString modelUsed;
        double accuracy;
        QString explanation;
        QJsonObject metadata;
    };
    
    struct OptimizationRecommendation {
        QString id;
        QString category;
        QString title;
        QString description;
        QHash<QString, QVariant> parameters;
        double expectedImprovement;
        double confidence;
        int priority;
        QString impact;
        QString effort;
        QDateTime timestamp;
        QDateTime validUntil;
        bool applied;
        QString reasoning;
        QStringList actions;
        QHash<QString, double> metrics;
        QJsonArray supportingData;
    };
    
    struct AnomalyDetection {
        QDateTime timestamp;
        QString metric;
        double value;
        double expectedValue;
        double deviation;
        double severity;
        QString description;
        QString possibleCause;
        QStringList recommendations;
        bool confirmed;
    };
    
    struct TrainingMetrics {
        QString modelName;
        int epochs;
        double trainingLoss;
        double validationLoss;
        double accuracy;
        double precision;
        double recall;
        double f1Score;
        QDateTime startTime;
        QDateTime endTime;
        int dataPoints;
        QString status;
    };

explicit MLPerformancePredictor(QObject *parent = nullptr);
    ~MLPerformancePredictor();
    
    // 初始化和配置
    bool initialize(ContinuousOptimizer* optimizer = nullptr, 
                   IntelligentAnalyzer* analyzer = nullptr,
                   LoadBalancer* balancer = nullptr);
    void setModelConfiguration(const ModelConfiguration& config);
    void enableModel(const QString& modelName, bool enabled);
    
    // 数据管理
    void addDataPoint(const DataPoint& dataPoint);
    void addDataPoints(const QList<DataPoint>& dataPoints);
    void removeOldData(const QDateTime& cutoffTime);
    void clearTrainingData();
    int getDataPointCount() const;
    
    // 特征工程
    bool registerFeature(const FeatureDefinition& feature);
    void updateFeatureImportance(const QString& featureName, double importance);
    QList<FeatureDefinition> getFeatures() const;
    QHash<QString, double> extractFeatures(const QList<DataPoint>& data) const;
    
    // 模型训练
    bool trainModel(const QString& modelName, const QList<DataPoint>& trainingData);
    bool trainAllModels();
    void scheduleRetraining(const QString& modelName, int intervalHours);
    TrainingMetrics getTrainingMetrics(const QString& modelName) const;
    
    // 预测功能
    PredictionResult predict(PredictionType type, const QDateTime& targetTime, 
                           const QHash<QString, double>& currentState = QHash<QString, double>());
    QList<PredictionResult> predictMultiple(PredictionType type, 
                                           const QList<QDateTime>& targetTimes,
                                           const QHash<QString, double>& currentState = QHash<QString, double>());
    PredictionResult predictPerformance(int minutesAhead = 30);
    PredictionResult predictResourceUsage(const QString& resourceType, int minutesAhead = 30);
    
    // 异常检测
    void enableAnomalyDetection(bool enabled);
    QList<AnomalyDetection> detectAnomalies(const QList<DataPoint>& data);
    AnomalyDetection checkForAnomaly(const DataPoint& dataPoint);
    void setAnomalyThreshold(double threshold);
    
    // 优化建议
    QList<OptimizationRecommendation> generateOptimizationRecommendations();
    OptimizationRecommendation generateSpecificRecommendation(const QString& category);
    void applyRecommendation(const QString& recommendationId);
    void markRecommendationApplied(const QString& recommendationId, bool applied);
    
    // 模型评估
    double evaluateModel(const QString& modelName, const QList<DataPoint>& testData);
    QJsonObject getModelPerformanceReport(const QString& modelName) const;
    QJsonObject getAllModelsReport() const;
    
    // 预测历史
    QList<PredictionResult> getPredictionHistory(PredictionType type, int limit = 100) const;
    void validatePrediction(const QString& predictionId, const QHash<QString, double>& actualValues);
    double getPredictionAccuracy(const QString& modelName) const;
    
    // 配置管理
    bool exportModels(const QString& directoryPath) const;
    bool importModels(const QString& directoryPath);
    bool saveConfiguration(const QString& filePath) const;
    bool loadConfiguration(const QString& filePath);
    void resetToDefaults();
    
    // 实时监控
    void startPrediction();
    void stopPrediction();
    bool isPredicting() const;
    void setPredictionInterval(int intervalMs);

signals:
    void dataPointAdded(const DataPoint& dataPoint);
    void modelTrained(const QString& modelName, double accuracy);
    void predictionGenerated(const PredictionResult& result);
    void anomalyDetected(const AnomalyDetection& anomaly);
    void recommendationGenerated(const OptimizationRecommendation& recommendation);
    void modelPerformanceChanged(const QString& modelName, double newAccuracy);
    void trainingCompleted(const QString& modelName, const TrainingMetrics& metrics);
    void predictionValidated(const QString& predictionId, double accuracy);

public slots:
    void performPrediction();
    void retrainModels();
    void updateFeatureImportance();
    void cleanupOldData();
    void validateRecentPredictions();
    void optimizeModelParameters();

private slots:
    void performPeriodicPrediction();
    void performPeriodicTraining();
    void performAnomalyCheck();
    void updateModelMetrics();
    void generatePeriodicRecommendations();

private:
    // 核心组件
    ContinuousOptimizer* m_optimizer;
    IntelligentAnalyzer* m_analyzer;
    LoadBalancer* m_balancer;
    
    // 定时器
    QTimer* m_predictionTimer;
    QTimer* m_trainingTimer;
    QTimer* m_anomalyTimer;
    QTimer* m_metricsTimer;
    QTimer* m_recommendationTimer;
    
    // 数据存储
    QList<DataPoint> m_trainingData;
    QHash<QString, FeatureDefinition> m_features;
    QHash<QString, ModelConfiguration> m_models;
    QList<PredictionResult> m_predictionHistory;
    QList<OptimizationRecommendation> m_recommendations;
    QList<AnomalyDetection> m_anomalies;
    QHash<QString, TrainingMetrics> m_trainingMetrics;
    mutable QMutex m_dataMutex;
    mutable QMutex m_modelMutex;
    mutable QMutex m_predictionMutex;
    
    // 配置参数
    bool m_isPredicting;
    bool m_anomalyDetectionEnabled;
    int m_predictionInterval;
    int m_trainingInterval;
    int m_anomalyCheckInterval;
    int m_metricsUpdateInterval;
    int m_recommendationInterval;
    int m_maxDataPoints;
    int m_maxPredictionHistory;
    double m_anomalyThreshold;
    double m_confidenceThreshold;
    
    // 性能指标
    QDateTime m_startTime;
    int m_totalPredictions;
    int m_accuratePredictions;
    int m_totalAnomalies;
    int m_confirmedAnomalies;
    
    // 私有方法 - 数据处理
    void preprocessData(QList<DataPoint>& data) const;
    void normalizeFeatures(QList<DataPoint>& data) const;
    QHash<QString, double> calculateStatistics(const QList<DataPoint>& data, const QString& feature) const;
    void updateFeatureStatistics();
    
    // 私有方法 - 特征工程
    QHash<QString, double> extractTemporalFeatures(const QList<DataPoint>& data) const;
    QHash<QString, double> extractStatisticalFeatures(const QList<DataPoint>& data) const;
    QHash<QString, double> extractTrendFeatures(const QList<DataPoint>& data) const;
    QHash<QString, double> extractSeasonalFeatures(const QList<DataPoint>& data) const;
    QHash<QString, double> extractCorrelationFeatures(const QList<DataPoint>& data) const;
    
    // 私有方法 - 模型实现
    PredictionResult predictWithLinearRegression(const QString& modelName, 
                                                const QHash<QString, double>& features) const;
    PredictionResult predictWithPolynomialRegression(const QString& modelName, 
                                                    const QHash<QString, double>& features) const;
    PredictionResult predictWithMovingAverage(const QString& modelName, 
                                             const QList<DataPoint>& recentData) const;
    PredictionResult predictWithExponentialSmoothing(const QString& modelName, 
                                                    const QList<DataPoint>& recentData) const;
    PredictionResult predictWithNeuralNetwork(const QString& modelName, 
                                             const QHash<QString, double>& features) const;
    PredictionResult predictWithEnsemble(const QHash<QString, double>& features) const;
    
    // 私有方法 - 训练算法
    bool trainLinearRegression(const QString& modelName, const QList<DataPoint>& data);
    bool trainPolynomialRegression(const QString& modelName, const QList<DataPoint>& data);
    bool trainMovingAverage(const QString& modelName, const QList<DataPoint>& data);
    bool trainExponentialSmoothing(const QString& modelName, const QList<DataPoint>& data);
    bool trainNeuralNetwork(const QString& modelName, const QList<DataPoint>& data);
    
    // 私有方法 - 异常检测
    double calculateAnomalyScore(const DataPoint& dataPoint) const;
    bool isAnomaly(double score) const;
    QString identifyAnomalyCause(const AnomalyDetection& anomaly) const;
    QStringList generateAnomalyRecommendations(const AnomalyDetection& anomaly) const;
    
    // 私有方法 - 优化建议
    OptimizationRecommendation generatePerformanceRecommendation() const;
    OptimizationRecommendation generateResourceRecommendation() const;
    OptimizationRecommendation generateBottleneckRecommendation() const;
    OptimizationRecommendation generateConfigurationRecommendation() const;
    double calculateExpectedImprovement(const OptimizationRecommendation& recommendation) const;
    
    // 私有方法 - 工具函数
    QString generatePredictionId() const;
    QString generateRecommendationId() const;
    double calculateMSE(const QList<double>& predicted, const QList<double>& actual) const;
    double calculateMAE(const QList<double>& predicted, const QList<double>& actual) const;
    double calculateR2Score(const QList<double>& predicted, const QList<double>& actual) const;
    QList<DataPoint> getRecentData(int minutes) const;
    void saveModelState();
    void loadModelState();
    void initializeDefaultModels();
    void initializeDefaultFeatures();
    
    // 私有方法 - 数学计算
    double calculateLinearTrend(const QList<double>& values) const;
    double calculateVolatility(const QList<double>& values) const;
    double calculateCorrelation(const QList<double>& x, const QList<double>& y) const;
};

#endif // MLPERFORMANCEPREDICTOR_H