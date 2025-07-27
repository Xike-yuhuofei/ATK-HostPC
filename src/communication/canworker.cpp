#include "canworker.h"
#include <QCanBus>
#include <QCanBusDeviceInfo>
#include <QCanBusFrame>
#include <QDebug>
#include <QJsonDocument>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(canWorker, "communication.can")

CanWorker::CanWorker(QObject *parent)
    : QObject(parent)
    , m_canDevice(nullptr)
    , m_bitrate(250000)
    , m_deviceStatus(CanDeviceStatus::Disconnected)
    , m_heartbeatTimer(new QTimer(this))
    , m_timeoutTimer(new QTimer(this))
    , m_reconnectTimer(new QTimer(this))
    , m_heartbeatInterval(5000)
    , m_timeoutInterval(10000)
    , m_autoReconnect(true)
    , m_logLevel(1)
    , m_maxRetries(3)
    , m_sentMessageCount(0)
    , m_receivedMessageCount(0)
    , m_errorCount(0)
    , m_isRunning(false)
    , m_isConnected(false)
    , m_reconnectAttempts(0)
{
    setupTimers();
}

CanWorker::~CanWorker()
{
    stopWorker();
    disconnectFromDevice();
}

bool CanWorker::connectToDevice(const QString& plugin, const QString& interface, int bitrate)
{
    if (m_isConnected) {
        qCWarning(canWorker, "Already connected to CAN device");
        return false;
    }

    m_plugin = plugin;
    m_interface = interface;
    m_bitrate = bitrate;

    // 创建CAN设备
    QString errorString;
    m_canDevice = QCanBus::instance()->createDevice(plugin, interface, &errorString);
    
    if (!m_canDevice) {
        qCCritical(canWorker, "Failed to create CAN device: %s", qPrintable(errorString));
        handleDeviceError(errorString);
        return false;
    }

    // 配置设备
    m_canDevice->setConfigurationParameter(QCanBusDevice::BitRateKey, bitrate);
    
    // 连接信号
    connect(m_canDevice, &QCanBusDevice::framesReceived, 
            this, &CanWorker::onFramesReceived);
    connect(m_canDevice, &QCanBusDevice::errorOccurred,
            this, &CanWorker::onErrorOccurred);
    connect(m_canDevice, &QCanBusDevice::stateChanged,
            this, &CanWorker::onStateChanged);

    // 连接设备
    if (!m_canDevice->connectDevice()) {
        QString error = m_canDevice->errorString();
        qCCritical(canWorker, "Failed to connect CAN device: %s", qPrintable(error));
        handleDeviceError(error);
        delete m_canDevice;
        m_canDevice = nullptr;
        return false;
    }

    m_isConnected = true;
    m_deviceStatus = CanDeviceStatus::Connected;
    m_reconnectAttempts = 0;
    
    qCInfo(canWorker, "Connected to CAN device: %s %s at %d bps", qPrintable(plugin), qPrintable(interface), bitrate);
    emit deviceConnected(QString("%1:%2").arg(plugin, interface));
    
    return true;
}

void CanWorker::disconnectFromDevice()
{
    if (m_canDevice) {
        m_canDevice->disconnectDevice();
        delete m_canDevice;
        m_canDevice = nullptr;
    }
    
    m_isConnected = false;
    m_deviceStatus = CanDeviceStatus::Disconnected;
    
    emit deviceDisconnected(QString("%1:%2").arg(m_plugin, m_interface));
}

bool CanWorker::isConnected() const
{
    return m_isConnected && m_canDevice && 
           m_canDevice->state() == QCanBusDevice::ConnectedState;
}

CanDeviceStatus CanWorker::getDeviceStatus() const
{
    return m_deviceStatus;
}

bool CanWorker::sendMessage(quint32 canId, const QByteArray& data, bool isExtended)
{
    if (!isConnected()) {
        qCWarning(canWorker, "CAN device not connected");
        return false;
    }

    QCanBusFrame frame(canId, data);
    frame.setExtendedFrameFormat(isExtended);
    
    if (!m_canDevice->writeFrame(frame)) {
        qCWarning(canWorker, "Failed to write CAN frame: %s", qPrintable(m_canDevice->errorString()));
        updateStatistics(true, true);
        return false;
    }
    
    updateStatistics(true, false);
    
    CanMessage message;
    message.canId = canId;
    message.data = data;
    message.timestamp = QDateTime::currentDateTime();
    message.isExtended = isExtended;
    message.isRemote = false;
    message.isError = false;
    
    logMessage(message, true);
    emit messageSent(message);
    
    return true;
}

bool CanWorker::sendMotionControl(quint8 deviceId, quint8 command, const QByteArray& parameters)
{
    quint32 canId = static_cast<quint32>(CanMessageType::MotionControl) + deviceId;
    QByteArray data;
    data.append(command);
    data.append(parameters);
    
    return sendMessage(canId, data);
}

bool CanWorker::sendGlueControl(quint8 deviceId, quint8 command, const QByteArray& parameters)
{
    quint32 canId = static_cast<quint32>(CanMessageType::GlueControl) + deviceId;
    QByteArray data;
    data.append(command);
    data.append(parameters);
    
    return sendMessage(canId, data);
}

