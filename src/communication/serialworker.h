#pragma once

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>
#include <QMutex>
#include <QQueue>
#include <QThread>
#include "protocolparser.h"
#include "constants.h"

enum class SerialConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Reconnecting,
    Error
};

#include "serialcommunication.h"

class SerialWorker : public QObject
{
    Q_OBJECT

public:
    explicit SerialWorker(QObject* parent = nullptr);
    ~SerialWorker();
    
    // 连接管理
    bool openPort(const SerialConfig& config);
    void closePort();
    bool isConnected() const;
    
    // 数据发送
    bool sendData(const QByteArray& data);
    bool sendFrame(ProtocolCommand command, const QByteArray& data = QByteArray());
    
    // 配置管理
    void setConfig(const SerialConfig& config);
    SerialConfig getConfig() const;
    
    // 状态查询
    SerialConnectionState getConnectionState() const;
    QString getLastError() const;
    
    // 可用串口列表
    static QStringList getAvailablePorts();
    
    // 自动重连
    void setAutoReconnect(bool enabled);
    bool getAutoReconnect() const;
    
    // 重连次数控制
    void setMaxReconnectAttempts(int maxAttempts);
    int getMaxReconnectAttempts() const;
    int getCurrentReconnectAttempts() const;
    void resetReconnectAttempts();
    
    // 静默模式控制（减少弹窗）
    void setSilentMode(bool silent);
    bool getSilentMode() const;
    
    // 统计信息
    qint64 getBytesReceived() const;
    qint64 getBytesSent() const;
    void resetStatistics();

signals:
    void connected();
    void disconnected();
    void connectionStateChanged(SerialConnectionState state);
    void dataReceived(const QByteArray& data);
    void frameReceived(const ProtocolFrame& frame);
    void errorOccurred(const QString& error);
    void bytesWritten(qint64 bytes);
    void statisticsUpdated(qint64 received, qint64 sent);

private slots:
    void onReadyRead();
    void onBytesWritten(qint64 bytes);
    void onErrorOccurred(QSerialPort::SerialPortError error);
    void onReconnectTimer();
    void onProtocolFrameReceived(const ProtocolFrame& frame);
    void onProtocolParseError(const QString& error);
    void onConnectionTimeout();

private:
    void setState(SerialConnectionState state);
    void handleError(const QString& error);
    void startReconnectTimer();
    void stopReconnectTimer();
    void updateStatistics();
    
    QSerialPort* serialPort;
    ProtocolParser* protocolParser;
    SerialConfig config;
    SerialConnectionState connectionState;
    QString lastError;
    
    QTimer* reconnectTimer;
    QTimer* connectionTimer;
    
    QMutex dataMutex;
    QQueue<QByteArray> sendQueue;
    
    // 重连控制
    int maxReconnectAttempts;
    int currentReconnectAttempts;
    bool silentMode;
    
    // 统计信息
    qint64 bytesReceived;
    qint64 bytesSent;
    QTimer* statisticsTimer;
    
    // 常量已移动到 constants.h
}; 