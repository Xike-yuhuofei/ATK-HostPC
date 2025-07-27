#ifndef CANWORKER_H
#define CANWORKER_H

#include <QObject>
#include <QThread>
#include <QTimer>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>
#include <QCanBusDevice>
#include <QCanBusFrame>
#include <QCanBusDeviceInfo>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDateTime>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(canWorker)

// CAN消息类型
enum class CanMessageType {
    MotionControl = 0x100,      // 运动控制
    GlueControl = 0x200,        // 点胶控制
    SystemStatus = 0x300,       // 系统状态
    ParameterSet = 0x400,       // 参数设置
    DataQuery = 0x500,          // 数据查询
    AlarmReport = 0x600,        // 报警上报
    Heartbeat = 0x700,          // 心跳包
    Emergency = 0x080           // 紧急停止
};

// CAN设备状态
enum class CanDeviceStatus {
    Disconnected = 0,           // 未连接
    Connecting = 1,             // 连接中
    Connected = 2,              // 已连接
    Error = 3,                  // 错误状态
    Timeout = 4                 // 超时
};

// CAN消息结构
struct CanMessage {
    quint32 canId;              // CAN ID
    QByteArray data;            // 数据
    QDateTime timestamp;        // 时间戳
    bool isExtended;            // 是否扩展帧
    bool isRemote;              // 是否远程帧
    bool isError;               // 是否错误帧
    QString description;        // 消息描述
};

// CAN设备信息
struct CanDeviceInfo {
    QString name;               // 设备名称
    QString plugin;             // 插件名称
    QString interface;          // 接口名称
    int bitrate;                // 波特率
    bool isConnected;           // 连接状态
    CanDeviceStatus status;     // 设备状态
    QDateTime lastHeartbeat;    // 最后心跳时间
    int errorCount;             // 错误计数
    int messageCount;           // 消息计数
    QString description;        // 设备描述
};

class CanWorker : public QObject
{
    Q_OBJECT

public:
    explicit CanWorker(QObject *parent = nullptr);
    ~CanWorker();

    // 连接管理
    bool connectToDevice(const QString& plugin, const QString& interface, int bitrate = 250000);
    void disconnectFromDevice();
    bool isConnected() const;
    CanDeviceStatus getDeviceStatus() const;
    
    // 消息发送
    bool sendMessage(quint32 canId, const QByteArray& data, bool isExtended = false);
    bool sendMotionControl(quint8 deviceId, quint8 command, const QByteArray& parameters);
    bool sendGlueControl(quint8 deviceId, quint8 command, const QByteArray& parameters);
    bool sendParameterSet(quint8 deviceId, quint16 paramId, const QVariant& value);
    bool sendDataQuery(quint8 deviceId, quint16 queryType);
    bool sendHeartbeat(quint8 deviceId);
    bool sendEmergencyStop(quint8 deviceId);
    
    // 设备管理
    QList<CanDeviceInfo> getAvailableDevices() const;
    CanDeviceInfo getDeviceInfo(const QString& name) const;
    bool addDevice(const CanDeviceInfo& deviceInfo);
    bool removeDevice(const QString& name);
    void updateDeviceStatus(const QString& name, CanDeviceStatus status);
    
    // 消息过滤
    void addMessageFilter(quint32 canId, quint32 mask = 0xFFFFFFFF);
    void removeMessageFilter(quint32 canId);
    void clearMessageFilters();
    
    // 数据处理
    QJsonObject parseMessage(const CanMessage& message);
    CanMessage createMessage(CanMessageType type, quint8 deviceId, const QByteArray& data);
    QByteArray encodeMotionCommand(quint8 command, double positionX, double positionY, double positionZ, double speed);
    QByteArray encodeGlueCommand(quint8 command, double volume, double pressure, double temperature);
    QByteArray encodeParameterData(quint16 paramId, const QVariant& value);
    
    // 统计信息
    int getSentMessageCount() const { return m_sentMessageCount; }
    int getReceivedMessageCount() const { return m_receivedMessageCount; }
    int getErrorCount() const { return m_errorCount; }
    void resetStatistics();
    