bool CanWorker::sendHeartbeat(quint8 deviceId)
{
    quint32 canId = static_cast<quint32>(CanMessageType::Heartbeat) + deviceId;
    QByteArray data;
    data.append(static_cast<char>(0x01)); // 心跳标识
    
    return sendMessage(canId, data);
}

bool CanWorker::sendEmergencyStop(quint8 deviceId)
{
    quint32 canId = static_cast<quint32>(CanMessageType::Emergency) + deviceId;
    QByteArray data;
    data.append(static_cast<char>(0xFF)); // 紧急停止标识
    
    return sendMessage(canId, data);
}

void CanWorker::startWorker()
{
    if (m_isRunning) {
        return;
    }
    
    m_isRunning = true;
    
    if (m_autoReconnect) {
        m_heartbeatTimer->start(m_heartbeatInterval);
        m_timeoutTimer->start(m_timeoutInterval);
    }
    
    qCInfo(canWorker, "CAN worker started");
}

void CanWorker::stopWorker()
{
    if (!m_isRunning) {
        return;
    }
    
    m_isRunning = false;
    
    m_heartbeatTimer->stop();
    m_timeoutTimer->stop();
    m_reconnectTimer->stop();
    
    qCInfo(canWorker, "CAN worker stopped");
}

void CanWorker::onFramesReceived()
{
    if (!m_canDevice) {
        return;
    }
    
    while (m_canDevice->framesAvailable()) {
        QCanBusFrame frame = m_canDevice->readFrame();
        
        CanMessage message;
        message.canId = frame.frameId();
        message.data = frame.payload();
        message.timestamp = QDateTime::currentDateTime();
        message.isExtended = frame.hasExtendedFrameFormat();
        message.isRemote = frame.frameType() == QCanBusFrame::RemoteRequestFrame;
        message.isError = frame.frameType() == QCanBusFrame::ErrorFrame;
        
        if (message.isError) {
            qCWarning(canWorker, "Received CAN error frame: %s", qPrintable(frame.toString()));
            updateStatistics(false, true);
            continue;
        }
        
        updateStatistics(false, false);
        logMessage(message, false);
        
        emit messageReceived(message);
        processMessage(message);
    }
}

void CanWorker::onErrorOccurred(QCanBusDevice::CanBusError error)
{
    QString errorString = m_canDevice ? m_canDevice->errorString() : "Unknown error";
    qCWarning(canWorker, "CAN device error: %d %s", static_cast<int>(error), qPrintable(errorString));
    
    handleDeviceError(errorString);
    
    if (error == QCanBusDevice::ConnectionError) {
        m_deviceStatus = CanDeviceStatus::Error;
        m_isConnected = false;
        
        if (m_autoReconnect) {
            attemptReconnection();
        }
    }
}

void CanWorker::onStateChanged(QCanBusDevice::CanBusDeviceState state)
{
    qCInfo(canWorker, "CAN device state changed: %d", static_cast<int>(state));
    
    switch (state) {
    case QCanBusDevice::ConnectedState:
        m_isConnected = true;
        m_deviceStatus = CanDeviceStatus::Connected;
        break;
    case QCanBusDevice::UnconnectedState:
        m_isConnected = false;
        m_deviceStatus = CanDeviceStatus::Disconnected;
        break;
    case QCanBusDevice::ConnectingState:
        m_deviceStatus = CanDeviceStatus::Connecting;
        break;
    case QCanBusDevice::ClosingState:
        m_deviceStatus = CanDeviceStatus::Disconnected;
        break;
    }
}

void CanWorker::processMessage(const CanMessage& message)
{
    if (!validateMessage(message)) {
        return;
    }
    
    // 检查消息过滤器
    if (!checkMessageFilter(message.canId)) {
        return;
    }
    
    // 根据消息类型处理
    CanMessageType msgType = static_cast<CanMessageType>(message.canId & 0xFF00);
    quint8 deviceId = static_cast<quint8>(message.canId & 0xFF);
    
    switch (msgType) {
    case CanMessageType::MotionControl:
        processMotionControlMessage(message);
        emit motionControlReceived(deviceId, message.data.at(0), message.data.mid(1));
        break;
    case CanMessageType::GlueControl:
        processGlueControlMessage(message);
        emit glueControlReceived(deviceId, message.data.at(0), message.data.mid(1));
        break;
    case CanMessageType::SystemStatus:
        processSystemStatusMessage(message);
        break;
    case CanMessageType::Heartbeat:
        processHeartbeatMessage(message);
        emit heartbeatReceived(deviceId);
        break;
    case CanMessageType::Emergency:
        processEmergencyMessage(message);
        emit emergencyStopReceived(deviceId);
        break;
    default:
        qCDebug(canWorker, "Unknown message type: %d", static_cast<int>(msgType));
        break;
    }
}

