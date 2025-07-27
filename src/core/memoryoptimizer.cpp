#include "memoryoptimizer.h"
#include "logmanager.h"
#include "errorhandler.h"
#include <QDateTime>
#include <QCoreApplication>
#include <QDebug>
#include <QProcess>
#include <cstdlib>
#include <cstring>
#include <algorithm>

// 静态成员初始化
MemoryOptimizer* MemoryOptimizer::s_instance = nullptr;
QMutex MemoryOptimizer::s_instanceMutex;

MemoryOptimizer::MemoryOptimizer(QObject* parent)
    : QObject(parent)
    , m_cleanupTimer(new QTimer(this))
    , m_statisticsTimer(new QTimer(this))
    , m_initialized(false)
    , m_shutdown(false)
    , m_lastCleanupTime(0)
    , m_totalAllocatedMemory(0)
    , m_totalFreedMemory(0)
{
    // 连接定时器信号
    connect(m_cleanupTimer, &QTimer::timeout, this, &MemoryOptimizer::onCleanupTimer);
    connect(m_statisticsTimer, &QTimer::timeout, this, &MemoryOptimizer::onStatisticsTimer);
    
    LogManager::getInstance()->info("内存优化器已创建", "MemoryOptimizer");
}

MemoryOptimizer::~MemoryOptimizer()
{
    shutdown();
    LogManager::getInstance()->info("内存优化器已销毁", "MemoryOptimizer");
}

MemoryOptimizer* MemoryOptimizer::getInstance()
{
    QMutexLocker locker(&s_instanceMutex);
    if (!s_instance) {
        s_instance = new MemoryOptimizer();
    }
    return s_instance;
}

bool MemoryOptimizer::initialize(const MemoryOptimizerConfig& config)
{
    QMutexLocker locker(&m_memoryMutex);
    
    if (m_initialized) {
        LogManager::getInstance()->warning("内存优化器已经初始化", "MemoryOptimizer");
        return true;
    }
    
    try {
        m_config = config;
        m_shutdown = false;
        
        // 预分配内存池
        for (int type = 0; type < 4; ++type) {
            MemoryBlockType blockType = static_cast<MemoryBlockType>(type);
            for (int i = 0; i < m_config.poolInitialSize / 4; ++i) {
                MemoryBlock* block = createMemoryBlock(blockType, 0);
                if (block) {
                    m_memoryPools[type].enqueue(block);
                }
            }
        }
        
        // 设置定时器
        if (m_config.enableAutoCleanup) {
            m_cleanupTimer->setInterval(m_config.cleanupInterval * 1000);
            m_cleanupTimer->start();
        }
        
        m_statisticsTimer->setInterval(5000); // 5秒更新一次统计
        m_statisticsTimer->start();
        
        m_initialized = true;
        m_lastCleanupTime = QDateTime::currentMSecsSinceEpoch();
        
        LogManager::getInstance()->info(
            QString("内存优化器初始化成功 - 池大小: %1, 内存阈值: %2MB")
            .arg(m_config.poolMaxSize)
            .arg(m_config.memoryThreshold / (1024 * 1024)),
            "MemoryOptimizer"
        );
        
        return true;
    }
    catch (const std::exception& e) {
        ErrorHandler::getInstance()->reportError(
            ErrorLevel::Critical,
            "MemoryOptimizer",
            QString("内存优化器初始化失败: %1").arg(e.what()),
            "MemoryOptimizer::initialize"
        );
        return false;
    }
}

