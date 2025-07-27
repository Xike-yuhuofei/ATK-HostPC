#ifndef PERFORMANCECONFIGMANAGER_H
#define PERFORMANCECONFIGMANAGER_H

#include <QObject>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>
#include <QMutex>
#include <QHash>
#include <QVariant>

/**
 * @brief 性能配置管理器类
 * 
 * 负责加载、管理和应用性能配置参数，
 * 提供实时配置更新和性能监控功能
 */
class PerformanceConfigManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 性能指标结构体
     */
    struct PerformanceMetrics {
        double memoryUsagePercent;
        double cpuUsagePercent;
        int databaseQueryTimeMs;
        int uiResponseTimeMs;
        int communicationLatencyMs;
        qint64 timestamp;
    };

    /**
     * @brief 配置参数结构体
     */
    struct OptimizationConfig {
        // 数据库连接池配置
        int dbMinConnections;
        int dbMaxConnections;
        int dbConnectionTimeoutMs;
        int dbIdleTimeoutMs;
        
        // 内存优化器配置
        bool memoryObjectPoolEnabled;
        bool memoryTrackingEnabled;
        bool memoryAutoCleanupEnabled;
        int memoryCleanupIntervalMs;
        int memoryThresholdMb;
        
        // UI更新优化器配置
        int uiMaxFps;
        int uiBatchSize;
        int uiUpdateIntervalMs;
        bool uiAdaptiveTuningEnabled;
        
        // 通信缓冲配置
        int commBufferSizeKb;
        int commMaxBuffers;
        int commTimeoutMs;
        bool commCompressionEnabled;
    };

    explicit PerformanceConfigManager(QObject *parent = nullptr);
    ~PerformanceConfigManager();

    /**
     * @brief 加载性能配置文件
     * @param configPath 配置文件路径
     * @return 是否加载成功
     */
    bool loadConfiguration(const QString &configPath);

    /**
     * @brief 获取优化配置参数
     * @return 优化配置结构体
     */
    OptimizationConfig getOptimizationConfig() const;

    /**
     * @brief 更新性能指标
     * @param metrics 性能指标数据
     */
    void updateMetrics(const PerformanceMetrics &metrics);

    /**
     * @brief 检查是否超过阈值
     * @param metrics 性能指标数据
     * @return 超过阈值的指标列表
     */
    QStringList checkThresholds(const PerformanceMetrics &metrics);

    /**
     * @brief 启动性能监控
     */
    void startMonitoring();

    /**
     * @brief 停止性能监控
     */
    void stopMonitoring();

    /**
     * @brief 获取配置值
     * @param key 配置键
     * @param defaultValue 默认值
     * @return 配置值
     */
    QVariant getConfigValue(const QString &key, const QVariant &defaultValue = QVariant()) const;

    /**
     * @brief 设置配置值
     * @param key 配置键
     * @param value 配置值
     */
    void setConfigValue(const QString &key, const QVariant &value);

    /**
     * @brief 保存配置到文件
     * @return 是否保存成功
     */
    bool saveConfiguration();

signals:
    /**
     * @brief 性能警告信号
     * @param metric 指标名称
     * @param value 当前值
     * @param threshold 阈值
     */
    void performanceWarning(const QString &metric, double value, double threshold);

    /**
     * @brief 配置更新信号
     * @param key 配置键
     * @param value 新值
     */
    void configurationUpdated(const QString &key, const QVariant &value);

    /**
     * @brief 监控数据更新信号
     * @param metrics 性能指标
     */
    void metricsUpdated(const PerformanceMetrics &metrics);

private slots:
    /**
     * @brief 定时监控槽函数
     */
    void onMonitoringTimer();

private:
    /**
     * @brief 解析JSON配置
     * @param jsonObj JSON对象
     */
    void parseConfiguration(const QJsonObject &jsonObj);

    /**
     * @brief 收集系统性能指标
     * @return 性能指标数据
     */
    PerformanceMetrics collectSystemMetrics();

    /**
     * @brief 发送警告通知
     * @param metric 指标名称
     * @param value 当前值
     * @param threshold 阈值
     */
    void sendAlert(const QString &metric, double value, double threshold);

private:
    QJsonObject m_config;                    ///< 配置JSON对象
    QString m_configPath;                    ///< 配置文件路径
    OptimizationConfig m_optimizationConfig; ///< 优化配置
    QTimer *m_monitoringTimer;               ///< 监控定时器
    mutable QMutex m_configMutex;            ///< 配置互斥锁
    QHash<QString, double> m_thresholds;     ///< 阈值映射
    QList<PerformanceMetrics> m_metricsHistory; ///< 历史指标数据
    bool m_monitoringEnabled;                ///< 监控是否启用
    int m_samplingIntervalMs;                ///< 采样间隔
};

#endif // PERFORMANCECONFIGMANAGER_H