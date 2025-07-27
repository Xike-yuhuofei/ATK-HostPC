#ifndef LOADBALANCER_H
#define LOADBALANCER_H

#include <QObject>
#include <QTimer>
#include <QThread>
#include <QThreadPool>
#include <QMutex>
#include <QQueue>
#include <QHash>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <functional>

class ContinuousOptimizer;
class IntelligentAnalyzer;

class LoadBalancer : public QObject
{
    Q_OBJECT

public:
    enum class TaskPriority {
        Low = 0,
        Normal = 1,
        High = 2,
        Critical = 3
    };
    
    enum class ResourceType {
        CPU = 0,
        Memory = 1,
        IO = 2,
        Network = 3,
        Database = 4
    };
    
    enum class BalancingStrategy {
        RoundRobin = 0,
        LeastLoaded = 1,
        WeightedRoundRobin = 2,
        ResourceBased = 3,
        Adaptive = 4
    };
    
    struct TaskInfo {
        QString id;
        QString name;
        TaskPriority priority;
        ResourceType primaryResource;
        QHash<ResourceType, double> resourceRequirements;
        std::function<void()> task;
        QDateTime submittedAt;
        QDateTime startedAt;
        QDateTime completedAt;
        int retryCount;
        bool completed;
        bool failed;
        QString errorMessage;
        double estimatedDuration;
        double actualDuration;
    };
    
    struct WorkerInfo {
        QString id;
        QThread* thread;
        bool busy;
        double cpuLoad;
        double memoryUsage;
        int activeTasks;
        int completedTasks;
        int failedTasks;
        QDateTime lastTaskCompleted;
        QHash<ResourceType, double> capabilities;
        QHash<ResourceType, double> currentLoad;
        double efficiency;
        bool enabled;
    };
    
    struct ResourceMetrics {
        ResourceType type;
        double totalCapacity;
        double currentUsage;
        double averageUsage;
        double peakUsage;
        double utilizationRate;
        QDateTime lastUpdated;
        QList<double> usageHistory;
    };
    
    struct BalancingMetrics {
        int totalTasks;
        int completedTasks;
        int failedTasks;
        int queuedTasks;
        double averageWaitTime;
        double averageExecutionTime;
        double throughput;
        double efficiency;
        QDateTime lastUpdated;
        QHash<TaskPriority, int> tasksByPriority;
        QHash<ResourceType, ResourceMetrics> resourceMetrics;
    };

explicit LoadBalancer(QObject *parent = nullptr);
    ~LoadBalancer();
    
    // 初始化和配置
    bool initialize(ContinuousOptimizer* optimizer = nullptr, IntelligentAnalyzer* analyzer = nullptr);
    void setBalancingStrategy(BalancingStrategy strategy);
    void setMaxWorkers(int maxWorkers);
    void setTaskTimeout(int timeoutMs);
    
    // 工作线程管理
    bool addWorker(const QString& workerId, const QHash<ResourceType, double>& capabilities);
    bool removeWorker(const QString& workerId);
    void enableWorker(const QString& workerId, bool enabled);
    QList<WorkerInfo> getWorkers() const;
    
    // 任务管理
    QString submitTask(const QString& taskName, TaskPriority priority, 
                      ResourceType primaryResource, 
                      const QHash<ResourceType, double>& requirements,
                      std::function<void()> task);
    bool cancelTask(const QString& taskId);
    bool retryTask(const QString& taskId);
    TaskInfo getTaskInfo(const QString& taskId) const;
    QList<TaskInfo> getQueuedTasks() const;
    QList<TaskInfo> getActiveTasks() const;
    QList<TaskInfo> getCompletedTasks(int limit = 100) const;
    
    // 负载均衡控制
    void startBalancing();
    void stopBalancing();
    bool isBalancing() const;
    
    // 资源监控
    void updateResourceUsage(ResourceType type, double usage);
    ResourceMetrics getResourceMetrics(ResourceType type) const;
    QHash<ResourceType, ResourceMetrics> getAllResourceMetrics() const;
    
