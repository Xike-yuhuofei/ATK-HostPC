#pragma once

#include <QObject>
#include <QTimer>
#include <QHash>
#include <QMutex>
#include <QVariant>
#include <QDateTime>
#include <QElapsedTimer>
#include <QAtomicInt>
#include <QThread>
#include <QJsonObject>
#include <functional>

// 前向声明
class DataCacheManager;
struct CacheStatistics;

// 缓存配置结构体
struct CacheConfig {
    bool enabled;
    qint64 maxSize;
    int maxEntries;
    int ttl;
    bool compressionEnabled;
    bool persistentCache;
    int cachePolicy;
};

// UI更新性能指标结构体
struct UIPerformanceMetrics {
    QAtomicInt totalUpdates;
    QAtomicInt totalUpdateTime;
    double averageUpdateTime;
    QAtomicInt updatesPerSecond;
    QDateTime lastUpdate;
    QElapsedTimer performanceTimer;
    QAtomicInt droppedUpdates;
    QAtomicInt coalescedUpdates;
    double cpuUsage;
    qint64 memoryUsage;
};

// 优化配置结构体
struct OptimizationConfig {
    int maxUpdatesPerSecond;
    int lowPriorityDelay;
    int highPriorityThreshold;
    bool enableFrameRateLimit;
    bool enableAdaptiveInterval;
    bool enableCoalescing;
    bool enableThreadOptimization;
    double cpuThreshold;
    qint64 memoryThreshold;
    int adaptiveWindowSize;
};

// UI更新类型
enum class UIUpdateType {
    StatusBar,
    ProgressBar,
    ChartData,
    TableData,
    Statistics,
    RealTimeData,
    ErrorMessage,
    LogMessage,
    Animation,
    Layout
};

// 渲染优化策略枚举
enum class RenderStrategy {
    Immediate,      // 立即渲染
    Batched,        // 批量渲染
    Deferred,       // 延迟渲染
    Adaptive        // 自适应渲染
};

// UI更新任务
struct UIUpdateTask {
    UIUpdateType type;
    QString widgetId;
    QVariant data;
    QDateTime timestamp;
    int priority;
    bool immediate;
    bool coalescing;                // 是否允许合并
    QThread* sourceThread;          // 源线程
    std::function<void()> callback; // 回调函数
    
    UIUpdateTask(UIUpdateType t = UIUpdateType::StatusBar, 
                const QString& id = QString(), 
                const QVariant& d = QVariant(),
                int p = 0, 
                bool imm = false)
        : type(t), widgetId(id), data(d), timestamp(QDateTime::currentDateTime()), 
          priority(p), immediate(imm), coalescing(true), sourceThread(nullptr)
    {}
    
    bool operator==(const UIUpdateTask& other) const
    {
        return type == other.type && widgetId == other.widgetId && 
               priority == other.priority && immediate == other.immediate;
    }
};

// UI更新批处理器
class UIUpdateOptimizer : public QObject
{
    Q_OBJECT

public:
    explicit UIUpdateOptimizer(QObject* parent = nullptr);
    ~UIUpdateOptimizer();
    
    // 更新请求管理
    void requestUpdate(const UIUpdateTask& task);
    void requestImmediateUpdate(const UIUpdateTask& task);
    void requestBatchUpdate(const QList<UIUpdateTask>& tasks);
    
    // 配置管理
    void setUpdateInterval(UIUpdateType type, int intervalMs);
    void setRenderStrategy(UIUpdateType type, RenderStrategy strategy);
    void setMaxBatchSize(int maxSize);
    void setMaxQueueSize(int maxSize);
    void enableUpdateType(UIUpdateType type, bool enabled);
    void enableCoalescing(bool enabled);
    void setOptimizationConfig(const ::OptimizationConfig& config);
    
    // 性能控制
    void pauseUpdates();
    void resumeUpdates();
    void clearPendingUpdates();
    void optimizeUpdateFrequency();
    void adaptivePerformanceTuning();
    
    // 性能监控
    int getPendingUpdateCount() const;
    double getAverageUpdateTime() const;
    int getUpdateRate() const;
    UIPerformanceMetrics getPerformanceMetrics() const;
    QStringList getOptimizationSuggestions() const;
    
    // 注册更新回调
    void registerUpdateCallback(UIUpdateType type, const QString& widgetId, 
                               std::function<void(const QVariant&)> callback);
    void unregisterUpdateCallback(UIUpdateType type, const QString& widgetId);
    
    // 缓存管理
    bool initializeCacheManager(const CacheConfig& config);
    QJsonObject getCacheStatistics() const;
    void setCacheConfiguration(const CacheConfig& config);
    bool isCacheEnabled() const;
    void enableCache(bool enabled);
    void clearCache();
    void setCacheSize(qint64 size);
    QVariant getCachedData(const QString& key) const;
    void setCachedData(const QString& key, const QVariant& data);

signals:
    void updateRequired(const UIUpdateTask& task);
    void batchUpdateRequired(const QList<UIUpdateTask>& tasks);
    void performanceWarning(const QString& message);
    void updateStatistics(int pendingCount, double avgTime, int rate);
    void optimizationSuggestion(const QStringList& suggestions);
    void renderStrategyChanged(UIUpdateType type, RenderStrategy strategy);

private slots:
    void processUpdates();
    void onPerformanceTimer();
    void onOptimizationTimer();
    void onAdaptiveTuning();

private:
    void processBatchUpdates();
    void executeUpdate(const UIUpdateTask& task);
    void optimizeQueue();
    void updatePerformanceMetrics();
    
    // 智能更新策略
    bool shouldSkipUpdate(const UIUpdateTask& task);
    bool shouldBatchUpdate(const UIUpdateTask& task);
    void mergeUpdates(QList<UIUpdateTask>& tasks);
    QList<UIUpdateTask> coalesceUpdates(const QList<UIUpdateTask>& tasks);
    void detectPerformanceBottlenecks();
    void updateSystemResourceUsage();
    int calculateAdaptiveInterval(UIUpdateType type);
    void adaptiveAdjustment();
    void checkSystemLoad();
    void clearLowPriorityUpdates();
    
    // 定时器管理
    QTimer* m_updateTimer;
    QTimer* m_performanceTimer;
    QTimer* m_optimizationTimer;
    QTimer* m_adaptiveTimer;
    
    // 更新队列
    QList<UIUpdateTask> m_updateQueue;
    QList<UIUpdateTask> m_highPriorityQueue;
    QHash<QString, UIUpdateTask> m_lastUpdates; // 用于去重
    mutable QMutex m_queueMutex;
    
    // 配置参数
    QHash<UIUpdateType, int> m_updateIntervals;
    QHash<UIUpdateType, bool> m_enabledTypes;
    QHash<UIUpdateType, RenderStrategy> m_renderStrategies;
    QHash<UIUpdateType, QList<qint64>> m_performanceHistory;
    int m_maxBatchSize;
    int m_maxQueueSize;
    bool m_paused;
    
    // 性能统计
    UIPerformanceMetrics m_metrics;
    
    // 更新回调
    QHash<QString, std::function<void(const QVariant&)>> m_updateCallbacks;
    
    // 智能优化参数
    ::OptimizationConfig m_optimizationConfig;
    
    // 帧率限制和自适应优化
    QElapsedTimer m_frameTimer;
    int m_targetFPS;
    int m_currentFPS;
    QList<double> m_recentUpdateTimes;
    QDateTime m_lastOptimization;
    bool m_adaptiveMode;
    
    // 缓存管理器
    DataCacheManager* m_cacheManager;
};