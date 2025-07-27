#include "dataprocessworker.h"
#include "logger/logmanager.h"
#include <QElapsedTimer>
#include <QCoreApplication>
#include <QDebug>

DataProcessWorker::DataProcessWorker(QObject* parent)
    : QObject(parent)
    , m_workerThread(nullptr)
    , m_running(0)
    , m_paused(0)
    , m_processedTaskCount(0)
    , m_totalProcessTime(0)
    , m_averageProcessTime(0.0)
    , m_maxQueueSize(1000)
    , m_workerThreadCount(2)
    , m_batchSize(10)
    , m_processingTimer(nullptr)
    , m_performanceTimer(nullptr)
    , m_protocolParser(nullptr)
{
    // 初始化性能指标
    m_metrics.tasksPerSecond = 0;
    m_metrics.bytesPerSecond = 0;
    m_metrics.cpuUsage = 0.0;
    m_metrics.memoryUsage = 0.0;
    m_metrics.lastUpdate = QDateTime::currentDateTime();
    m_lastPerformanceUpdate = QDateTime::currentDateTime();
    
    // 创建协议解析器
    m_protocolParser = new ProtocolParser(this);
    
    // 创建定时器
    m_processingTimer = new QTimer(this);
    m_performanceTimer = new QTimer(this);
    
    // 连接定时器信号
    connect(m_processingTimer, &QTimer::timeout, this, &DataProcessWorker::onProcessingTimer);
    connect(m_performanceTimer, &QTimer::timeout, this, &DataProcessWorker::onPerformanceTimer);
    
    // 设置定时器间隔
    m_processingTimer->setInterval(10); // 10ms处理一次
    m_performanceTimer->setInterval(5000); // 5秒更新一次性能统计
    
    LogManager::getInstance()->info("数据处理工作线程已创建", "DataProcessWorker");
}

DataProcessWorker::~DataProcessWorker()
{
    stopProcessing();
    
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait(3000);
        delete m_workerThread;
    }
    
    LogManager::getInstance()->info("数据处理工作线程已销毁", "DataProcessWorker");
}

void DataProcessWorker::startProcessing()
{
    if (m_running.loadAcquire()) {
        return;
    }
    
    m_running.storeRelease(1);
    m_paused.storeRelease(0);
    
    // 创建工作线程
    if (!m_workerThread) {
        m_workerThread = new QThread(this);
        moveToThread(m_workerThread);
        m_workerThread->start();
    }
    
    // 启动定时器
    m_processingTimer->start();
    m_performanceTimer->start();
    
    LogManager::getInstance()->info("数据处理工作线程已启动", "DataProcessWorker");
}

void DataProcessWorker::stopProcessing()
{
    if (!m_running.loadAcquire()) {
        return;
    }
    
    m_running.storeRelease(0);
    m_paused.storeRelease(0);
    
    // 停止定时器
    m_processingTimer->stop();
    m_performanceTimer->stop();
    
    // 唤醒等待的线程
    m_taskCondition.wakeAll();
    
    LogManager::getInstance()->info("数据处理工作线程已停止", "DataProcessWorker");
}

void DataProcessWorker::pauseProcessing()
{
    if (!m_running.loadAcquire()) {
        return;
    }
    
    m_paused.storeRelease(1);
    LogManager::getInstance()->info("数据处理工作线程已暂停", "DataProcessWorker");
}

void DataProcessWorker::resumeProcessing()
{
    if (!m_running.loadAcquire()) {
        return;
    }
    
    m_paused.storeRelease(0);
    m_taskCondition.wakeAll();
    LogManager::getInstance()->info("数据处理工作线程已恢复", "DataProcessWorker");
}

void DataProcessWorker::addTask(const DataProcessTask& task)
{
    QMutexLocker locker(&m_taskMutex);
    
    // 检查队列是否已满
    if (m_taskQueue.size() >= m_maxQueueSize) {
        // 移除最旧的任务
        m_taskQueue.dequeue();
        LogManager::getInstance()->warning("任务队列已满，移除最旧任务", "DataProcessWorker");
    }
    
    // 按优先级插入任务
    bool inserted = false;
    for (int i = 0; i < m_taskQueue.size(); ++i) {
        if (task.priority > m_taskQueue[i].priority) {
            m_taskQueue.insert(i, task);
            inserted = true;
            break;
        }
    }
    
    if (!inserted) {
        m_taskQueue.enqueue(task);
    }
    
    // 唤醒处理线程
    m_taskCondition.wakeOne();
    
    // 检查队列状态
    bool overloaded = m_taskQueue.size() > m_maxQueueSize * 0.8;
    emit queueStatusChanged(m_taskQueue.size(), overloaded);
}

void DataProcessWorker::addHighPriorityTask(const DataProcessTask& task)
{
    DataProcessTask highPriorityTask = task;
    highPriorityTask.priority = 1000; // 高优先级
    addTask(highPriorityTask);
}

void DataProcessWorker::clearTasks()
{
    QMutexLocker locker(&m_taskMutex);
    m_taskQueue.clear();
    LogManager::getInstance()->info("任务队列已清空", "DataProcessWorker");
}

int DataProcessWorker::getQueueSize() const
{
    QMutexLocker locker(&m_taskMutex);
    return m_taskQueue.size();
}

qint64 DataProcessWorker::getProcessedTaskCount() const
{
    return m_processedTaskCount;
}

double DataProcessWorker::getAverageProcessTime() const
{
    return m_averageProcessTime;
}

bool DataProcessWorker::isProcessing() const
{
    return m_running.loadAcquire() && !m_paused.loadAcquire();
}

void DataProcessWorker::setMaxQueueSize(int maxSize)
{
    m_maxQueueSize = maxSize;
}

