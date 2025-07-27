#include "communicationmanager.h"
#include "../logger/logmanager.h"
#include "../core/errorhandler.h"
#include "serialcommunication.h"
#include "tcpcommunication.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QThread>
#include <QDebug>

// 静态成员初始化
CommunicationManager* CommunicationManager::s_instance = nullptr;
QMutex CommunicationManager::s_mutex;

CommunicationManager* CommunicationManager::getInstance()
{
    QMutexLocker locker(&s_mutex);
    if (!s_instance) {
        s_instance = new CommunicationManager();
    }
    return s_instance;
}

CommunicationManager::CommunicationManager(QObject* parent)
    : QObject(parent)
    , m_autoReconnectEnabled(true)
    , m_heartbeatEnabled(false)
    , m_connectionPoolingEnabled(false)
    , m_monitoringEnabled(false)
    , m_globalTimeout(5000)
    , m_maxConnections(10)
    , m_monitoringInterval(1000)
    , m_monitoringTimer(new QTimer(this))
    , m_healthCheckTimer(new QTimer(this))
    , m_cleanupTimer(new QTimer(this))
    , m_workerThread(new QThread(this))
    , m_bufferPool(nullptr)
{
    // 初始化缓冲池
    m_bufferPool = new CommunicationBufferPool(this);
    
    // 配置缓冲池默认参数
    PoolConfig poolConfig;
    poolConfig.smallBufferSize = 256;
    poolConfig.mediumBufferSize = 1024;
    poolConfig.largeBufferSize = 4096;
    poolConfig.hugeBufferSize = 16384;
    poolConfig.maxPoolSize = 1000;
    poolConfig.initialPoolSize = 50;
    poolConfig.maxIdleTime = 300;
    poolConfig.cleanupInterval = 60;
    poolConfig.enableAutoCleanup = true;
    poolConfig.enableThreadSafety = true;
    poolConfig.growthFactor = 2;
    poolConfig.shrinkThreshold = 10;
    poolConfig.enableStatistics = true;
    // 内存警告相关配置已移除
    
    m_bufferPool->initialize(poolConfig);
    
    // 连接缓冲池信号
    connect(m_bufferPool, &CommunicationBufferPool::memoryUsageWarning,
            this, [this](double usage) {
                LogManager::getInstance()->warning(
                    QString("缓冲池内存使用率过高: %1%").arg(usage * 100, 0, 'f', 1),
                    "CommunicationManager"
                );
            });
    
    connect(m_bufferPool, &CommunicationBufferPool::poolHealthChanged,
            this, [this](bool healthy) {
                LogManager::getInstance()->info(
                    QString("缓冲池健康状态: %1").arg(healthy ? "健康" : "异常"),
                    "CommunicationManager"
                );
            });
    
    // 设置定时器
    m_monitoringTimer->setInterval(m_monitoringInterval);
    m_healthCheckTimer->setInterval(30000); // 30秒健康检查
    m_cleanupTimer->setInterval(60000);     // 1分钟清理
    
    // 连接定时器信号
    connect(m_monitoringTimer, &QTimer::timeout, this, &CommunicationManager::onMonitoringTimer);
    connect(m_healthCheckTimer, &QTimer::timeout, this, &CommunicationManager::onHealthCheckTimer);
    connect(m_cleanupTimer, &QTimer::timeout, this, &CommunicationManager::onCleanupTimer);
    
    // 启动工作线程
    m_workerThread->start();
    
    // 启动清理定时器
    m_cleanupTimer->start();
    
    LogManager::getInstance()->info("通信管理器已初始化", "CommunicationManager");
}

CommunicationManager::~CommunicationManager()
{
    // 停止所有定时器
    if (m_monitoringTimer) m_monitoringTimer->stop();
    if (m_healthCheckTimer) m_healthCheckTimer->stop();
    if (m_cleanupTimer) m_cleanupTimer->stop();
    
    // 断开所有连接
    disconnectAll();
    
    // 清理所有连接
    QMutexLocker locker(&m_connectionsMutex);
    for (auto& info : m_connections) {
        cleanupConnection(info);
    }
    m_connections.clear();
    
    // 关闭缓冲池
    if (m_bufferPool) {
        m_bufferPool->shutdown();
        delete m_bufferPool;
        m_bufferPool = nullptr;
    }
    
    // 停止工作线程
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait(3000);
        if (m_workerThread->isRunning()) {
            m_workerThread->terminate();
            m_workerThread->wait(1000);
        }
    }
    
    LogManager::getInstance()->info("通信管理器已销毁", "CommunicationManager");
}

