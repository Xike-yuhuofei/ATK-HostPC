#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QVariant>
#include "constants.h"
#include "protocolparser.h"

// 通讯连接状态枚举
enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Reconnecting,
    Error
};

// 通讯类型枚举
enum class CommunicationType {
    Serial,
    TCP,
    UDP,
    CAN,
    Modbus
};

// 通讯配置基类
struct CommunicationConfig {
    QString name;                   // 连接名称
    CommunicationType type;         // 通讯类型
    bool autoReconnect;            // 自动重连
    int timeout;                   // 超时时间
    int reconnectInterval;         // 重连间隔
    int maxReconnectAttempts;      // 最大重连次数
    bool enableHeartbeat;          // 启用心跳
    int heartbeatInterval;         // 心跳间隔
    
    CommunicationConfig() 
        : name("Connection")
        , type(CommunicationType::Serial)
        , autoReconnect(true)
        , timeout(Protocol::DEFAULT_TIMEOUT)
        , reconnectInterval(Protocol::RECONNECT_DELAY)
        , maxReconnectAttempts(Protocol::MAX_RECONNECT_ATTEMPTS)
        , enableHeartbeat(true)
        , heartbeatInterval(Protocol::HEARTBEAT_INTERVAL)
    {}
    
    virtual ~CommunicationConfig() = default;
};

// 通讯统计信息
struct CommunicationStats {
    qint64 bytesReceived;          // 接收字节数
    qint64 bytesSent;              // 发送字节数
    qint64 framesReceived;         // 接收帧数
    qint64 framesSent;             // 发送帧数
    qint64 errorCount;             // 错误次数
    qint64 reconnectCount;         // 重连次数
    double averageLatency;         // 平均延迟(ms)
    QDateTime startTime;           // 连接开始时间
    QDateTime lastActivityTime;    // 最后活动时间
    
    CommunicationStats() 
        : bytesReceived(0)
        , bytesSent(0)
        , framesReceived(0)
        , framesSent(0)
        , errorCount(0)
        , reconnectCount(0)
        , averageLatency(0.0)
        , startTime(QDateTime::currentDateTime())
        , lastActivityTime(QDateTime::currentDateTime())
    {}
    
    void reset() {
        bytesReceived = 0;
        bytesSent = 0;
        framesReceived = 0;
        framesSent = 0;
        errorCount = 0;
        reconnectCount = 0;
        averageLatency = 0.0;
        startTime = QDateTime::currentDateTime();
        lastActivityTime = QDateTime::currentDateTime();
    }
};

// 通讯接口抽象类
class ICommunication : public QObject
{
    Q_OBJECT

public:
    explicit ICommunication(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~ICommunication() = default;

    // 基本连接管理
    virtual bool connect(const CommunicationConfig& config) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual ConnectionState getConnectionState() const = 0;
    virtual CommunicationType getType() const = 0;
    virtual QString getName() const = 0;
    
    // 数据传输
    virtual bool sendData(const QByteArray& data) = 0;
    virtual bool sendFrame(ProtocolCommand command, const QByteArray& data = QByteArray()) = 0;
    virtual QByteArray receiveData() = 0;
    
    // 配置管理
    virtual void setConfig(const CommunicationConfig& config) = 0;
    virtual CommunicationConfig getConfig() const = 0;
    virtual bool updateConfig(const QString& key, const QVariant& value) = 0;
    
    // 状态查询
    virtual QString getLastError() const = 0;
    virtual CommunicationStats getStatistics() const = 0;
    virtual void resetStatistics() = 0;
    
    // 心跳管理
    virtual void enableHeartbeat(bool enabled) = 0;
    virtual bool isHeartbeatEnabled() const = 0;
    virtual void sendHeartbeat() = 0;
    virtual qint64 getLastHeartbeatTime() const = 0;
    
    // 重连管理
    virtual void enableAutoReconnect(bool enabled) = 0;
    virtual bool isAutoReconnectEnabled() const = 0;
    virtual void setMaxReconnectAttempts(int maxAttempts) = 0;
    virtual int getCurrentReconnectAttempts() const = 0;
    virtual void resetReconnectAttempts() = 0;
    
    // 高级功能
    virtual void flush() = 0;                                    // 刷新缓冲区
    virtual void clearBuffers() = 0;                             // 清空缓冲区
    virtual bool testConnection() = 0;                           // 测试连接
    virtual QStringList getAvailableConnections() const = 0;    // 获取可用连接列表
    virtual void setProperty(const QString& name, const QVariant& value) = 0;  // 设置属性
    virtual QVariant getProperty(const QString& name) const = 0;               // 获取属性

signals:
    // 连接状态信号
    void connected();
    void disconnected();
    void connectionStateChanged(ConnectionState state);
    void connectionError(const QString& error);
    void reconnectAttempt(int attempt);
    
    // 数据传输信号
    void dataReceived(const QByteArray& data);
    void dataSent(const QByteArray& data);
    void frameReceived(const ProtocolFrame& frame);
    void frameSent(const ProtocolFrame& frame);
    void bytesWritten(qint64 bytes);
    
    // 协议相关信号
    void heartbeatReceived(qint64 delay);
    void heartbeatTimeout();
    void protocolError(const QString& error);
    
    // 统计信息信号
    void statisticsUpdated(const CommunicationStats& stats);
    void performanceAlert(const QString& message);
    
    // 高级信号
    void propertyChanged(const QString& name, const QVariant& value);
    void configurationChanged();

public slots:
    // 公共槽函数
    virtual void reconnect() = 0;
    virtual void startHeartbeat() = 0;
    virtual void stopHeartbeat() = 0;
    virtual void updateStatistics() = 0;

protected slots:
    // 内部处理槽函数 
    virtual void onHeartbeatTimer() = 0;
    virtual void onReconnectTimer() = 0;
    virtual void onConnectionTimeout() = 0;
    virtual void onDataReceived() = 0;
    virtual void onErrorOccurred() = 0;

protected:
    // 辅助方法
    virtual void setState(ConnectionState state) = 0;
    virtual void handleError(const QString& error) = 0;
    virtual void updateLastActivity() = 0;
    virtual void logMessage(const QString& message, const QString& level = "INFO") = 0;
    
    // 验证方法
    virtual bool validateConfig(const CommunicationConfig& config) const = 0;
    virtual bool validateData(const QByteArray& data) const = 0;
    virtual bool isValidFrame(const ProtocolFrame& frame) const = 0;
    
    // 内部状态
    ConnectionState m_connectionState = ConnectionState::Disconnected;
    QString m_lastError;
    CommunicationStats m_statistics;
    bool m_autoReconnectEnabled = true;
    bool m_heartbeatEnabled = true;
    int m_currentReconnectAttempts = 0;
    qint64 m_lastHeartbeatTime = 0;
    
private:
    // 禁用复制构造函数和赋值操作符
    ICommunication(const ICommunication&) = delete;
    ICommunication& operator=(const ICommunication&) = delete;
};

// 工厂模式创建通讯对象
class CommunicationFactory
{
public:
    static ICommunication* createCommunication(CommunicationType type, QObject* parent = nullptr);
    static QStringList getSupportedTypes();
    static QString getTypeDescription(CommunicationType type);
    static bool isTypeSupported(CommunicationType type);
    
private:
    CommunicationFactory() = delete;  // 静态工厂类，禁止实例化
};

// 辅助函数
QString connectionStateToString(ConnectionState state);
QString communicationTypeToString(CommunicationType type);
CommunicationType stringToCommunicationType(const QString& typeString);
ConnectionState stringToConnectionState(const QString& stateString); 