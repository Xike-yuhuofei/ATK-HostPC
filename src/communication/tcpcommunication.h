#pragma once

#include "icommunication.h"
#include <QTcpSocket>
#include <QHostAddress>
#include <QTimer>
#include <QMutex>
#include <QQueue>

// TCP特定配置
struct TcpConfig : public CommunicationConfig {
    QString hostAddress;
    quint16 port;
    int connectTimeout;
    int readTimeout;
    int writeTimeout;
    bool keepAlive;
    
    TcpConfig() : CommunicationConfig() {
        type = CommunicationType::TCP;
        name = "TCP";
        hostAddress = "127.0.0.1";
        port = Communication::DEFAULT_TCP_PORT;
        connectTimeout = Communication::TCP_CONNECT_TIMEOUT;
        readTimeout = Communication::TCP_READ_TIMEOUT;
        writeTimeout = Communication::TCP_READ_TIMEOUT;
        keepAlive = true;
    }
    
    // 从基类转换
    static TcpConfig fromBase(const CommunicationConfig& base) {
        TcpConfig config;
        config.name = base.name;
        config.autoReconnect = base.autoReconnect;
        config.timeout = base.timeout;
        config.reconnectInterval = base.reconnectInterval;
        config.maxReconnectAttempts = base.maxReconnectAttempts;
        config.enableHeartbeat = base.enableHeartbeat;
        config.heartbeatInterval = base.heartbeatInterval;
        return config;
    }
};

class TcpCommunication : public ICommunication
{
    Q_OBJECT

public:
    explicit TcpCommunication(QObject* parent = nullptr);
    ~TcpCommunication() override;

    // ICommunication接口实现
    bool connect(const CommunicationConfig& config) override;
    void disconnect() override;
    bool isConnected() const override;
    ConnectionState getConnectionState() const override;
    CommunicationType getType() const override;
    QString getName() const override;
    
    // 数据传输
    bool sendData(const QByteArray& data) override;
    bool sendFrame(ProtocolCommand command, const QByteArray& data = QByteArray()) override;
    QByteArray receiveData() override;
    
    // 配置管理
    void setConfig(const CommunicationConfig& config) override;
    CommunicationConfig getConfig() const override;
    bool updateConfig(const QString& key, const QVariant& value) override;
    
    // 状态查询
    QString getLastError() const override;
    CommunicationStats getStatistics() const override;
    void resetStatistics() override;
    
    // 心跳管理
    void enableHeartbeat(bool enabled) override;
    bool isHeartbeatEnabled() const override;
    void sendHeartbeat() override;
    qint64 getLastHeartbeatTime() const override;
    
    // 重连管理
    void enableAutoReconnect(bool enabled) override;
    bool isAutoReconnectEnabled() const override;
    void setMaxReconnectAttempts(int maxAttempts) override;
    int getCurrentReconnectAttempts() const override;
    void resetReconnectAttempts() override;
    
    // 高级功能
    void flush() override;
    void clearBuffers() override;
    bool testConnection() override;
    QStringList getAvailableConnections() const override;
    void setProperty(const QString& name, const QVariant& value) override;
    QVariant getProperty(const QString& name) const override;

    // TCP特定功能
    bool setHostAddress(const QString& address);
    bool setPort(quint16 port);
    bool setConnectTimeout(int timeout);
    bool setKeepAlive(bool enabled);
    
    QString getHostAddress() const;
    quint16 getPort() const;
    int getConnectTimeout() const;
    bool isKeepAliveEnabled() const;
    
    // 网络状态
    QString getPeerAddress() const;
    quint16 getPeerPort() const;
    QString getLocalAddress() const;
    quint16 getLocalPort() const;

public slots:
    void reconnect() override;
    void startHeartbeat() override;
    void stopHeartbeat() override;
    void updateStatistics() override;

protected slots:
    void onHeartbeatTimer() override;
    void onReconnectTimer() override;
    void onConnectionTimeout() override;
    void onDataReceived() override;
    void onErrorOccurred() override;
    
    // TCP特定槽函数
    void onTcpConnected();
    void onTcpDisconnected();
    void onTcpDataReceived();
    void onTcpBytesWritten(qint64 bytes);
    void onTcpErrorOccurred(QAbstractSocket::SocketError error);
    void onTcpStateChanged(QAbstractSocket::SocketState state);

protected:
    void setState(ConnectionState state) override;
    void handleError(const QString& error) override;
    void updateLastActivity() override;
    void logMessage(const QString& message, const QString& level = "INFO") override;
    
    // 验证方法
    bool validateConfig(const CommunicationConfig& config) const override;
    bool validateData(const QByteArray& data) const override;
    bool isValidFrame(const ProtocolFrame& frame) const override;
    
    // TCP特定方法
    bool connectToHost();
    void disconnectFromHost();
    void configureTcpSocket();
    void startConnectionTimer();
    void stopConnectionTimer();
    void startReconnectTimer();
    void stopReconnectTimer();
    QString tcpErrorToString(QAbstractSocket::SocketError error) const;
    QString tcpStateToString(QAbstractSocket::SocketState state) const;

private:
    // TCP对象
    QTcpSocket* m_tcpSocket;
    ProtocolParser* m_protocolParser;
    
    // 配置
    TcpConfig m_config;
    
    // 定时器
    QTimer* m_heartbeatTimer;
    QTimer* m_reconnectTimer;
    QTimer* m_connectionTimer;
    QTimer* m_statisticsTimer;
    QTimer* m_keepAliveTimer;
    
    // 线程安全
    QMutex m_dataMutex;
    QQueue<QByteArray> m_sendQueue;
    QByteArray m_receiveBuffer;
    
    // 属性存储
    QVariantMap m_properties;
    
    // 状态追踪
    bool m_isConnecting;
    qint64 m_connectStartTime;
    
    // 辅助方法
    void initializeTimers();
    void connectSignals();
    void processReceivedData(const QByteArray& data);
    void updateConnectionStatistics();
    void calculateLatency();
    void sendKeepAlive();
}; 