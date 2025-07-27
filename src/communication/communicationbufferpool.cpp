#include "communicationbufferpool.h"
#include "logmanager.h"
#include "../core/errorhandler.h"
#include <QDateTime>
#include <QCoreApplication>
#include <QDebug>
#include <algorithm>

// 静态成员初始化
CommunicationBufferPool* CommunicationBufferPool::s_instance = nullptr;
QMutex CommunicationBufferPool::s_instanceMutex;

CommunicationBufferPool::CommunicationBufferPool(QObject* parent)
    : QObject(parent)
    , m_cleanupTimer(new QTimer(this))
    , m_statisticsTimer(new QTimer(this))
    , m_initialized(false)
    , m_shutdown(false)
    , m_memoryThreshold(100 * 1024 * 1024) // 100MB默认阈值
    , m_lastCleanupTime(0)
{
    // 连接定时器信号
    connect(m_cleanupTimer, &QTimer::timeout, this, &CommunicationBufferPool::onCleanupTimer);
    connect(m_statisticsTimer, &QTimer::timeout, this, &CommunicationBufferPool::onStatisticsTimer);
    
    LogManager::getInstance()->info("通信缓冲池管理器已创建", "CommunicationBufferPool");
}

CommunicationBufferPool::~CommunicationBufferPool()
{
    shutdown();
    LogManager::getInstance()->info("通信缓冲池管理器已销毁", "CommunicationBufferPool");
}

CommunicationBufferPool* CommunicationBufferPool::getInstance()
{
    QMutexLocker locker(&s_instanceMutex);
    if (!s_instance) {
        s_instance = new CommunicationBufferPool();
    }
    return s_instance;
}

bool CommunicationBufferPool::initialize(const PoolConfig& config)
{
    QMutexLocker locker(&m_poolMutex);
    
    if (m_initialized) {
        LogManager::getInstance()->warning("缓冲池已经初始化", "CommunicationBufferPool");
        return true;
    }
    
    try {
        m_config = config;
        m_shutdown = false;
        
        // 预分配初始缓冲区
        for (int type = 0; type < 4; ++type) {
            BufferType bufferType = static_cast<BufferType>(type);
            preallocateBuffers(bufferType, m_config.initialPoolSize / 4);
        }
        
        // 设置定时器
        if (m_config.enableAutoCleanup) {
            m_cleanupTimer->setInterval(m_config.cleanupInterval * 1000);
            m_cleanupTimer->start();
        }
        
        if (m_config.enableStatistics) {
            m_statisticsTimer->setInterval(5000); // 5秒更新一次统计
            m_statisticsTimer->start();
        }
        
        m_initialized = true;
        m_lastCleanupTime = QDateTime::currentMSecsSinceEpoch();
        
        LogManager::getInstance()->info(
            QString("缓冲池初始化成功 - 最大大小: %1, 初始大小: %2")
            .arg(m_config.maxPoolSize)
            .arg(m_config.initialPoolSize),
            "CommunicationBufferPool"
        );
        
        return true;
    }
    catch (const std::exception& e) {
        ErrorHandler::getInstance()->reportError(
            ErrorLevel::Critical,
            "CommunicationBufferPool",
            QString("缓冲池初始化失败: %1").arg(e.what()),
            "CommunicationBufferPool::initialize"
        );
        return false;
    }
}

void CommunicationBufferPool::shutdown()
{
    QMutexLocker locker(&m_poolMutex);
    
    if (m_shutdown) {
        return;
    }
    
    m_shutdown = true;
    
    // 停止定时器
    m_cleanupTimer->stop();
    m_statisticsTimer->stop();
    
    // 强制清理所有缓冲区
    forceCleanup();
    
    m_initialized = false;
    
    LogManager::getInstance()->info("缓冲池已关闭", "CommunicationBufferPool");
}

