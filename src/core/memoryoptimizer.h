#ifndef MEMORYOPTIMIZER_H
#define MEMORYOPTIMIZER_H

#include <QObject>
#include <QMutex>
#include <QTimer>
#include <QHash>
#include <QQueue>
#include <QAtomicInt>
#include <QAtomicPointer>
#include <QThread>
#include <QSharedPointer>
#include <QWeakPointer>
#include <memory>

/**
 * @brief 内存块类型枚举
 */
enum class MemoryBlockType {
    Small = 0,      // 小块内存 (8-64字节)
    Medium,         // 中块内存 (64-512字节)
    Large,          // 大块内存 (512-4KB)
    Huge            // 超大内存 (4KB+)
};

/**
 * @brief 内存块信息结构
 */
struct MemoryBlock {
    void* ptr;                      // 内存指针
    size_t size;                    // 内存大小
    MemoryBlockType type;           // 内存类型
    qint64 allocatedTime;           // 分配时间
    qint64 lastAccessTime;          // 最后访问时间
    int refCount;                   // 引用计数
    bool inUse;                     // 是否使用中
    QThread* ownerThread;           // 拥有线程
    QString allocatorInfo;          // 分配器信息
    
    MemoryBlock() : ptr(nullptr), size(0), type(MemoryBlockType::Small),
                   allocatedTime(0), lastAccessTime(0), refCount(0),
                   inUse(false), ownerThread(nullptr) {}
};

/**
 * @brief 对象池模板类
 */
template<typename T>
class ObjectPool {
public:
    ObjectPool(int initialSize = 10, int maxSize = 100)
        : m_maxSize(maxSize), m_currentSize(0) {
        // 预分配对象
        for (int i = 0; i < initialSize; ++i) {
            m_availableObjects.enqueue(std::make_unique<T>());
            m_currentSize++;
        }
    }
    
    ~ObjectPool() {
        clear();
    }
    
    std::unique_ptr<T> acquire() {
        QMutexLocker locker(&m_mutex);
        
        if (!m_availableObjects.isEmpty()) {
            return std::move(m_availableObjects.dequeue());
        }
        
        // 如果池为空且未达到最大大小，创建新对象
        if (m_currentSize < m_maxSize) {
            m_currentSize++;
            return std::make_unique<T>();
        }
        
        // 池已满，返回空指针
        return nullptr;
    }
    
    void release(std::unique_ptr<T> obj) {
        if (!obj) return;
        
        QMutexLocker locker(&m_mutex);
        
        if (m_availableObjects.size() < m_maxSize / 2) {
            // 重置对象状态（如果需要）
            resetObject(obj.get());
            m_availableObjects.enqueue(std::move(obj));
        } else {
            // 池已满，直接销毁对象
            m_currentSize--;
        }
    }
    
    int size() const {
        QMutexLocker locker(&m_mutex);
        return m_currentSize;
    }
    
    int available() const {
        QMutexLocker locker(&m_mutex);
        return m_availableObjects.size();
    }
    
    void clear() {
        QMutexLocker locker(&m_mutex);
        while (!m_availableObjects.isEmpty()) {
            m_availableObjects.dequeue();
        }
        m_currentSize = 0;
    }
    
private:
    void resetObject(T* obj) {
        // 默认实现：什么都不做
        // 子类可以重写此方法来重置对象状态
        Q_UNUSED(obj)
    }
    
    QQueue<std::unique_ptr<T>> m_availableObjects;
    mutable QMutex m_mutex;
    int m_maxSize;
    int m_currentSize;
};

/**
 * @brief 内存统计信息结构
 */
struct MemoryStatistics {
    QAtomicInt totalAllocations;        // 总分配次数
    QAtomicInt totalDeallocations;      // 总释放次数
    QAtomicInt currentAllocations;      // 当前分配数量
    QAtomicInt peakAllocations;         // 峰值分配数量
    qint64 totalMemoryUsed;             // 总内存使用量
    qint64 peakMemoryUsed;              // 峰值内存使用量
    qint64 totalMemoryAllocated;        // 总分配内存量
    qint64 totalMemoryFreed;            // 总释放内存量
    double fragmentationRatio;          // 碎片化比率
    int poolHitCount;                   // 池命中次数
    int poolMissCount;                  // 池未命中次数
    double poolHitRatio;                // 池命中率
    