    // 配置管理
    void setHeartbeatInterval(int interval) { m_heartbeatInterval = interval; }
    void setTimeoutInterval(int interval) { m_timeoutInterval = interval; }
    void setAutoReconnect(bool enable) { m_autoReconnect = enable; }
    void setLogLevel(int level) { m_logLevel = level; }

public slots:
    void startWorker();
    void stopWorker();
    void processMessage(const CanMessage& message);
    void onDeviceTimeout(const QString& deviceName);
    void onHeartbeatTimer();
    void onReconnectTimer();

private slots:
    void onFramesReceived();
    void onErrorOccurred(QCanBusDevice::CanBusError error);
    void onStateChanged(QCanBusDevice::CanBusDeviceState state);

private:
    void initializeDevice();
    void setupTimers();
    void setupConnections();
    void processReceivedFrames();
    void processMotionControlMessage(const CanMessage& message);
    void processGlueControlMessage(const CanMessage& message);
    void processSystemStatusMessage(const CanMessage& message);
    void processParameterSetMessage(const CanMessage& message);
    void processDataQueryMessage(const CanMessage& message);
    void processAlarmReportMessage(const CanMessage& message);
    void processHeartbeatMessage(const CanMessage& message);
    void processEmergencyMessage(const CanMessage& message);
    
    bool validateMessage(const CanMessage& message);
    bool checkMessageFilter(quint32 canId);
    void updateStatistics(bool sent, bool error = false);
    void logMessage(const CanMessage& message, bool sent);
    void handleDeviceError(const QString& error);
    void attemptReconnection();
    
    quint8 calculateChecksum(const QByteArray& data);
    QByteArray addChecksum(const QByteArray& data);
    bool verifyChecksum(const QByteArray& data);
    
    QString formatCanId(quint32 canId);
    QString formatData(const QByteArray& data);
    QString formatTimestamp(const QDateTime& timestamp);
    QString getMessageTypeString(CanMessageType type);
    QString getDeviceStatusString(CanDeviceStatus status);

private:
    // CAN设备
    QCanBusDevice* m_canDevice;
    QString m_plugin;
    QString m_interface;
    int m_bitrate;
    CanDeviceStatus m_deviceStatus;
    
    // 设备列表
    QList<CanDeviceInfo> m_devices;
    QMutex m_devicesMutex;
    
    // 消息队列
    QQueue<CanMessage> m_sendQueue;
    QQueue<CanMessage> m_receiveQueue;
    QMutex m_sendQueueMutex;
    QMutex m_receiveQueueMutex;
    QWaitCondition m_sendCondition;
    
    // 消息过滤
    QList<QPair<quint32, quint32>> m_messageFilters; // ID, Mask
    QMutex m_filtersMutex;
    
    // 定时器
    QTimer* m_heartbeatTimer;
    QTimer* m_timeoutTimer;
    QTimer* m_reconnectTimer;
    
    // 配置参数
    int m_heartbeatInterval;    // 心跳间隔(ms)
    int m_timeoutInterval;      // 超时间隔(ms)
    bool m_autoReconnect;       // 自动重连
    int m_logLevel;             // 日志级别
    int m_maxRetries;           // 最大重试次数
    
    // 统计信息
    int m_sentMessageCount;
    int m_receivedMessageCount;
    int m_errorCount;
    QDateTime m_lastMessageTime;
    
    // 状态变量
    bool m_isRunning;
    bool m_isConnected;
    int m_reconnectAttempts;
    QString m_lastError;
    
    // 线程同步
    QMutex m_mutex;
    QWaitCondition m_condition;

signals:
    void deviceConnected(const QString& deviceName);
    void deviceDisconnected(const QString& deviceName);
    void deviceError(const QString& deviceName, const QString& error);
    void deviceTimeout(const QString& deviceName);
    void messageReceived(const CanMessage& message);
    void messageSent(const CanMessage& message);
    void motionControlReceived(quint8 deviceId, quint8 command, const QByteArray& data);
    void glueControlReceived(quint8 deviceId, quint8 command, const QByteArray& data);
    void systemStatusReceived(quint8 deviceId, const QJsonObject& status);
    void parameterSetReceived(quint8 deviceId, quint16 paramId, const QVariant& value);
    void dataQueryReceived(quint8 deviceId, quint16 queryType, const QByteArray& data);
    void alarmReportReceived(quint8 deviceId, quint16 alarmCode, const QString& message);
    void heartbeatReceived(quint8 deviceId);
    void emergencyStopReceived(quint8 deviceId);
    void statisticsUpdated(int sent, int received, int errors);
    void logMessage(const QString& message);
};

#endif // CANWORKER_H 