void MemoryOptimizer::shutdown()
{
    QMutexLocker locker(&m_memoryMutex);
    
    if (m_shutdown) {
        return;
    }
    
    m_shutdown = true;
    
    // 停止定时器
    m_cleanupTimer->stop();
    m_statisticsTimer->stop();
    
    // 清理所有内存池
    for (int type = 0; type < 4; ++type) {
        while (!m_memoryPools[type].isEmpty()) {
            MemoryBlock* block = m_memoryPools[type].dequeue();
            destroyMemoryBlock(block);
        }
    }
    
    // 清理已分配的内存块
    for (auto it = m_allocatedBlocks.begin(); it != m_allocatedBlocks.end(); ++it) {
        destroyMemoryBlock(it.value());
    }
    m_allocatedBlocks.clear();
    
    // 清理对象池
    {
        QMutexLocker poolsLocker(&m_poolsMutex);
        m_objectPools.clear();
    }
    
    m_initialized = false;
    
    LogManager::getInstance()->info("内存优化器已关闭", "MemoryOptimizer");
}

void* MemoryOptimizer::allocateMemory(size_t size, size_t alignment)
{
    if (!m_initialized || m_shutdown || size == 0) {
        return nullptr;
    }
    
    QMutexLocker locker(&m_memoryMutex);
    
    MemoryBlockType type = determineBlockType(size);
    MemoryBlock* block = allocateFromPool(type, size);
    
    if (!block) {
        // 池中没有可用内存，创建新块
        block = createMemoryBlock(type, size);
        if (!block) {
            LogManager::getInstance()->error("内存分配失败", "MemoryOptimizer");
            return nullptr;
        }
    }
    
    // 更新块信息
    block->inUse = true;
    block->lastAccessTime = QDateTime::currentMSecsSinceEpoch();
    block->refCount = 1;
    block->ownerThread = QThread::currentThread();
    
    // 添加到已分配映射
    m_allocatedBlocks[block->ptr] = block;
    
    // 更新统计
    m_statistics.totalAllocations.fetchAndAddOrdered(1);
    m_statistics.currentAllocations.fetchAndAddOrdered(1);
    m_totalAllocatedMemory += block->size;
    
    int currentAllocs = m_statistics.currentAllocations.loadAcquire();
    if (currentAllocs > m_statistics.peakAllocations.loadAcquire()) {
        m_statistics.peakAllocations.storeRelease(currentAllocs);
    }
    
    return block->ptr;
}

void MemoryOptimizer::deallocateMemory(void* ptr)
{
    if (!ptr || !m_initialized) {
        return;
    }
    
    QMutexLocker locker(&m_memoryMutex);
    
    auto it = m_allocatedBlocks.find(ptr);
    if (it == m_allocatedBlocks.end()) {
        LogManager::getInstance()->warning("尝试释放未知内存块", "MemoryOptimizer");
        return;
    }
    
    MemoryBlock* block = it.value();
    m_allocatedBlocks.erase(it);
    
    // 更新块信息
    block->inUse = false;
    block->lastAccessTime = QDateTime::currentMSecsSinceEpoch();
    block->refCount = 0;
    block->ownerThread = nullptr;
    
    // 清零内存内容（安全考虑）
    std::memset(block->ptr, 0, block->size);
    
    // 返回到池中或销毁
    returnToPool(block);
    
    // 更新统计
    m_statistics.totalDeallocations.fetchAndAddOrdered(1);
    m_statistics.currentAllocations.fetchAndSubOrdered(1);
    m_totalFreedMemory += block->size;
}

void* MemoryOptimizer::reallocateMemory(void* ptr, size_t newSize)
{
    if (!ptr) {
        return allocateMemory(newSize);
    }
    
    if (newSize == 0) {
        deallocateMemory(ptr);
        return nullptr;
    }
    
    QMutexLocker locker(&m_memoryMutex);
    
    auto it = m_allocatedBlocks.find(ptr);
    if (it == m_allocatedBlocks.end()) {
        LogManager::getInstance()->warning("尝试重新分配未知内存块", "MemoryOptimizer");
        return nullptr;
    }
    
    MemoryBlock* oldBlock = it.value();
    
    // 如果新大小小于等于当前大小，直接返回
    if (newSize <= oldBlock->size) {
        return ptr;
    }
    
    // 分配新内存
    void* newPtr = allocateMemory(newSize);
    if (!newPtr) {
        return nullptr;
    }
    
    // 复制旧数据
    std::memcpy(newPtr, ptr, oldBlock->size);
    
    // 释放旧内存
    deallocateMemory(ptr);
    
    return newPtr;
}