    MemoryStatistics() : totalAllocations(0), totalDeallocations(0),
                        currentAllocations(0), peakAllocations(0),
                        totalMemoryUsed(0), peakMemoryUsed(0),
                        totalMemoryAllocated(0), totalMemoryFreed(0),
                        fragmentationRatio(0.0), poolHitCount(0),
                        poolMissCount(0), poolHitRatio(0.0) {}
};

/**
 * @brief 内存优化配置结构
 */
struct MemoryOptimizerConfig {
    bool enableObjectPools;             // 启用对象池
    bool enableMemoryTracking;          // 启用内存跟踪
    bool enableAutoCleanup;             // 启用自动清理
    bool enableFragmentationDetection; // 启用碎片检测
    int cleanupInterval;                // 清理间隔(秒)
    int maxIdleTime;                    // 最大空闲时间(秒)
    qint64 memoryThreshold;             // 内存阈值(字节)
    double fragmentationThreshold;      // 碎片化阈值
    int poolInitialSize;                // 池初始大小
    int poolMaxSize;                    // 池最大大小
    
    MemoryOptimizerConfig() : enableObjectPools(true), enableMemoryTracking(true),
                             enableAutoCleanup(true), enableFragmentationDetection(true),
                             cleanupInterval(60), maxIdleTime(300),
                             memoryThreshold(500 * 1024 * 1024), // 500MB
                             fragmentationThreshold(0.3), poolInitialSize(20),
                             poolMaxSize(200) {}
};

/**
 * @brief 智能内存管理器
 * 
 * 提供高效的内存分配、对象池管理、内存跟踪和优化功能
 * 支持自动清理、碎片检测、统计监控等高级特性
 */
class MemoryOptimizer : public QObject
{
    Q_OBJECT

public:
    explicit MemoryOptimizer(QObject* parent = nullptr);
    ~MemoryOptimizer();

    /**
     * @brief 获取单例实例
     * @return 单例指针
     */
    static MemoryOptimizer* getInstance();

    /**
     * @brief 初始化内存优化器
     * @param config 配置参数
     * @return 是否成功
     */
    bool initialize(const MemoryOptimizerConfig& config = MemoryOptimizerConfig());

    /**
     * @brief 关闭内存优化器
     */
    void shutdown();

    /**
     * @brief 分配内存
     * @param size 内存大小
     * @param alignment 对齐要求
     * @return 内存指针
     */
    void* allocateMemory(size_t size, size_t alignment = sizeof(void*));

    /**
     * @brief 释放内存
     * @param ptr 内存指针
     */
    void deallocateMemory(void* ptr);

    /**
     * @brief 重新分配内存
     * @param ptr 原内存指针
     * @param newSize 新大小
     * @return 新内存指针
     */
    void* reallocateMemory(void* ptr, size_t newSize);

    /**
     * @brief 获取对象池
     * @tparam T 对象类型
     * @return 对象池指针
     */
    template<typename T>
    ObjectPool<T>* getObjectPool() {
        QString typeName = typeid(T).name();
        QMutexLocker locker(&m_poolsMutex);
        
        auto it = m_objectPools.find(typeName);
        if (it == m_objectPools.end()) {
            auto pool = std::make_shared<ObjectPool<T>>(m_config.poolInitialSize, m_config.poolMaxSize);
            m_objectPools[typeName] = pool;
            return pool.get();
        }
        
        return static_cast<ObjectPool<T>*>(it.value().get());
    }

    /**
     * @brief 创建智能指针
     * @tparam T 对象类型
     * @tparam Args 构造参数类型
     * @param args 构造参数
     * @return 智能指针
     */
    template<typename T, typename... Args>
    QSharedPointer<T> createShared(Args&&... args) {
        if (m_config.enableObjectPools) {
            ObjectPool<T>* pool = getObjectPool<T>();
            auto obj = pool->acquire();
            if (obj) {
                // 使用placement new重新构造对象
                new (obj.get()) T(std::forward<Args>(args)...);
                
                // 创建自定义删除器，将对象返回到池中
                auto deleter = [pool](T* ptr) {
                    ptr->~T();
                    pool->release(std::unique_ptr<T>(ptr));
                };
                
                return QSharedPointer<T>(obj.release(), deleter);
            }
        }
        
        // 回退到标准分配
        return QSharedPointer<T>::create(std::forward<Args>(args)...);
    }