QString CommunicationManager::createConnection(CommunicationType type, const QString& name)
{
    QMutexLocker locker(&m_connectionsMutex);
    
    // 检查连接数量限制
    if (m_connections.size() >= m_maxConnections) {
        LogManager::getInstance()->warning(
            QString("已达到最大连接数限制: %1").arg(m_maxConnections),
            "CommunicationManager"
        );
        return QString();
    }
    
    QString connectionName = name.isEmpty() ? generateUniqueConnectionName(type) : name;
    
    // 检查名称是否已存在
    if (m_connections.contains(connectionName)) {
        LogManager::getInstance()->warning(
            QString("连接名称已存在: %1").arg(connectionName),
            "CommunicationManager"
        );
        return QString();
    }
    
    // 创建连接对象
    ICommunication* communication = nullptr;
    switch (type) {
        case CommunicationType::Serial:
            communication = new SerialCommunication();
            break;
        case CommunicationType::TCP:
            communication = new TcpCommunication();
            break;
        default:
            LogManager::getInstance()->error(
                QString("不支持的通信类型: %1").arg(static_cast<int>(type)),
                "CommunicationManager"
            );
            return QString();
    }
    
    if (!communication) {
        LogManager::getInstance()->error(
            "创建通信对象失败",
            "CommunicationManager"
        );
        return QString();
    }
    
    // 创建连接信息
    ConnectionInfo info;
    info.name = connectionName;
    info.communication = communication;
    info.type = type;
    info.state = ConnectionState::Disconnected;
    info.createTime = QDateTime::currentDateTime();
    info.lastActiveTime = QDateTime::currentDateTime();
    info.isActive = false;
    info.priority = m_connections.size(); // 默认优先级
    
    // 设置连接
    setupConnection(info);
    
    // 存储连接
    m_connections[connectionName] = info;
    
    emit connectionCreated(connectionName, type);
    
    LogManager::getInstance()->info(
        QString("连接已创建: %1 (类型: %2)").arg(connectionName).arg(static_cast<int>(type)),
        "CommunicationManager"
    );
    
    return connectionName;
}

bool CommunicationManager::removeConnection(const QString& name)
{
    QMutexLocker locker(&m_connectionsMutex);
    
    if (!m_connections.contains(name)) {
        return false;
    }
    
    ConnectionInfo& info = m_connections[name];
    
    // 断开连接
    if (info.communication && info.state == ConnectionState::Connected) {
        info.communication->disconnect();
    }
    
    // 清理连接
    cleanupConnection(info);
    
    // 移除连接
    m_connections.remove(name);
    
    emit connectionRemoved(name);
    
    LogManager::getInstance()->info(
        QString("连接已移除: %1").arg(name),
        "CommunicationManager"
    );
    
    return true;
}