void MemoryOptimizer::performCleanup()
{
    QMutexLocker locker(&m_memoryMutex);
    
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    qint64 idleThreshold = m_config.maxIdleTime * 1000; // 转换为毫秒
    
    int cleanedCount = 0;
    
    // 清理各类型的空闲内存块
    for (int type = 0; type < 4; ++type) {
        QQueue<MemoryBlock*>& pool = m_memoryPools[type];
        QQueue<MemoryBlock*> tempPool;
        
        while (!pool.isEmpty()) {
            MemoryBlock* block = pool.dequeue();
            
            if (currentTime - block->lastAccessTime > idleThreshold) {
                // 内存块空闲时间过长，销毁它
                destroyMemoryBlock(block);
                cleanedCount++;
            } else {
                // 保留内存块
                tempPool.enqueue(block);
            }
        }
        
        pool = tempPool;
    }
    
    m_lastCleanupTime = currentTime;
    
    if (cleanedCount > 0) {
        LogManager::getInstance()->debug(
            QString("清理空闲内存块: %1个").arg(cleanedCount),
            "MemoryOptimizer"
        );
    }
}

void MemoryOptimizer::detectFragmentation()
{
    QMutexLocker locker(&m_memoryMutex);
    
    // 计算碎片化比率
    qint64 totalAllocated = 0;
    qint64 totalUsed = 0;
    
    for (int type = 0; type < 4; ++type) {
        for (const MemoryBlock* block : m_memoryPools[type]) {
            totalAllocated += block->size;
        }
    }
    
    for (auto it = m_allocatedBlocks.begin(); it != m_allocatedBlocks.end(); ++it) {
        const MemoryBlock* block = it.value();
        totalUsed += block->size;
        totalAllocated += block->size;
    }
    
    if (totalAllocated > 0) {
        double fragmentationRatio = 1.0 - (static_cast<double>(totalUsed) / totalAllocated);
        m_statistics.fragmentationRatio = fragmentationRatio;
        
        if (fragmentationRatio > m_config.fragmentationThreshold) {
            emit fragmentationWarning(fragmentationRatio);
            LogManager::getInstance()->warning(
                QString("检测到内存碎片化: %1%").arg(fragmentationRatio * 100, 0, 'f', 2),
                "MemoryOptimizer"
            );
        }
    }
}

void MemoryOptimizer::compactMemory()
{
    QMutexLocker locker(&m_memoryMutex);
    
    // 简单的内存压缩：清理所有空闲内存块
    int compactedCount = 0;
    
    for (int type = 0; type < 4; ++type) {
        while (!m_memoryPools[type].isEmpty()) {
            MemoryBlock* block = m_memoryPools[type].dequeue();
            destroyMemoryBlock(block);
            compactedCount++;
        }
    }
    
    LogManager::getInstance()->info(
        QString("内存压缩完成，释放了 %1 个内存块").arg(compactedCount),
        "MemoryOptimizer"
    );
}

MemoryBlockType MemoryOptimizer::determineBlockType(size_t size) const
{
    if (size <= 64) {
        return MemoryBlockType::Small;
    } else if (size <= 512) {
        return MemoryBlockType::Medium;
    } else if (size <= 4096) {
        return MemoryBlockType::Large;
    } else {
        return MemoryBlockType::Huge;
    }
}

MemoryBlock* MemoryOptimizer::allocateFromPool(MemoryBlockType type, size_t size)
{
    int typeIndex = static_cast<int>(type);
    
    if (!m_memoryPools[typeIndex].isEmpty()) {
        MemoryBlock* block = m_memoryPools[typeIndex].dequeue();
        
        // 检查块大小是否足够
        if (block->size >= size) {
            m_statistics.poolHitCount++;
            return block;
        } else {
            // 块太小，需要重新分配
            destroyMemoryBlock(block);
        }
    }
    
    m_statistics.poolMissCount++;
    return nullptr;
}