QByteArray* CommunicationBufferPool::acquireBuffer(int size, BufferType type)
{
    if (!m_initialized || m_shutdown) {
        LogManager::getInstance()->error("缓冲池未初始化或已关闭", "CommunicationBufferPool");
        return nullptr;
    }
    
    QMutexLocker locker(&m_poolMutex);
    
    // 根据大小自动确定类型
    if (type == BufferType::Small && size > getBufferSize(BufferType::Small)) {
        type = determineBufferType(size);
    }
    
    int typeIndex = static_cast<int>(type);
    BufferInfo* bufferInfo = nullptr;
    
    // 尝试从可用池中获取
    if (!m_availableBuffers[typeIndex].isEmpty()) {
        bufferInfo = m_availableBuffers[typeIndex].dequeue();
        m_statistics.hitCount.fetchAndAddOrdered(1);
    } else {
        // 创建新缓冲区
        bufferInfo = createBuffer(type);
        if (!bufferInfo) {
            m_statistics.missCount.fetchAndAddOrdered(1);
            LogManager::getInstance()->error("无法创建新缓冲区", "CommunicationBufferPool");
            return nullptr;
        }
        m_statistics.missCount.fetchAndAddOrdered(1);
    }
    
    // 更新缓冲区信息
    bufferInfo->inUse = true;
    bufferInfo->lastUsedTime = QDateTime::currentMSecsSinceEpoch();
    bufferInfo->useCount++;
    bufferInfo->ownerThread = QThread::currentThread();
    
    // 确保缓冲区大小足够
    if (bufferInfo->buffer->size() < size) {
        bufferInfo->buffer->resize(size);
    }
    
    // 添加到使用中列表
    m_inUseBuffers.append(bufferInfo);
    m_bufferMap[bufferInfo->buffer] = bufferInfo;
    
    // 更新统计
    m_statistics.totalAllocated.fetchAndAddOrdered(1);
    m_statistics.currentInUse.fetchAndAddOrdered(1);
    
    int currentInUse = m_statistics.currentInUse.loadAcquire();
    if (currentInUse > m_statistics.peakUsage.loadAcquire()) {
        m_statistics.peakUsage.storeRelease(currentInUse);
    }
    
    return bufferInfo->buffer;
}

void CommunicationBufferPool::releaseBuffer(QByteArray* buffer)
{
    if (!buffer || !m_initialized) {
        return;
    }
    
    QMutexLocker locker(&m_poolMutex);
    
    auto it = m_bufferMap.find(buffer);
    if (it == m_bufferMap.end()) {
        LogManager::getInstance()->warning("尝试释放未知缓冲区", "CommunicationBufferPool");
        return;
    }
    
    BufferInfo* bufferInfo = it.value();
    
    // 从使用中列表移除
    m_inUseBuffers.removeOne(bufferInfo);
    m_bufferMap.remove(buffer);
    
    // 重置缓冲区状态
    bufferInfo->inUse = false;
    bufferInfo->lastUsedTime = QDateTime::currentMSecsSinceEpoch();
    bufferInfo->ownerThread = nullptr;
    
    // 清空缓冲区内容
    bufferInfo->buffer->clear();
    
    // 检查池大小限制
    int typeIndex = static_cast<int>(bufferInfo->type);
    if (m_availableBuffers[typeIndex].size() < m_config.maxPoolSize / 4) {
        // 返回到可用池
        m_availableBuffers[typeIndex].enqueue(bufferInfo);
    } else {
        // 池已满，销毁缓冲区
        destroyBuffer(bufferInfo);
    }
    
    // 更新统计
    m_statistics.totalReleased.fetchAndAddOrdered(1);
    m_statistics.currentInUse.fetchAndSubOrdered(1);
}

void CommunicationBufferPool::preallocateBuffers(BufferType type, int count)
{
    QMutexLocker locker(&m_poolMutex);
    
    int typeIndex = static_cast<int>(type);
    
    for (int i = 0; i < count; ++i) {
        BufferInfo* bufferInfo = createBuffer(type);
        if (bufferInfo) {
            m_availableBuffers[typeIndex].enqueue(bufferInfo);
        }
    }
    
    LogManager::getInstance()->debug(
        QString("预分配缓冲区 - 类型: %1, 数量: %2")
        .arg(static_cast<int>(type))
        .arg(count),
        "CommunicationBufferPool"
    );
}