bool CommunicationManager::sendData(const QString& connectionName, const QByteArray& data)
{
    QMutexLocker locker(&m_connectionsMutex);
    
    if (!m_connections.contains(connectionName)) {
        return false;
    }
    
    ConnectionInfo& info = m_connections[connectionName];
    if (!info.communication || info.state != ConnectionState::Connected) {
        return false;
    }
    
    // 使用缓冲池分配缓冲区
    QByteArray* buffer = nullptr;
    if (m_bufferPool) {
        BufferType bufferType = BufferType::Small;
        if (data.size() > 4096) {
            bufferType = BufferType::Huge;
        } else if (data.size() > 1024) {
            bufferType = BufferType::Large;
        } else if (data.size() > 256) {
            bufferType = BufferType::Medium;
        }
        
        int size = 256;
        switch (bufferType) {
            case BufferType::Small: size = 512; break;
            case BufferType::Medium: size = 4096; break;
            case BufferType::Large: size = 65536; break;
            case BufferType::Huge: size = 1048576; break;
        }
        buffer = m_bufferPool->acquireBuffer(size, bufferType);
        if (buffer) {
            *buffer = data;
        }
    }
    
    bool success = false;
    if (buffer) {
        success = info.communication->sendData(*buffer);
        // 释放缓冲区
        m_bufferPool->releaseBuffer(buffer);
    } else {
        success = info.communication->sendData(data);
    }
    
    if (success) {
        updateConnectionActivity(connectionName);
        emit dataSent(connectionName, data);
        
        // 更新统计信息
        QMutexLocker statsLocker(&m_statsMutex);
        m_totalStats.bytesSent += data.size();
        m_totalStats.framesSent++;
    }
    
    return success;
}

// 缓冲池管理方法实现
bool CommunicationManager::initializeBufferPool(const PoolConfig& config)
{
    if (!m_bufferPool) {
        return false;
    }
    
    bool success = m_bufferPool->initialize(config);
    
    if (success) {
        LogManager::getInstance()->info("缓冲池初始化成功", "CommunicationManager");
    } else {
        LogManager::getInstance()->error("缓冲池初始化失败", "CommunicationManager");
    }
    
    return success;
}

QByteArray* CommunicationManager::allocateBuffer(BufferType type)
{
    if (m_bufferPool) {
        int size = 256; // 默认大小
        switch (type) {
            case BufferType::Small: size = 256; break;
            case BufferType::Medium: size = 1024; break;
            case BufferType::Large: size = 4096; break;
            case BufferType::Huge: size = 16384; break;
        }
        return m_bufferPool->acquireBuffer(size, type);
    }
    return nullptr;
}

void CommunicationManager::releaseBuffer(QByteArray* buffer)
{
    if (m_bufferPool && buffer) {
        m_bufferPool->releaseBuffer(buffer);
    }
}

PoolStatistics CommunicationManager::getBufferPoolStatistics() const
{
    if (m_bufferPool) {
        return m_bufferPool->getStatistics();
    }
    return PoolStatistics();
}

void CommunicationManager::setBufferPoolConfiguration(const PoolConfig& config)
{
    if (m_bufferPool) {
        m_bufferPool->setPoolConfig(config);
        LogManager::getInstance()->info("缓冲池配置已更新", "CommunicationManager");
    }
}

bool CommunicationManager::isBufferPoolEnabled() const
{
    return m_bufferPool != nullptr;
}

void CommunicationManager::enableBufferPool(bool enabled)
{
    if (!m_bufferPool) {
        return;
    }
    
    if (enabled) {
        // 使用默认配置初始化缓冲池
        PoolConfig config;
        config.smallBufferSize = 512;
        config.mediumBufferSize = 4096;
        config.largeBufferSize = 65536;
        config.hugeBufferSize = 1048576;
        config.maxPoolSize = 1000;
        config.initialPoolSize = 50;
        config.maxIdleTime = 300;
        config.cleanupInterval = 60;
        config.enableAutoCleanup = true;
        config.enableStatistics = true;
        config.enableThreadSafety = true;
        config.growthFactor = 2;
        config.shrinkThreshold = 10;
        
        initializeBufferPool(config);
    } else if (!enabled) {
        m_bufferPool->shutdown();
        LogManager::getInstance()->info("缓冲池已禁用", "CommunicationManager");
    }
}

// 辅助方法实现
QString CommunicationManager::generateUniqueConnectionName(CommunicationType type) const
{
    QString prefix;
    switch (type) {
        case CommunicationType::Serial:
            prefix = "Serial";
            break;
        case CommunicationType::TCP:
            prefix = "TCP";
            break;
        default:
            prefix = "Unknown";
            break;
    }
    
    int counter = 1;
    QString name;
    do {
        name = QString("%1_%2").arg(prefix).arg(counter++);
    } while (m_connections.contains(name));
    
    return name;
}