void MemoryOptimizer::returnToPool(MemoryBlock* block)
{
    if (!block) return;
    
    int typeIndex = static_cast<int>(block->type);
    
    // 检查池大小限制
    if (m_memoryPools[typeIndex].size() < m_config.poolMaxSize / 4) {
        m_memoryPools[typeIndex].enqueue(block);
    } else {
        // 池已满，销毁块
        destroyMemoryBlock(block);
    }
}

MemoryBlock* MemoryOptimizer::createMemoryBlock(MemoryBlockType type, size_t size)
{
    try {
        MemoryBlock* block = new MemoryBlock();
        block->type = type;
        block->allocatedTime = QDateTime::currentMSecsSinceEpoch();
        block->lastAccessTime = block->allocatedTime;
        
        // 确定实际分配大小
        size_t actualSize = size;
        switch (type) {
            case MemoryBlockType::Small:
                actualSize = std::max(size, static_cast<size_t>(64));
                break;
            case MemoryBlockType::Medium:
                actualSize = std::max(size, static_cast<size_t>(512));
                break;
            case MemoryBlockType::Large:
                actualSize = std::max(size, static_cast<size_t>(4096));
                break;
            case MemoryBlockType::Huge:
                actualSize = std::max(size, static_cast<size_t>(65536));
                break;
        }
        
        block->size = actualSize;
        block->ptr = std::aligned_alloc(sizeof(void*), actualSize);
        
        if (!block->ptr) {
            delete block;
            return nullptr;
        }
        
        // 初始化内存为零
        std::memset(block->ptr, 0, actualSize);
        
        return block;
    }
    catch (const std::exception& e) {
        ErrorHandler::getInstance()->reportError(
            ErrorLevel::Error,
            "MemoryOptimizer",
            QString("内存块创建失败: %1").arg(e.what()),
            "MemoryOptimizer::createMemoryBlock"
        );
        return nullptr;
    }
}

void MemoryOptimizer::destroyMemoryBlock(MemoryBlock* block)
{
    if (block) {
        if (block->ptr) {
            std::free(block->ptr);
        }
        delete block;
    }
}

void MemoryOptimizer::updateStatistics()
{
    // 计算池命中率
    int totalRequests = m_statistics.poolHitCount + m_statistics.poolMissCount;
    if (totalRequests > 0) {
        m_statistics.poolHitRatio = static_cast<double>(m_statistics.poolHitCount) / totalRequests;
    }
    
    // 更新内存使用统计
    m_statistics.totalMemoryAllocated = m_totalAllocatedMemory;
    m_statistics.totalMemoryFreed = m_totalFreedMemory;
    m_statistics.totalMemoryUsed = m_totalAllocatedMemory - m_totalFreedMemory;
    
    if (m_statistics.totalMemoryUsed > m_statistics.peakMemoryUsed) {
        m_statistics.peakMemoryUsed = m_statistics.totalMemoryUsed;
    }
}

void MemoryOptimizer::checkMemoryUsage()
{
    if (m_statistics.totalMemoryUsed > m_config.memoryThreshold) {
        emit memoryUsageWarning(m_statistics.totalMemoryUsed, m_config.memoryThreshold);
        
        // 触发清理
        performCleanup();
    }
}

void MemoryOptimizer::cleanupIdleBlocks()
{
    performCleanup();
}

MemoryStatistics MemoryOptimizer::getStatistics() const
{
    QMutexLocker locker(&m_memoryMutex);
    return m_statistics;
}

qint64 MemoryOptimizer::getMemoryUsage() const
{
    return m_statistics.totalMemoryUsed;
}

double MemoryOptimizer::getFragmentationRatio() const
{
    return m_statistics.fragmentationRatio;
}