void CanWorker::setupTimers()
{
    // 心跳定时器
    connect(m_heartbeatTimer, &QTimer::timeout, this, &CanWorker::onHeartbeatTimer);
    
    // 超时定时器
    connect(m_timeoutTimer, &QTimer::timeout, this, [this]() {
        if (m_isConnected) {
            qCWarning(canWorker, "CAN communication timeout");
            emit deviceTimeout(QString("%1:%2").arg(m_plugin, m_interface));
        }
    });
    
    // 重连定时器
    connect(m_reconnectTimer, &QTimer::timeout, this, &CanWorker::onReconnectTimer);
}

void CanWorker::onHeartbeatTimer()
{
    // 发送心跳给所有设备
    for (const auto& device : m_devices) {
        if (device.isConnected) {
            // 从设备名称中提取设备ID（简化实现）
            quint8 deviceId = 1; // 实际应该从设备配置中获取
            sendHeartbeat(deviceId);
        }
    }
}

void CanWorker::onReconnectTimer()
{
    attemptReconnection();
}

void CanWorker::attemptReconnection()
{
    if (m_reconnectAttempts >= m_maxRetries) {
        qCCritical(canWorker, "Max reconnection attempts reached");
        m_reconnectTimer->stop();
        return;
    }
    
    m_reconnectAttempts++;
    qCInfo(canWorker, "Attempting reconnection %d of %d", m_reconnectAttempts, m_maxRetries);
    
    disconnectFromDevice();
    
    if (connectToDevice(m_plugin, m_interface, m_bitrate)) {
        m_reconnectTimer->stop();
        m_reconnectAttempts = 0;
    } else {
        m_reconnectTimer->start(5000); // 5秒后重试
    }
}

bool CanWorker::validateMessage(const CanMessage& message)
{
    // 基本验证
    if (message.data.size() > 8) {
        qCWarning(canWorker, "Invalid CAN message: data too long");
        return false;
    }
    
    return true;
}

bool CanWorker::checkMessageFilter(quint32 canId)
{
    QMutexLocker locker(&m_filtersMutex);
    
    if (m_messageFilters.isEmpty()) {
        return true; // 没有过滤器，接受所有消息
    }
    
    for (const auto& filter : m_messageFilters) {
        if ((canId & filter.second) == (filter.first & filter.second)) {
            return true;
        }
    }
    
    return false;
}

void CanWorker::updateStatistics(bool sent, bool error)
{
    if (sent) {
        m_sentMessageCount++;
    } else {
        m_receivedMessageCount++;
    }
    
    if (error) {
        m_errorCount++;
    }
    
    m_lastMessageTime = QDateTime::currentDateTime();
    
    emit statisticsUpdated(m_sentMessageCount, m_receivedMessageCount, m_errorCount);
}

void CanWorker::logMessage(const CanMessage& message, bool sent)
{
    if (m_logLevel < 2) {
        return;
    }
    
    QString direction = sent ? "TX" : "RX";
    QString logMsg = QString("%1: ID=0x%2 Data=%3")
                    .arg(direction)
                    .arg(message.canId, 0, 16)
                    .arg(formatData(message.data));
    
    emit logMessage(logMsg);
}

void CanWorker::handleDeviceError(const QString& error)
{
    m_lastError = error;
    m_errorCount++;
    emit deviceError(QString("%1:%2").arg(m_plugin, m_interface), error);
}

QString CanWorker::formatData(const QByteArray& data)
{
    QString result;
    for (int i = 0; i < data.size(); ++i) {
        if (i > 0) result += " ";
        result += QString("%1").arg(static_cast<quint8>(data.at(i)), 2, 16, QChar('0')).toUpper();
    }
    return result;
}

void CanWorker::processMotionControlMessage(const CanMessage& message)
{
    // 处理运动控制消息的具体逻辑
    Q_UNUSED(message)
}

void CanWorker::processGlueControlMessage(const CanMessage& message)
{
    // 处理点胶控制消息的具体逻辑
    Q_UNUSED(message)
}

void CanWorker::processSystemStatusMessage(const CanMessage& message)
{
    // 处理系统状态消息的具体逻辑
    Q_UNUSED(message)
}

void CanWorker::processHeartbeatMessage(const CanMessage& message)
{
    // 处理心跳消息的具体逻辑
    Q_UNUSED(message)
}

void CanWorker::processEmergencyMessage(const CanMessage& message)
{
    // 处理紧急停止消息的具体逻辑
    Q_UNUSED(message)
}

void CanWorker::resetStatistics()
{
    m_sentMessageCount = 0;
    m_receivedMessageCount = 0;
    m_errorCount = 0;
    m_lastMessageTime = QDateTime::currentDateTime();
}

void CanWorker::addMessageFilter(quint32 canId, quint32 mask)
{
    QMutexLocker locker(&m_filtersMutex);
    m_messageFilters.append(qMakePair(canId, mask));
}

void CanWorker::removeMessageFilter(quint32 canId)
{
    QMutexLocker locker(&m_filtersMutex);
    for (int i = 0; i < m_messageFilters.size(); ++i) {
        if (m_messageFilters.at(i).first == canId) {
            m_messageFilters.removeAt(i);
            break;
        }
    }
}

void CanWorker::clearMessageFilters()
{
    QMutexLocker locker(&m_filtersMutex);
    m_messageFilters.clear();
} 