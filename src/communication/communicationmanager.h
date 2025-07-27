#pragma once

#include <QObject>
#include <QMap>
#include <QTimer>
#include <QMutex>
#include <QThread>
#include "icommunication.h"
#include "constants.h"
#include "communicationbufferpool.h"

// 连接信息结构
struct ConnectionInfo {
    QString name;
    ICommunication* communication;
    ConnectionState state;
    CommunicationType type;
    QDateTime createTime;
    QDateTime lastActiveTime;
    bool isActive;
    int priority;  // 连接优先级，数值越低优先级越高
    
    ConnectionInfo() 
        : communication(nullptr)
        , state(ConnectionState::Disconnected)
        , type(CommunicationType::Serial)
        , createTime(QDateTime::currentDateTime())
        , lastActiveTime(QDateTime::currentDateTime())
        , isActive(false)
        , priority(0)
    {}
};

// 通讯管理器类
class CommunicationManager : public QObject
{
    Q_OBJECT

public:
    explicit CommunicationManager(QObject* parent = nullptr);
    ~CommunicationManager();
    
    // 单例模式
    static CommunicationManager* getInstance();
    
    // 连接管理
    QString createConnection(CommunicationType type, const QString& name = QString());
    bool removeConnection(const QString& name);
    bool connectToDevice(const QString& name, const CommunicationConfig& config);
    bool disconnectFromDevice(const QString& name);
    void disconnectAll();
    
    // 连接查询
    ICommunication* getConnection(const QString& name) const;
    QStringList getConnectionNames() const;
    QList<ConnectionInfo> getAllConnections() const;
    ConnectionInfo getConnectionInfo(const QString& name) const;
    bool hasConnection(const QString& name) const;
    bool isConnected(const QString& name) const;
    int getConnectionCount() const;
    int getActiveConnectionCount() const;
    
    // 数据传输
    bool sendData(const QString& connectionName, const QByteArray& data);
    bool sendFrame(const QString& connectionName, ProtocolCommand command, const QByteArray& data = QByteArray());
    bool broadcastData(const QByteArray& data);
    bool broadcastFrame(ProtocolCommand command, const QByteArray& data = QByteArray());
    
    // 配置管理
    bool setConnectionConfig(const QString& name, const CommunicationConfig& config);
    CommunicationConfig getConnectionConfig(const QString& name) const;
    bool updateConnectionConfig(const QString& name, const QString& key, const QVariant& value);
    
    // 状态管理
    void enableAutoReconnect(bool enabled);
    bool isAutoReconnectEnabled() const;
    void setGlobalTimeout(int timeout);
    int getGlobalTimeout() const;
    void enableHeartbeat(bool enabled);
    bool isHeartbeatEnabled() const;
    
    // 统计信息
    CommunicationStats getTotalStatistics() const;
    CommunicationStats getConnectionStatistics(const QString& name) const;
    void resetAllStatistics();
    void resetConnectionStatistics(const QString& name);
    
    // 优先级管理
    void setConnectionPriority(const QString& name, int priority);
    int getConnectionPriority(const QString& name) const;
    QStringList getConnectionsByPriority() const;
    QString getPrimaryConnection() const;  // 获取最高优先级连接
    
    // 高级功能
    void enableConnectionPooling(bool enabled);
    bool isConnectionPoolingEnabled() const;
    void setMaxConnections(int maxCount);
    int getMaxConnections() const;
    bool testAllConnections();
    void flushAllBuffers();
    void clearAllBuffers();
    
    // 监控和诊断
    bool enableMonitoring(bool enabled);
    bool isMonitoringEnabled() const;
    void setMonitoringInterval(int interval);
    int getMonitoringInterval() const;
    QString generateDiagnosticReport() const;
    
    // 导入导出配置
    bool saveConfiguration(const QString& filePath) const;
    bool loadConfiguration(const QString& filePath);
    QJsonObject exportConfiguration() const;
    bool importConfiguration(const QJsonObject& config);
    
    // 缓冲池管理
    bool initializeBufferPool(const PoolConfig& config);
    QByteArray* allocateBuffer(BufferType type);
    void releaseBuffer(QByteArray* buffer);
    PoolStatistics getBufferPoolStatistics() const;
    void setBufferPoolConfiguration(const PoolConfig& config);
    bool isBufferPoolEnabled() const;
    void enableBufferPool(bool enabled);

signals:
    // 连接管理信号
    void connectionCreated(const QString& name, CommunicationType type);
    void connectionRemoved(const QString& name);
    void connectionConnected(const QString& name);
    void connectionDisconnected(const QString& name);
    void connectionStateChanged(const QString& name, ConnectionState state);
    void connectionError(const QString& name, const QString& error);
    
    // 数据传输信号
    void dataReceived(const QString& connectionName, const QByteArray& data);
    void dataSent(const QString& connectionName, const QByteArray& data);
    void frameReceived(const QString& connectionName, const ProtocolFrame& frame);
    void frameSent(const QString& connectionName, const ProtocolFrame& frame);
    
    // 状态监控信号
    void allConnectionsDisconnected();
    void primaryConnectionChanged(const QString& newPrimary);
    void statisticsUpdated(const CommunicationStats& totalStats);
    void performanceAlert(const QString& connectionName, const QString& message);
    void connectionTimeout(const QString& connectionName);
    
    // 配置变更信号
    void configurationChanged();
    void globalSettingsChanged();

public slots:
    // 全局操作
    void reconnectAll();
    void updateAllStatistics();
    void checkConnectionHealth();
    void cleanupInactiveConnections();
    
    // 监控操作
    void startMonitoring();
    void stopMonitoring();
    
private slots:
    // 连接信号转发
    void onConnectionConnected();
    void onConnectionDisconnected();
    void onConnectionStateChanged(ConnectionState state);
    void onConnectionError(const QString& error);
    void onDataReceived(const QByteArray& data);
    void onFrameReceived(const ProtocolFrame& frame);
    
    // 内部定时器
    void onMonitoringTimer();
    void onHealthCheckTimer();
    void onCleanupTimer();

private:
    // 静态实例
    static CommunicationManager* s_instance;
    static QMutex s_mutex;
    
    // 连接存储
    QMap<QString, ConnectionInfo> m_connections;
    mutable QMutex m_connectionsMutex;
    
    // 全局设置
    bool m_autoReconnectEnabled;
    bool m_heartbeatEnabled;
    bool m_connectionPoolingEnabled;
    bool m_monitoringEnabled;
    int m_globalTimeout;
    int m_maxConnections;
    int m_monitoringInterval;
    
    // 定时器
    QTimer* m_monitoringTimer;
    QTimer* m_healthCheckTimer;
    QTimer* m_cleanupTimer;
    
    // 线程管理
    QThread* m_workerThread;
    
    // 统计信息
    CommunicationStats m_totalStats;
    mutable QMutex m_statsMutex;
    
    // 缓冲池管理器
    CommunicationBufferPool* m_bufferPool;
    
    // 辅助方法
    QString generateUniqueConnectionName(CommunicationType type) const;
    void setupConnection(ConnectionInfo& info);
    void cleanupConnection(ConnectionInfo& info);
    void updateConnectionActivity(const QString& name);
    void calculateTotalStatistics();
    bool validateConnectionName(const QString& name) const;
    void moveToWorkerThread(ICommunication* communication);
    void setupGlobalConnections();
    void saveSettings();
    void loadSettings();
    
    // 禁用复制构造函数和赋值操作符
    CommunicationManager(const CommunicationManager&) = delete;
    CommunicationManager& operator=(const CommunicationManager&) = delete;
};