void CommunicationBufferPool::cleanupIdleBuffers()
{
    QMutexLocker locker(&m_poolMutex);
    
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    qint64 idleThreshold = m_config.maxIdleTime * 1000; // 转换为毫秒
    
    int cleanedCount = 0;
    
    // 清理各类型的空闲缓冲区
    for (int type = 0; type < 4; ++type) {
        QQueue<BufferInfo*>& queue = m_availableBuffers[type];
        QQueue<BufferInfo*> tempQueue;
        
        while (!queue.isEmpty()) {
            BufferInfo* bufferInfo = queue.dequeue();
            
            if (currentTime - bufferInfo->lastUsedTime > idleThreshold) {
                // 缓冲区空闲时间过长，销毁它
                destroyBuffer(bufferInfo);
                cleanedCount++;
            } else {
                // 保留缓冲区
                tempQueue.enqueue(bufferInfo);
            }
        }
        
        queue = tempQueue;
    }
    
    m_lastCleanupTime = currentTime;
    
    if (cleanedCount > 0) {
        LogManager::getInstance()->debug(
            QString("清理空闲缓冲区: %1个").arg(cleanedCount),
            "CommunicationBufferPool"
        );
    }
}

void CommunicationBufferPool::forceCleanup()
{
    QMutexLocker locker(&m_poolMutex);
    
    int cleanedCount = 0;
    
    // 清理所有可用缓冲区
    for (int type = 0; type < 4; ++type) {
        while (!m_availableBuffers[type].isEmpty()) {
            BufferInfo* bufferInfo = m_availableBuffers[type].dequeue();
            destroyBuffer(bufferInfo);
            cleanedCount++;
        }
    }
    
    // 清理使用中的缓冲区映射
    m_bufferMap.clear();
    
    // 注意：不清理m_inUseBuffers，因为这些缓冲区可能仍在使用
    
    LogManager::getInstance()->info(
        QString("强制清理缓冲区: %1个").arg(cleanedCount),
        "CommunicationBufferPool"
    );
}

BufferInfo* CommunicationBufferPool::createBuffer(BufferType type)
{
    try {
        BufferInfo* info = new BufferInfo();
        info->type = type;
        info->allocatedTime = QDateTime::currentMSecsSinceEpoch();
        info->lastUsedTime = info->allocatedTime;
        info->useCount = 0;
        info->inUse = false;
        
        int bufferSize = getBufferSize(type);
        info->buffer = new QByteArray();
        info->buffer->reserve(bufferSize);
        
        return info;
    }
    catch (const std::exception& e) {
        ErrorHandler::getInstance()->reportError(
            ErrorLevel::Error,
            "CommunicationBufferPool",
            QString("创建缓冲区失败: %1").arg(e.what()),
            "CommunicationBufferPool::createBuffer"
        );
        return nullptr;
    }
}

void CommunicationBufferPool::destroyBuffer(BufferInfo* info)
{
    if (info) {
        delete info->buffer;
        delete info;
    }
}

BufferType CommunicationBufferPool::determineBufferType(int size) const
{
    if (size <= m_config.smallBufferSize) {
        return BufferType::Small;
    } else if (size <= m_config.mediumBufferSize) {
        return BufferType::Medium;
    } else if (size <= m_config.largeBufferSize) {
        return BufferType::Large;
    } else {
        return BufferType::Huge;
    }
}

int CommunicationBufferPool::getBufferSize(BufferType type) const
{
    switch (type) {
        case BufferType::Small:
            return m_config.smallBufferSize;
        case BufferType::Medium:
            return m_config.mediumBufferSize;
        case BufferType::Large:
            return m_config.largeBufferSize;
        case BufferType::Huge:
            return m_config.hugeBufferSize;
        default:
            return m_config.smallBufferSize;
    }
}