void CommunicationManager::setupConnection(ConnectionInfo& info)
{
    if (!info.communication) {
        return;
    }
    
    // 连接信号
    connect(info.communication, &ICommunication::connected,
            this, &CommunicationManager::onConnectionConnected);
    connect(info.communication, &ICommunication::disconnected,
            this, &CommunicationManager::onConnectionDisconnected);
    connect(info.communication, &ICommunication::connectionStateChanged,
            this, &CommunicationManager::onConnectionStateChanged);
    connect(info.communication, &ICommunication::connectionError,
            this, &CommunicationManager::onConnectionError);
    connect(info.communication, &ICommunication::dataReceived,
            this, &CommunicationManager::onDataReceived);
    
    // 移动到工作线程
    moveToWorkerThread(info.communication);
}

void CommunicationManager::cleanupConnection(ConnectionInfo& info)
{
    if (info.communication) {
        info.communication->disconnect();
        info.communication->deleteLater();
        info.communication = nullptr;
    }
}

void CommunicationManager::updateConnectionActivity(const QString& name)
{
    QMutexLocker locker(&m_connectionsMutex);
    if (m_connections.contains(name)) {
        m_connections[name].lastActiveTime = QDateTime::currentDateTime();
        m_connections[name].isActive = true;
    }
}

void CommunicationManager::moveToWorkerThread(ICommunication* communication)
{
    if (communication && m_workerThread) {
        communication->moveToThread(m_workerThread);
    }
}

// 槽函数实现
void CommunicationManager::onConnectionConnected()
{
    ICommunication* communication = qobject_cast<ICommunication*>(sender());
    if (!communication) return;
    
    QMutexLocker locker(&m_connectionsMutex);
    for (auto& info : m_connections) {
        if (info.communication == communication) {
            info.state = ConnectionState::Connected;
            info.isActive = true;
            updateConnectionActivity(info.name);
            emit connectionConnected(info.name);
            break;
        }
    }
}

void CommunicationManager::onConnectionDisconnected()
{
    ICommunication* communication = qobject_cast<ICommunication*>(sender());
    if (!communication) return;
    
    QMutexLocker locker(&m_connectionsMutex);
    for (auto& info : m_connections) {
        if (info.communication == communication) {
            info.state = ConnectionState::Disconnected;
            info.isActive = false;
            emit connectionDisconnected(info.name);
            break;
        }
    }
}

void CommunicationManager::onConnectionStateChanged(ConnectionState state)
{
    ICommunication* communication = qobject_cast<ICommunication*>(sender());
    if (!communication) return;
    
    QMutexLocker locker(&m_connectionsMutex);
    for (auto& info : m_connections) {
        if (info.communication == communication) {
            info.state = state;
            emit connectionStateChanged(info.name, state);
            break;
        }
    }
}

void CommunicationManager::onConnectionError(const QString& error)
{
    ICommunication* communication = qobject_cast<ICommunication*>(sender());
    if (!communication) return;
    
    QMutexLocker locker(&m_connectionsMutex);
    for (auto& info : m_connections) {
        if (info.communication == communication) {
            emit connectionError(info.name, error);
            LogManager::getInstance()->error(
                QString("连接错误 [%1]: %2").arg(info.name, error),
                "CommunicationManager"
            );
            break;
        }
    }
}

void CommunicationManager::onDataReceived(const QByteArray& data)
{
    ICommunication* communication = qobject_cast<ICommunication*>(sender());
    if (!communication) return;
    
    QMutexLocker locker(&m_connectionsMutex);
    for (auto& info : m_connections) {
        if (info.communication == communication) {
            updateConnectionActivity(info.name);
            emit dataReceived(info.name, data);
            
            // 更新统计信息
            QMutexLocker statsLocker(&m_statsMutex);
            m_totalStats.bytesReceived += data.size();
            m_totalStats.framesReceived++;
            break;
        }
    }
}

void CommunicationManager::onMonitoringTimer()
{
    // 监控逻辑实现
    calculateTotalStatistics();
    emit statisticsUpdated(m_totalStats);
}