void DataProcessWorker::setWorkerThreadCount(int threadCount)
{
    m_workerThreadCount = threadCount;
}

void DataProcessWorker::setBatchSize(int batchSize)
{
    m_batchSize = batchSize;
}

void DataProcessWorker::processData(const QByteArray& data)
{
    DataProcessTask task(DataProcessType::ParseFrame, data);
    addTask(task);
}

void DataProcessWorker::processFrame(const ProtocolFrame& frame)
{
    DataProcessTask task(DataProcessType::ProcessSensorData);
    QByteArray frameData;
    QDataStream stream(&frameData, QIODevice::WriteOnly);
    stream << frame.header << static_cast<quint8>(frame.command) 
           << frame.dataLength << frame.data << frame.checksum << frame.tail;
    task.data = frameData;
    addTask(task);
}

void DataProcessWorker::processStatistics()
{
    DataProcessTask task(DataProcessType::CalculateStatistics);
    addTask(task);
}

void DataProcessWorker::onProcessingTimer()
{
    if (!m_running.loadAcquire() || m_paused.loadAcquire()) {
        return;
    }
    
    processTasks();
}

void DataProcessWorker::onPerformanceTimer()
{
    // 更新性能统计
    QDateTime now = QDateTime::currentDateTime();
    qint64 elapsedMs = m_lastPerformanceUpdate.msecsTo(now);
    
    if (elapsedMs > 0) {
        m_metrics.tasksPerSecond = (m_processedTaskCount * 1000) / elapsedMs;
        m_metrics.lastUpdate = now;
        
        emit performanceUpdated(m_processedTaskCount, m_averageProcessTime);
        
        // 重置统计
        m_lastPerformanceUpdate = now;
    }
}

void DataProcessWorker::processTasks()
{
    QMutexLocker locker(&m_taskMutex);
    
    int processedCount = 0;
    QElapsedTimer timer;
    timer.start();
    
    // 批量处理任务
    while (!m_taskQueue.isEmpty() && processedCount < m_batchSize) {
        DataProcessTask task = m_taskQueue.dequeue();
        locker.unlock();
        
        // 处理任务
        processTask(task);
        processedCount++;
        
        locker.relock();
    }
    
    // 更新性能统计
    if (processedCount > 0) {
        qint64 processingTime = timer.elapsed();
        m_processedTaskCount += processedCount;
        m_totalProcessTime += processingTime;
        m_averageProcessTime = static_cast<double>(m_totalProcessTime) / m_processedTaskCount;
    }
}

void DataProcessWorker::processTask(const DataProcessTask& task)
{
    try {
        switch (task.type) {
        case DataProcessType::ParseFrame:
            processParseFrame(task);
            break;
        case DataProcessType::ProcessSensorData:
            processSensorData(task);
            break;
        case DataProcessType::CalculateStatistics:
            processCalculateStats(task);
            break;
        case DataProcessType::UpdateDatabase:
            processUpdateDatabase(task);
            break;
        case DataProcessType::GenerateReport:
            processGenerateReport(task);
            break;
        default:
            LogManager::getInstance()->warning("未知的任务类型", "DataProcessWorker");
            break;
        }
    } catch (const std::exception& e) {
        QString error = QString("处理任务时发生异常: %1").arg(e.what());
        LogManager::getInstance()->error(error, "DataProcessWorker");
        emit errorOccurred(error);
    }
}

void DataProcessWorker::processParseFrame(const DataProcessTask& task)
{
    // 使用协议解析器解析数据
    if (m_protocolParser) {
        m_protocolParser->parseData(task.data);
    }
}

void DataProcessWorker::processSensorData(const DataProcessTask& task)
{
    // 处理传感器数据
    // TODO: 实现具体的传感器数据处理逻辑
    emit dataProcessed(task.data);
}

void DataProcessWorker::processCalculateStats(const DataProcessTask& task)
{
    Q_UNUSED(task)
    // 计算统计数据
    QVariantMap stats;
    stats["processedTasks"] = m_processedTaskCount;
    stats["averageProcessTime"] = m_averageProcessTime;
    stats["queueSize"] = getQueueSize();
    stats["timestamp"] = QDateTime::currentDateTime();
    
    emit statisticsUpdated(stats);
}

void DataProcessWorker::processUpdateDatabase(const DataProcessTask& task)
{
    // 更新数据库
    // TODO: 实现数据库更新逻辑
    Q_UNUSED(task)
}

void DataProcessWorker::processGenerateReport(const DataProcessTask& task)
{
    // 生成报告
    // TODO: 实现报告生成逻辑
    Q_UNUSED(task)
}

void DataProcessWorker::optimizeQueue()
{
    QMutexLocker locker(&m_taskMutex);
    
    // 移除过期的任务
    QDateTime now = QDateTime::currentDateTime();
    QQueue<DataProcessTask> newQueue;
    
    while (!m_taskQueue.isEmpty()) {
        DataProcessTask task = m_taskQueue.dequeue();
        // 如果任务不超过30秒，保留
        if (task.timestamp.msecsTo(now) < 30000) {
            newQueue.enqueue(task);
        }
    }
    
    m_taskQueue = newQueue;
}

void DataProcessWorker::balanceLoad()
{
    // 负载均衡逻辑
    int queueSize = getQueueSize();
    
    if (queueSize > m_maxQueueSize * 0.8) {
        // 队列过载，增加处理频率
        m_processingTimer->setInterval(5);
    } else if (queueSize < m_maxQueueSize * 0.2) {
        // 队列空闲，降低处理频率
        m_processingTimer->setInterval(20);
    } else {
        // 正常状态
        m_processingTimer->setInterval(10);
    }
} 