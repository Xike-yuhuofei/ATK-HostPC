#pragma once

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>
#include <QTimer>
#include <QDateTime>
#include <QAtomicInt>
#include "constants.h"
#include "protocolparser.h"

// 数据处理任务类型
enum class DataProcessType {
    ParseFrame,
    ProcessSensorData,
    CalculateStatistics,
    UpdateDatabase,
    GenerateReport
};

// 数据处理任务
struct DataProcessTask {
    DataProcessType type;
    QByteArray data;
    QDateTime timestamp;
    int priority;
    QVariant customData;
    
    DataProcessTask(DataProcessType t = DataProcessType::ParseFrame, 
                   const QByteArray& d = QByteArray(), 
                   int p = 0)
        : type(t), data(d), timestamp(QDateTime::currentDateTime()), priority(p)
    {}
};

// 数据处理工作线程
class DataProcessWorker : public QObject
{
    Q_OBJECT

public:
    explicit DataProcessWorker(QObject* parent = nullptr);
    ~DataProcessWorker();
    
    // 线程管理
    void startProcessing();
    void stopProcessing();
    void pauseProcessing();
    void resumeProcessing();
    
    // 任务管理
    void addTask(const DataProcessTask& task);
    void addHighPriorityTask(const DataProcessTask& task);
    void clearTasks();
    
    // 性能监控
    int getQueueSize() const;
    qint64 getProcessedTaskCount() const;
    double getAverageProcessTime() const;
    bool isProcessing() const;
    
    // 配置
    void setMaxQueueSize(int maxSize);
    void setWorkerThreadCount(int threadCount);
    void setBatchSize(int batchSize);

signals:
    // 处理完成信号
    void frameProcessed(const ProtocolFrame& frame);
    void statisticsUpdated(const QVariantMap& stats);
    void dataProcessed(const QByteArray& processedData);
    void errorOccurred(const QString& error);
    
    // 性能信号
    void performanceUpdated(qint64 processedCount, double avgTime);
    void queueStatusChanged(int queueSize, bool overloaded);

public slots:
    void processData(const QByteArray& data);
    void processFrame(const ProtocolFrame& frame);
    void processStatistics();

private slots:
    void onProcessingTimer();
    void onPerformanceTimer();

private:
    void processTasks();
    void processTask(const DataProcessTask& task);
    void processParseFrame(const DataProcessTask& task);
    void processSensorData(const DataProcessTask& task);
    void processCalculateStats(const DataProcessTask& task);
    void processUpdateDatabase(const DataProcessTask& task);
    void processGenerateReport(const DataProcessTask& task);
    
    // 性能优化
    void optimizeQueue();
    void balanceLoad();
    
    // 线程控制
    QThread* m_workerThread;
    mutable QMutex m_taskMutex;
    QWaitCondition m_taskCondition;
    QQueue<DataProcessTask> m_taskQueue;
    QAtomicInt m_running;
    QAtomicInt m_paused;
    
    // 性能统计
    qint64 m_processedTaskCount;
    qint64 m_totalProcessTime;
    double m_averageProcessTime;
    QDateTime m_lastPerformanceUpdate;
    
    // 配置参数
    int m_maxQueueSize;
    int m_workerThreadCount;
    int m_batchSize;
    
    // 定时器
    QTimer* m_processingTimer;
    QTimer* m_performanceTimer;
    
    // 处理器组件
    ProtocolParser* m_protocolParser;
    
    // 性能监控
    struct PerformanceMetrics {
        qint64 tasksPerSecond;
        qint64 bytesPerSecond;
        double cpuUsage;
        double memoryUsage;
        QDateTime lastUpdate;
    } m_metrics;
}; 