void CommunicationManager::onHealthCheckTimer()
{
    // 健康检查逻辑
    QMutexLocker locker(&m_connectionsMutex);
    for (const auto& info : m_connections) {
        if (info.communication && info.state == ConnectionState::Connected) {
            // 检查连接健康状态
            if (!info.isActive) {
                LogManager::getInstance()->warning(
                    QString("连接可能不活跃: %1").arg(info.name),
                    "CommunicationManager"
                );
            }
        }
    }
}

void CommunicationManager::onCleanupTimer()
{
    // 清理不活跃连接
    cleanupInactiveConnections();
}

void CommunicationManager::calculateTotalStatistics()
{
    // 计算总体统计信息
    QMutexLocker locker(&m_statsMutex);
    // 这里可以添加更复杂的统计计算逻辑
}

void CommunicationManager::cleanupInactiveConnections()
{
    QMutexLocker locker(&m_connectionsMutex);
    QDateTime now = QDateTime::currentDateTime();
    
    for (auto it = m_connections.begin(); it != m_connections.end();) {
        const auto& info = it.value();
        
        // 检查连接是否长时间不活跃
        if (info.state == ConnectionState::Disconnected &&
            info.lastActiveTime.secsTo(now) > 3600) { // 1小时不活跃
            
            LogManager::getInstance()->info(
                QString("清理不活跃连接: %1").arg(info.name),
                "CommunicationManager"
            );
            
            cleanupConnection(const_cast<ConnectionInfo&>(info));
            it = m_connections.erase(it);
        } else {
            ++it;
        }
    }
}

void CommunicationManager::disconnectAll()
{
    QMutexLocker locker(&m_connectionsMutex);
    for (auto& info : m_connections) {
        if (info.communication && info.state == ConnectionState::Connected) {
            info.communication->disconnect();
        }
    }
    LogManager::getInstance()->info("断开所有连接", "CommunicationManager");
}

void CommunicationManager::reconnectAll()
{
    QMutexLocker locker(&m_connectionsMutex);
    for (auto& info : m_connections) {
        if (info.communication && info.state == ConnectionState::Disconnected) {
            info.communication->reconnect();
        }
    }
    LogManager::getInstance()->info("重新连接所有断开的连接", "CommunicationManager");
}

void CommunicationManager::startMonitoring()
{
    if (!m_monitoringEnabled) {
        m_monitoringEnabled = true;
        m_monitoringTimer->start();
        m_healthCheckTimer->start();
        LogManager::getInstance()->info("开始监控连接状态", "CommunicationManager");
    }
}

void CommunicationManager::stopMonitoring()
{
    if (m_monitoringEnabled) {
        m_monitoringEnabled = false;
        m_monitoringTimer->stop();
        m_healthCheckTimer->stop();
        LogManager::getInstance()->info("停止监控连接状态", "CommunicationManager");
    }
}

void CommunicationManager::updateAllStatistics()
{
    calculateTotalStatistics();
    emit statisticsUpdated(m_totalStats);
    LogManager::getInstance()->debug("更新所有连接统计信息", "CommunicationManager");
}

void CommunicationManager::checkConnectionHealth()
{
    QMutexLocker locker(&m_connectionsMutex);
    for (const auto& info : m_connections) {
        if (info.communication && info.state == ConnectionState::Connected) {
            // 检查连接健康状态
            if (!info.isActive) {
                LogManager::getInstance()->warning(
                    QString("连接健康检查: %1 可能不活跃").arg(info.name),
                    "CommunicationManager"
                );
            }
        }
    }
}

void CommunicationManager::onFrameReceived(const ProtocolFrame& frame)
{
    // 处理接收到的协议帧
    ICommunication* communication = qobject_cast<ICommunication*>(sender());
    QString connectionName = "Unknown";
    
    if (communication) {
        QMutexLocker locker(&m_connectionsMutex);
        for (const auto& info : m_connections) {
            if (info.communication == communication) {
                connectionName = info.name;
                break;
            }
        }
    }
    
    emit frameReceived(connectionName, frame);
    LogManager::getInstance()->debug(
        QString("接收到协议帧，命令: %1").arg(static_cast<int>(frame.command)),
        "CommunicationManager"
    );
}