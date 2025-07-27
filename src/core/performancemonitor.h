#pragma once

#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QDateTime>
#include <QMap>
#include <QQueue>
#include <QThread>
#include <QProcess>
#include <functional>

// 前向声明
class MemoryOptimizer;
struct MemoryStatistics;
struct MemoryOptimizerConfig;

/**
 * @brief 性能指标结构
 */
struct PerformanceMetrics {
    QDateTime timestamp;
    
    // CPU指标
    double cpuUsage;          // CPU使用率 (%)
    double cpuTemperature;    // CPU温度 (°C)
    
    // 内存指标
    qint64 memoryUsed;        // 已使用内存 (bytes)
    qint64 memoryTotal;       // 总内存 (bytes)
    double memoryUsage;       // 内存使用率 (%)
    
    // 磁盘指标
    qint64 diskUsed;          // 已使用磁盘空间 (bytes)
    qint64 diskTotal;         // 总磁盘空间 (bytes)
    double diskUsage;         // 磁盘使用率 (%)
    
    // 网络指标
    qint64 networkBytesIn;    // 网络接收字节数
    qint64 networkBytesOut;   // 网络发送字节数
    
    // 应用程序指标
    qint64 appMemoryUsage;    // 应用程序内存使用 (bytes)
    double appCpuUsage;       // 应用程序CPU使用率 (%)
    int threadCount;          // 线程数量
    int handleCount;          // 句柄数量
    
    // 自定义指标
    QMap<QString, double> customMetrics;
};

/**
 * @brief 性能告警配置
 */
struct PerformanceAlert {
    QString name;
    QString metric;           // 监控的指标名称
    double threshold;         // 告警阈值
    bool enabled;            // 是否启用
    int duration;            // 持续时间(秒)
    QString action;          // 告警动作
};

/**
 * @brief 性能监控回调函数类型
 */
using PerformanceCallback = std::function<void(const PerformanceMetrics&)>;
using AlertCallback = std::function<void(const PerformanceAlert&, const PerformanceMetrics&)>;

/**
 * @brief 性能监控器
 * 
 * 提供系统和应用程序性能监控功能，包括：
 * - CPU、内存、磁盘、网络监控
 * - 性能指标收集和历史记录
 * - 性能告警和阈值监控
 * - 性能数据导出和分析
 * - 自定义性能指标支持
 */
class PerformanceMonitor : public QObject
{
    Q_OBJECT

public:
    static PerformanceMonitor* getInstance();
    
    // 监控控制
    void startMonitoring();
    void stopMonitoring();
    void pauseMonitoring();
    void resumeMonitoring();
    bool isMonitoring() const { return monitoring; }
    
    // 监控配置
    void setMonitoringInterval(int intervalMs);
    void setHistorySize(int size);
    void setMetricsEnabled(const QStringList& metrics);
    
    // 性能指标获取
    PerformanceMetrics getCurrentMetrics() const;
    QList<PerformanceMetrics> getHistoryMetrics(int count = 100) const;
    QList<PerformanceMetrics> getMetricsByTimeRange(const QDateTime& start, const QDateTime& end) const;
    
    // 自定义指标
    void addCustomMetric(const QString& name, double value);
    void removeCustomMetric(const QString& name);
    QMap<QString, double> getCustomMetrics() const;
    
    // 性能告警
    void addAlert(const PerformanceAlert& alert);
    void removeAlert(const QString& name);
    void updateAlert(const PerformanceAlert& alert);
    QList<PerformanceAlert> getAlerts() const;
    void setAlertsEnabled(bool enabled);
    
    // 回调管理
    void registerCallback(const QString& name, PerformanceCallback callback);
    void unregisterCallback(const QString& name);
    void registerAlertCallback(const QString& name, AlertCallback callback);
    void unregisterAlertCallback(const QString& name);
    
    // 数据导出
    bool exportMetrics(const QString& filename, const QDateTime& start = QDateTime(), 
                      const QDateTime& end = QDateTime()) const;
    bool exportAlerts(const QString& filename) const;
    