bool MemoryOptimizer::isMemoryHealthy() const
{
    // 检查各种健康指标
    bool healthy = true;
    
    // 检查内存使用
    if (m_statistics.totalMemoryUsed > m_config.memoryThreshold) {
        healthy = false;
    }
    
    // 检查碎片化
    if (m_statistics.fragmentationRatio > m_config.fragmentationThreshold) {
        healthy = false;
    }
    
    // 检查池命中率
    if (m_statistics.poolHitRatio < 0.7) { // 命中率低于70%
        healthy = false;
    }
    
    return healthy;
}

void MemoryOptimizer::resetStatistics()
{
    QMutexLocker locker(&m_memoryMutex);
    
    m_statistics.totalAllocations = 0;
    m_statistics.totalDeallocations = 0;
    m_statistics.peakAllocations = 0;
    m_statistics.peakMemoryUsed = 0;
    m_statistics.poolHitCount = 0;
    m_statistics.poolMissCount = 0;
    m_statistics.poolHitRatio = 0.0;
    m_statistics.fragmentationRatio = 0.0;
    
    m_totalAllocatedMemory = 0;
    m_totalFreedMemory = 0;
    
    LogManager::getInstance()->info("内存统计信息已重置", "MemoryOptimizer");
}

QString MemoryOptimizer::getMemoryReport() const
{
    QMutexLocker locker(&m_memoryMutex);
    
    QString report;
    report += "=== 内存使用报告 ===\n";
    report += QString("总分配次数: %1\n").arg(m_statistics.totalAllocations.loadAcquire());
    report += QString("总释放次数: %1\n").arg(m_statistics.totalDeallocations.loadAcquire());
    report += QString("当前分配数量: %1\n").arg(m_statistics.currentAllocations.loadAcquire());
    report += QString("峰值分配数量: %1\n").arg(m_statistics.peakAllocations.loadAcquire());
    report += QString("当前内存使用: %1 MB\n").arg(m_statistics.totalMemoryUsed / (1024.0 * 1024.0), 0, 'f', 2);
    report += QString("峰值内存使用: %1 MB\n").arg(m_statistics.peakMemoryUsed / (1024.0 * 1024.0), 0, 'f', 2);
    report += QString("碎片化比率: %1%\n").arg(m_statistics.fragmentationRatio * 100, 0, 'f', 2);
    report += QString("池命中率: %1%\n").arg(m_statistics.poolHitRatio * 100, 0, 'f', 2);
    
    // 各类型池状态
    report += "\n=== 内存池状态 ===\n";
    const char* typeNames[] = {"小块", "中块", "大块", "超大块"};
    for (int i = 0; i < 4; ++i) {
        report += QString("%1内存池: %2 个可用\n")
                 .arg(typeNames[i])
                 .arg(m_memoryPools[i].size());
    }
    
    report += QString("\n使用中内存块: %1 个\n").arg(m_allocatedBlocks.size());
    
    return report;
}

void MemoryOptimizer::onCleanupTimer()
{
    if (!m_shutdown) {
        performCleanup();
        detectFragmentation();
        checkMemoryUsage();
    }
}

void MemoryOptimizer::onStatisticsTimer()
{
    if (!m_shutdown) {
        updateStatistics();
        emit statisticsUpdated(m_statistics);
        
        bool currentHealth = isMemoryHealthy();
        static bool lastHealth = true;
        
        if (currentHealth != lastHealth) {
            emit memoryHealthChanged(currentHealth);
            lastHealth = currentHealth;
        }
    }
}

void MemoryOptimizer::setConfig(const MemoryOptimizerConfig& config)
{
    QMutexLocker locker(&m_memoryMutex);
    m_config = config;
    
    // 更新定时器间隔
    if (m_config.enableAutoCleanup) {
        m_cleanupTimer->setInterval(m_config.cleanupInterval * 1000);
    }
    
    LogManager::getInstance()->info("内存优化器配置已更新", "MemoryOptimizer");
}

MemoryOptimizerConfig MemoryOptimizer::getConfig() const
{
    QMutexLocker locker(&m_memoryMutex);
    return m_config;
}