void CommunicationBufferPool::updateStatistics()
{
    // 计算命中率
    int totalRequests = m_statistics.hitCount.loadAcquire() + m_statistics.missCount.loadAcquire();
    if (totalRequests > 0) {
        m_statistics.hitRatio = static_cast<double>(m_statistics.hitCount.loadAcquire()) / totalRequests;
    }
    
    // 计算总内存使用量
    qint64 totalMemory = 0;
    for (int type = 0; type < 4; ++type) {
        BufferType bufferType = static_cast<BufferType>(type);
        int bufferSize = getBufferSize(bufferType);
        totalMemory += m_availableBuffers[type].size() * bufferSize;
    }
    
    // 添加使用中缓冲区的内存
    for (const BufferInfo* info : m_inUseBuffers) {
        totalMemory += info->buffer->capacity();
    }
    
    m_statistics.totalMemoryUsage = totalMemory;
    
    if (totalMemory > m_statistics.peakMemoryUsage) {
        m_statistics.peakMemoryUsage = totalMemory;
    }
}

void CommunicationBufferPool::checkMemoryUsage()
{
    if (m_statistics.totalMemoryUsage > m_memoryThreshold) {
        emit memoryUsageWarning(m_statistics.totalMemoryUsage, m_memoryThreshold);
        
        // 触发清理
        cleanupIdleBuffers();
    }
}

PoolStatistics CommunicationBufferPool::getStatistics()
{
    QMutexLocker locker(&m_poolMutex);
    return m_statistics;
}

int CommunicationBufferPool::getAvailableCount(BufferType type) const
{
    QMutexLocker locker(&m_poolMutex);
    int typeIndex = static_cast<int>(type);
    return m_availableBuffers[typeIndex].size();
}

int CommunicationBufferPool::getInUseCount(BufferType type) const
{
    QMutexLocker locker(&m_poolMutex);
    
    int count = 0;
    for (const BufferInfo* info : m_inUseBuffers) {
        if (info->type == type) {
            count++;
        }
    }
    return count;
}

qint64 CommunicationBufferPool::getTotalMemoryUsage() const
{
    return m_statistics.totalMemoryUsage;
}

bool CommunicationBufferPool::isHealthy() const
{
    // 检查各种健康指标
    bool healthy = true;
    
    // 检查内存使用
    if (m_statistics.totalMemoryUsage > m_memoryThreshold) {
        healthy = false;
    }
    
    // 检查命中率
    if (m_statistics.hitRatio < 0.7) { // 命中率低于70%
        healthy = false;
    }
    
    // 检查是否有过多的使用中缓冲区
    if (m_statistics.currentInUse.loadAcquire() > m_config.maxPoolSize * 0.9) {
        healthy = false;
    }
    
    return healthy;
}

void CommunicationBufferPool::resetStatistics()
{
    QMutexLocker locker(&m_poolMutex);
    
    m_statistics.totalAllocated = 0;
    m_statistics.totalReleased = 0;
    m_statistics.peakUsage = 0;
    m_statistics.hitCount = 0;
    m_statistics.missCount = 0;
    m_statistics.peakMemoryUsage = 0;
    m_statistics.hitRatio = 0.0;
    
    LogManager::getInstance()->info("缓冲池统计信息已重置", "CommunicationBufferPool");
}

void CommunicationBufferPool::onCleanupTimer()
{
    if (!m_shutdown) {
        cleanupIdleBuffers();
        checkMemoryUsage();
    }
}

void CommunicationBufferPool::onStatisticsTimer()
{
    if (!m_shutdown) {
        updateStatistics();
        emit statisticsUpdated(m_statistics);
        
        bool currentHealth = isHealthy();
        static bool lastHealth = true;
        
        if (currentHealth != lastHealth) {
            emit poolHealthChanged(currentHealth);
            lastHealth = currentHealth;
        }
    }
}

void CommunicationBufferPool::setPoolConfig(const PoolConfig& config)
{
    QMutexLocker locker(&m_poolMutex);
    m_config = config;
    
    // 更新定时器间隔
    if (m_config.enableAutoCleanup) {
        m_cleanupTimer->setInterval(m_config.cleanupInterval * 1000);
    }
    
    LogManager::getInstance()->info("缓冲池配置已更新", "CommunicationBufferPool");
}

PoolConfig CommunicationBufferPool::getPoolConfig() const
{
    QMutexLocker locker(&m_poolMutex);
    return m_config;
}