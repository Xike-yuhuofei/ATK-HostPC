#pragma once

#include "icommunication.h"
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>
#include <QMutex>
#include <QQueue>

// 串口特定配置
struct SerialConfig : public CommunicationConfig {
    QString portName;
    qint32 baudRate;
    QSerialPort::DataBits dataBits;
    QSerialPort::Parity parity;
    QSerialPort::StopBits stopBits;
    QSerialPort::FlowControl flowControl;
    
    SerialConfig() : CommunicationConfig() {
        type = CommunicationType::Serial;
        name = "Serial";
        portName = "COM1";
        baudRate = Communication::DEFAULT_BAUD_RATE;
        dataBits = static_cast<QSerialPort::DataBits>(Communication::DEFAULT_DATA_BITS);
        parity = QSerialPort::NoParity;
        stopBits = static_cast<QSerialPort::StopBits>(Communication::DEFAULT_STOP_BITS);
        flowControl = QSerialPort::NoFlowControl;
    }
    
    // 从基类转换
    static SerialConfig fromBase(const CommunicationConfig& base) {
        SerialConfig config;
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

class SerialCommunication : public ICommunication
{
    Q_OBJECT

public:
    explicit SerialCommunication(QObject* parent = nullptr);
    ~SerialCommunication() override;

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

    // 串口特定功能
    bool setPortName(const QString& portName);
    bool setBaudRate(qint32 baudRate);
    bool setDataBits(QSerialPort::DataBits dataBits);
    bool setParity(QSerialPort::Parity parity);
    bool setStopBits(QSerialPort::StopBits stopBits);
    bool setFlowControl(QSerialPort::FlowControl flowControl);
    
    QString getPortName() const;
    qint32 getBaudRate() const;
    QSerialPort::DataBits getDataBits() const;
    QSerialPort::Parity getParity() const;
    QSerialPort::StopBits getStopBits() const;
    QSerialPort::FlowControl getFlowControl() const;

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
    
    // 串口特定槽函数
    void onSerialDataReceived();
    void onSerialBytesWritten(qint64 bytes);
    void onSerialErrorOccurred(QSerialPort::SerialPortError error);

protected:
    void setState(ConnectionState state) override;
    void handleError(const QString& error) override;
    void updateLastActivity() override;
    void logMessage(const QString& message, const QString& level = "INFO") override;
    
    // 验证方法
    bool validateConfig(const CommunicationConfig& config) const override;
    bool validateData(const QByteArray& data) const override;
    bool isValidFrame(const ProtocolFrame& frame) const override;
    
    // 串口特定方法
    bool openSerialPort();
    void closeSerialPort();
    void configureSerialPort();
    void startConnectionTimer();
    void stopConnectionTimer();
    void startReconnectTimer();
    void stopReconnectTimer();
    QString serialErrorToString(QSerialPort::SerialPortError error) const;

private:
    // 串口对象
    QSerialPort* m_serialPort;
    ProtocolParser* m_protocolParser;
    
    // 配置
    SerialConfig m_config;
    
    // 定时器
    QTimer* m_heartbeatTimer;
    QTimer* m_reconnectTimer;
    QTimer* m_connectionTimer;
    QTimer* m_statisticsTimer;
    
    // 线程安全
    QMutex m_dataMutex;
    QQueue<QByteArray> m_sendQueue;
    QByteArray m_receiveBuffer;
    
    // 属性存储
    QVariantMap m_properties;
    
    // 辅助方法
    void initializeTimers();
    void connectSignals();
    void processReceivedData(const QByteArray& data);
    void updateConnectionStatistics();
    void calculateLatency();

private:
    // 连接状态
    bool m_isConnecting;
    qint64 m_connectStartTime;
}; 