    /**
     * @brief 执行内存清理
     */
    void performCleanup();

    /**
     * @brief 检测内存碎片
     */
    void detectFragmentation();

    /**
     * @brief 压缩内存
     */
    void compactMemory();

    /**
     * @brief 设置配置
     * @param config 新配置
     */
    void setConfig(const MemoryOptimizerConfig& config);

    /**
     * @brief 获取配置
     * @return 当前配置
     */
    MemoryOptimizerConfig getConfig() const;

    /**
     * @brief 获取内存统计信息
     * @return 统计信息
     */
    MemoryStatistics getStatistics() const;

    /**
     * @brief 获取内存使用情况
     * @return 内存使用量(字节)
     */
    qint64 getMemoryUsage() const;

    /**
     * @brief 获取碎片化比率
     * @return 碎片化比率
     */
    double getFragmentationRatio() const;

    /**
     * @brief 检查内存健康状态
     * @return 是否健康
     */
    bool isMemoryHealthy() const;

    /**
     * @brief 重置统计信息
     */
    void resetStatistics();

    /**
     * @brief 获取内存使用报告
     * @return 报告字符串
     */
    QString getMemoryReport() const;

public slots:
    /**
     * @brief 定时清理槽
     */
    void onCleanupTimer();

    /**
     * @brief 统计更新槽
     */
    void onStatisticsTimer();

signals:
    /**
     * @brief 内存使用警告信号
     * @param usage 当前使用量
     * @param threshold 阈值
     */
    void memoryUsageWarning(qint64 usage, qint64 threshold);

    /**
     * @brief 内存碎片警告信号
     * @param ratio 碎片化比率
     */
    void fragmentationWarning(double ratio);

    /**
     * @brief 统计信息更新信号
     * @param stats 统计信息
     */
    void statisticsUpdated(const MemoryStatistics& stats);

    /**
     * @brief 内存健康状态变化信号
     * @param healthy 是否健康
     */
    void memoryHealthChanged(bool healthy);

private:
    /**
     * @brief 确定内存块类型
     * @param size 内存大小
     * @return 内存块类型
     */
    MemoryBlockType determineBlockType(size_t size) const;

    /**
     * @brief 从池中分配内存
     * @param type 内存块类型
     * @param size 内存大小
     * @return 内存块指针
     */
    MemoryBlock* allocateFromPool(MemoryBlockType type, size_t size);

    /**
     * @brief 返回内存到池中
     * @param block 内存块
     */
    void returnToPool(MemoryBlock* block);

    /**
     * @brief 创建新内存块
     * @param type 内存块类型
     * @param size 内存大小
     * @return 内存块指针
     */
    MemoryBlock* createMemoryBlock(MemoryBlockType type, size_t size);

    /**
     * @brief 销毁内存块
     * @param block 内存块
     */
    void destroyMemoryBlock(MemoryBlock* block);

    /**
     * @brief 更新统计信息
     */
    void updateStatistics();

    /**
     * @brief 检查内存使用情况
     */
    void checkMemoryUsage();

    /**
     * @brief 清理空闲内存块
     */
    void cleanupIdleBlocks();

private:
    static MemoryOptimizer* s_instance;         // 单例实例
    static QMutex s_instanceMutex;              // 实例互斥锁
    
    // 内存池 - 按类型分组
    QQueue<MemoryBlock*> m_memoryPools[4];      // 内存池队列
    QHash<void*, MemoryBlock*> m_allocatedBlocks; // 已分配内存块映射
    
    // 对象池管理
    QHash<QString, std::shared_ptr<void>> m_objectPools; // 对象池映射
    mutable QMutex m_poolsMutex;                // 对象池互斥锁
    
    mutable QMutex m_memoryMutex;               // 内存互斥锁
    
    MemoryOptimizerConfig m_config;             // 配置
    MemoryStatistics m_statistics;              // 统计信息
    
    QTimer* m_cleanupTimer;                     // 清理定时器
    QTimer* m_statisticsTimer;                  // 统计定时器
    
    bool m_initialized;                         // 是否已初始化
    bool m_shutdown;                            // 是否已关闭
    
    qint64 m_lastCleanupTime;                   // 最后清理时间
    qint64 m_totalAllocatedMemory;              // 总分配内存
    qint64 m_totalFreedMemory;                  // 总释放内存
};

#endif // MEMORYOPTIMIZER_H