    // 性能监控
    BalancingMetrics getBalancingMetrics() const;
    QJsonObject getPerformanceReport() const;
    QJsonObject getWorkerStatistics() const;
    
    // 优化建议
    QStringList getOptimizationSuggestions() const;
    void applyOptimizationRecommendations(const QJsonArray& recommendations);
    
    // 配置管理
    bool exportConfiguration(const QString& filePath) const;
    bool importConfiguration(const QString& filePath);
    void resetToDefaults();

signals:
    void taskSubmitted(const QString& taskId, const QString& taskName);
    void taskStarted(const QString& taskId, const QString& workerId);
    void taskCompleted(const QString& taskId, double duration);
    void taskFailed(const QString& taskId, const QString& error);
    void workerAdded(const QString& workerId);
    void workerRemoved(const QString& workerId);
    void resourceThresholdExceeded(ResourceType type, double usage);
    void balancingOptimized(const QString& strategy, double improvement);
    void performanceAlert(const QString& message, int severity);

public slots:
    void optimizeBalancing();
    void rebalanceTasks();
    void updateWorkerMetrics();
    void cleanupCompletedTasks();
    void handleResourceAlert(ResourceType type, double usage);

private slots:
    void processTaskQueue();
    void monitorWorkers();
    void updateMetrics();
    void handleTaskTimeout();
    void analyzePerformance();

private:
    // 核心组件
    ContinuousOptimizer* m_optimizer;
    IntelligentAnalyzer* m_analyzer;
    
    // 定时器
    QTimer* m_balancingTimer;
    QTimer* m_monitoringTimer;
    QTimer* m_metricsTimer;
    QTimer* m_cleanupTimer;
    
    // 线程池和工作线程
    QThreadPool* m_threadPool;
    QHash<QString, WorkerInfo> m_workers;
    mutable QMutex m_workersMutex;
    
    // 任务管理
    QQueue<TaskInfo> m_taskQueue;
    QHash<QString, TaskInfo> m_activeTasks;
    QHash<QString, TaskInfo> m_completedTasks;
    mutable QMutex m_tasksMutex;
    
    // 资源监控
    QHash<ResourceType, ResourceMetrics> m_resourceMetrics;
    mutable QMutex m_resourceMutex;
    
    // 配置参数
    BalancingStrategy m_strategy;
    int m_maxWorkers;
    int m_taskTimeout;
    int m_balancingInterval;
    int m_monitoringInterval;
    int m_metricsInterval;
    int m_cleanupInterval;
    int m_maxQueueSize;
    int m_maxRetries;
    double m_resourceThreshold;
    bool m_isRunning;
    
    // 性能指标
    BalancingMetrics m_metrics;
    QDateTime m_startTime;
    int m_taskIdCounter;
    
    // 私有方法
    QString generateTaskId();
    WorkerInfo* selectWorker(const TaskInfo& task);
    WorkerInfo* selectWorkerRoundRobin();
    WorkerInfo* selectWorkerLeastLoaded();
    WorkerInfo* selectWorkerWeighted();
    WorkerInfo* selectWorkerResourceBased(const TaskInfo& task);
    WorkerInfo* selectWorkerAdaptive(const TaskInfo& task);
    
    void executeTask(const TaskInfo& task, WorkerInfo* worker);
    void updateWorkerLoad(const QString& workerId, const QHash<ResourceType, double>& load);
    void updateTaskMetrics(const TaskInfo& task);
    void updateResourceMetrics(ResourceType type, double usage, bool updateHistory);
    
    double calculateWorkerScore(const WorkerInfo& worker, const TaskInfo& task) const;
    double calculateResourceUtilization(ResourceType type) const;
    double calculateSystemEfficiency() const;
    
    void optimizeWorkerAllocation();
    void adjustBalancingStrategy();
    void predictResourceNeeds();
    void generatePerformanceInsights();
    
    void saveBalancingState();
    void loadBalancingState();
    void initializeDefaultWorkers();
    void setupResourceMonitoring();
};

#endif // LOADBALANCER_H