    // 统计分析
    PerformanceMetrics getAverageMetrics(const QDateTime& start, const QDateTime& end) const;
    PerformanceMetrics getMaxMetrics(const QDateTime& start, const QDateTime& end) const;
    PerformanceMetrics getMinMetrics(const QDateTime& start, const QDateTime& end) const;
    
    // 性能优化建议
    QStringList getOptimizationSuggestions() const;
    
    // 内存优化管理
    bool initializeMemoryOptimizer(const MemoryOptimizerConfig& config);
    MemoryStatistics getMemoryOptimizerStatistics() const;
    void setMemoryOptimizerConfiguration(const MemoryOptimizerConfig& config);
    bool isMemoryOptimizerEnabled() const;
    void enableMemoryOptimizer(bool enabled);
    void triggerMemoryOptimization();
    void setMemoryThreshold(double threshold);
    
signals:
    void metricsUpdated(const PerformanceMetrics& metrics);
    void alertTriggered(const PerformanceAlert& alert, const PerformanceMetrics& metrics);
    void monitoringStarted();
    void monitoringStopped();
    void performanceIssueDetected(const QString& issue);
    
private slots:
    void collectMetrics();
    void checkAlerts();
    void processMetricsQueue();
    
private:
    explicit PerformanceMonitor(QObject* parent = nullptr);
    ~PerformanceMonitor();
    
    // 系统指标收集
    double getCpuUsage();
    double getCpuTemperature();
    void getMemoryInfo(qint64& used, qint64& total);
    void getDiskInfo(qint64& used, qint64& total);
    void getNetworkInfo(qint64& bytesIn, qint64& bytesOut);
    
    // 应用程序指标收集
    qint64 getAppMemoryUsage();
    double getAppCpuUsage();
    int getThreadCount();
    int getHandleCount();
    
    // 告警处理
    void processAlert(const PerformanceAlert& alert, const PerformanceMetrics& metrics);
    bool checkAlertCondition(const PerformanceAlert& alert, const PerformanceMetrics& metrics);
    
    // 回调通知
    void notifyCallbacks(const PerformanceMetrics& metrics);
    void notifyAlertCallbacks(const PerformanceAlert& alert, const PerformanceMetrics& metrics);
    
    static PerformanceMonitor* instance;
    static QMutex instanceMutex;
    
    mutable QMutex metricsMutex;
    QTimer* collectTimer;
    QTimer* alertTimer;
    QTimer* processTimer;
    
    bool monitoring;
    bool paused;
    bool alertsEnabled;
    int monitoringInterval;
    int historySize;
    
    // 性能数据
    QQueue<PerformanceMetrics> metricsQueue;
    QList<PerformanceMetrics> metricsHistory;
    PerformanceMetrics currentMetrics;
    
    // 自定义指标
    QMap<QString, double> customMetrics;
    
    // 告警配置
    QList<PerformanceAlert> alerts;
    QMap<QString, QDateTime> alertLastTriggered;
    
    // 回调函数
    QMap<QString, PerformanceCallback> callbacks;
    QMap<QString, AlertCallback> alertCallbacks;
    
    // 启用的指标
    QStringList enabledMetrics;
    
    // 内存优化器
    MemoryOptimizer* m_memoryOptimizer;
    double m_memoryThreshold;
    
    // 系统信息缓存
    QProcess* systemProcess;
    qint64 lastNetworkBytesIn;
    qint64 lastNetworkBytesOut;
    QDateTime lastNetworkCheck;
    
    // 禁用拷贝构造和赋值
    PerformanceMonitor(const PerformanceMonitor&) = delete;
    PerformanceMonitor& operator=(const PerformanceMonitor&) = delete;
};

// 便捷宏定义
#define PERF_MONITOR PerformanceMonitor::getInstance()
#define ADD_CUSTOM_METRIC(name, value) PerformanceMonitor::getInstance()->addCustomMetric(name, value)
#define GET_CURRENT_METRICS() PerformanceMonitor::getInstance()->getCurrentMetrics()