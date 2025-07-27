#include "serialcommunication.h"
#include "logger/logmanager.h"
#include "constants.h"
#include <QDebug>
#include <QDateTime>
#include <QThread>

SerialCommunication::SerialCommunication(QObject* parent)
    : ICommunication(parent)
    , m_serialPort(nullptr)
    , m_protocolParser(nullptr)
    , m_heartbeatTimer(nullptr)
    , m_reconnectTimer(nullptr)
    , m_connectionTimer(nullptr)
    , m_statisticsTimer(nullptr)
    , m_isConnecting(false)
    , m_connectStartTime(0)
{
    // 创建串口对象
    m_serialPort = new QSerialPort(this);
    
    // 创建协议解析器
    m_protocolParser = new ProtocolParser(this);
    
    // 初始化定时器
    initializeTimers();
    
    // 连接信号
    connectSignals();
    
    // 设置初始状态
    setState(ConnectionState::Disconnected);
    
    LogManager::getInstance()->info("串口通讯对象已创建", "SerialCommunication");
}

SerialCommunication::~SerialCommunication()
{
    // 停止心跳
    stopHeartbeat();
    
    // 关闭连接
    if (isConnected()) {
        disconnect();
    }
    
    LogManager::getInstance()->info("串口通讯对象已销毁", "SerialCommunication");
}

bool SerialCommunication::connect(const CommunicationConfig& config)
{
    if (isConnected()) {
        LogManager::getInstance()->warning("串口已连接", "SerialCommunication");
        return true;
    }
    
    // 设置配置
    setConfig(config);
    
    // 验证配置
    if (!validateConfig(m_config)) {
        handleError("配置验证失败");
        return false;
    }
    
    // 开始连接
    setState(ConnectionState::Connecting);
    m_isConnecting = true;
    m_connectStartTime = QDateTime::currentMSecsSinceEpoch();
    
    // 启动连接超时定时器
    startConnectionTimer();
    
    // 打开串口
    if (openSerialPort()) {
        // 重置重连计数
        resetReconnectAttempts();
        
        // 启动心跳
        if (m_config.enableHeartbeat) {
            startHeartbeat();
        }
        
        // 启动统计定时器
        m_statisticsTimer->start(System::STATISTICS_UPDATE_INTERVAL);
        
        LogManager::getInstance()->info(
            QString("串口连接成功: %1").arg(m_config.portName),
            "SerialCommunication"
        );
        
        return true;
    } else {
        handleError("无法打开串口");
        return false;
    }
}

void SerialCommunication::disconnect()
{
    if (!isConnected()) {
        return;
    }
    
    // 停止所有定时器
    stopHeartbeat();
    stopConnectionTimer();
    stopReconnectTimer();
    m_statisticsTimer->stop();
    
    // 关闭串口
    closeSerialPort();
    
    // 设置状态
    setState(ConnectionState::Disconnected);
    m_isConnecting = false;
    
    LogManager::getInstance()->info("串口连接已断开", "SerialCommunication");
}

bool SerialCommunication::isConnected() const
{
    return m_serialPort && m_serialPort->isOpen() && (m_connectionState == ConnectionState::Connected);
}

ConnectionState SerialCommunication::getConnectionState() const
{
    return m_connectionState;
}

CommunicationType SerialCommunication::getType() const
{
    return CommunicationType::Serial;
}

QString SerialCommunication::getName() const
{
    return m_config.name;
}

bool SerialCommunication::sendData(const QByteArray& data)
{
    if (!validateData(data)) {
        return false;
    }
    
    if (!isConnected()) {
        handleError("串口未连接");
        return false;
    }
    
    QMutexLocker locker(&m_dataMutex);
    
    qint64 bytesWritten = m_serialPort->write(data);
    if (bytesWritten == -1) {
        handleError("数据发送失败");
        return false;
    }
    
    if (bytesWritten != data.size()) {
        handleError("数据发送不完整");
        return false;
    }
    
    // 更新统计信息
    m_statistics.bytesSent += bytesWritten;
    m_statistics.framesSent++;
    updateLastActivity();
    
    emit dataSent(data);
    
    return true;
}

bool SerialCommunication::sendFrame(ProtocolCommand command, const QByteArray& data)
{
    if (!m_protocolParser) {
        handleError("协议解析器未初始化");
        return false;
    }
    
    QByteArray frameData = m_protocolParser->buildFrame(command, data);
    if (frameData.isEmpty()) {
        handleError("构建协议帧失败");
        return false;
    }
    
    if (sendData(frameData)) {
        ProtocolFrame frame;
        frame.command = command;
        frame.data = data;
        frame.timestamp = QDateTime::currentDateTime();
        
        emit frameSent(frame);
        return true;
    }
    
    return false;
}

QByteArray SerialCommunication::receiveData()
{
    if (!isConnected()) {
        return QByteArray();
    }
    
    QMutexLocker locker(&m_dataMutex);
    QByteArray data = m_receiveBuffer;
    m_receiveBuffer.clear();
    
    return data;
}

void SerialCommunication::setConfig(const CommunicationConfig& config)
{
    // 转换为串口配置
    if (config.type == CommunicationType::Serial) {
        m_config = SerialConfig::fromBase(config);
    } else {
        m_config = SerialConfig::fromBase(config);
        m_config.type = CommunicationType::Serial;
    }
    
    // 如果已连接，需要重新配置
    if (isConnected()) {
        configureSerialPort();
    }
    
    emit configurationChanged();
}

CommunicationConfig SerialCommunication::getConfig() const
{
    return m_config;
}

bool SerialCommunication::updateConfig(const QString& key, const QVariant& value)
{
    bool updated = false;
    
    if (key == "portName") {
        m_config.portName = value.toString();
        updated = true;
    } else if (key == "baudRate") {
        m_config.baudRate = value.toInt();
        updated = true;
    } else if (key == "dataBits") {
        m_config.dataBits = static_cast<QSerialPort::DataBits>(value.toInt());
        updated = true;
    } else if (key == "parity") {
        m_config.parity = static_cast<QSerialPort::Parity>(value.toInt());
        updated = true;
    } else if (key == "stopBits") {
        m_config.stopBits = static_cast<QSerialPort::StopBits>(value.toInt());
        updated = true;
    } else if (key == "flowControl") {
        m_config.flowControl = static_cast<QSerialPort::FlowControl>(value.toInt());
        updated = true;
    } else if (key == "timeout") {
        m_config.timeout = value.toInt();
        updated = true;
    } else if (key == "autoReconnect") {
        m_config.autoReconnect = value.toBool();
        updated = true;
    } else if (key == "enableHeartbeat") {
        m_config.enableHeartbeat = value.toBool();
        if (m_config.enableHeartbeat) {
            startHeartbeat();
        } else {
            stopHeartbeat();
        }
        updated = true;
    } else if (key == "heartbeatInterval") {
        m_config.heartbeatInterval = value.toInt();
        if (m_heartbeatTimer && m_heartbeatTimer->isActive()) {
            m_heartbeatTimer->setInterval(m_config.heartbeatInterval);
        }
        updated = true;
    }
    
    if (updated) {
        emit propertyChanged(key, value);
        emit configurationChanged();
    }
    
    return updated;
}

QString SerialCommunication::getLastError() const
{
    return m_lastError;
}

CommunicationStats SerialCommunication::getStatistics() const
{
    return m_statistics;
}

void SerialCommunication::resetStatistics()
{
    m_statistics.reset();
    emit statisticsUpdated(m_statistics);
}

void SerialCommunication::enableHeartbeat(bool enabled)
{
    m_heartbeatEnabled = enabled;
    updateConfig("enableHeartbeat", enabled);
}

bool SerialCommunication::isHeartbeatEnabled() const
{
    return m_heartbeatEnabled;
}

void SerialCommunication::sendHeartbeat()
{
    if (!isConnected()) {
        return;
    }
    
    if (!m_protocolParser) {
        handleError("协议解析器未初始化");
        return;
    }
    
    QByteArray heartbeatData = m_protocolParser->buildHeartbeatFrame();
    if (sendData(heartbeatData)) {
        m_lastHeartbeatTime = QDateTime::currentMSecsSinceEpoch();
        logMessage("心跳包已发送", "DEBUG");
    } else {
        logMessage("心跳包发送失败", "WARNING");
    }
}

qint64 SerialCommunication::getLastHeartbeatTime() const
{
    return m_lastHeartbeatTime;
}

void SerialCommunication::enableAutoReconnect(bool enabled)
{
    m_autoReconnectEnabled = enabled;
    updateConfig("autoReconnect", enabled);
}

bool SerialCommunication::isAutoReconnectEnabled() const
{
    return m_autoReconnectEnabled;
}

void SerialCommunication::setMaxReconnectAttempts(int maxAttempts)
{
    m_config.maxReconnectAttempts = maxAttempts;
}

int SerialCommunication::getCurrentReconnectAttempts() const
{
    return m_currentReconnectAttempts;
}

void SerialCommunication::resetReconnectAttempts()
{
    m_currentReconnectAttempts = 0;
}

void SerialCommunication::flush()
{
    if (m_serialPort && m_serialPort->isOpen()) {
        m_serialPort->flush();
    }
}

void SerialCommunication::clearBuffers()
{
    QMutexLocker locker(&m_dataMutex);
    m_receiveBuffer.clear();
    m_sendQueue.clear();
    
    if (m_serialPort && m_serialPort->isOpen()) {
        m_serialPort->clear();
    }
}

bool SerialCommunication::testConnection()
{
    if (!isConnected()) {
        return false;
    }
    
    // 发送心跳包测试连接
    sendHeartbeat();
    
    // 等待一段时间看是否有响应
    QThread::msleep(100);
    
    return isConnected();
}

QStringList SerialCommunication::getAvailableConnections() const
{
    QStringList portList;
    for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts()) {
        portList << info.portName();
    }
    return portList;
}

void SerialCommunication::setProperty(const QString& name, const QVariant& value)
{
    m_properties[name] = value;
    emit propertyChanged(name, value);
}

QVariant SerialCommunication::getProperty(const QString& name) const
{
    return m_properties.value(name);
}

// 串口特定功能实现
bool SerialCommunication::setPortName(const QString& portName)
{
    return updateConfig("portName", portName);
}

bool SerialCommunication::setBaudRate(qint32 baudRate)
{
    return updateConfig("baudRate", baudRate);
}

bool SerialCommunication::setDataBits(QSerialPort::DataBits dataBits)
{
    return updateConfig("dataBits", static_cast<int>(dataBits));
}

bool SerialCommunication::setParity(QSerialPort::Parity parity)
{
    return updateConfig("parity", static_cast<int>(parity));
}

bool SerialCommunication::setStopBits(QSerialPort::StopBits stopBits)
{
    return updateConfig("stopBits", static_cast<int>(stopBits));
}

bool SerialCommunication::setFlowControl(QSerialPort::FlowControl flowControl)
{
    return updateConfig("flowControl", static_cast<int>(flowControl));
}

QString SerialCommunication::getPortName() const
{
    return m_config.portName;
}

qint32 SerialCommunication::getBaudRate() const
{
    return m_config.baudRate;
}

QSerialPort::DataBits SerialCommunication::getDataBits() const
{
    return m_config.dataBits;
}

QSerialPort::Parity SerialCommunication::getParity() const
{
    return m_config.parity;
}

QSerialPort::StopBits SerialCommunication::getStopBits() const
{
    return m_config.stopBits;
}

QSerialPort::FlowControl SerialCommunication::getFlowControl() const
{
    return m_config.flowControl;
}

// 公共槽函数
void SerialCommunication::reconnect()
{
    if (isConnected()) {
        disconnect();
    }
    
    // 延迟重连
    QTimer::singleShot(m_config.reconnectInterval, this, [this]() {
        connect(m_config);
    });
}

void SerialCommunication::startHeartbeat()
{
    if (!m_heartbeatTimer) {
        return;
    }
    
    if (m_config.enableHeartbeat && isConnected()) {
        m_heartbeatTimer->start(m_config.heartbeatInterval);
        logMessage("心跳检测已启动", "INFO");
    }
}

void SerialCommunication::stopHeartbeat()
{
    if (m_heartbeatTimer && m_heartbeatTimer->isActive()) {
        m_heartbeatTimer->stop();
        logMessage("心跳检测已停止", "INFO");
    }
}

void SerialCommunication::updateStatistics()
{
    updateConnectionStatistics();
    emit statisticsUpdated(m_statistics);
}

// 内部处理槽函数
void SerialCommunication::onHeartbeatTimer()
{
    if (!isConnected()) {
        stopHeartbeat();
        return;
    }
    
    // 检查心跳超时
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    if (m_lastHeartbeatTime > 0) {
        qint64 elapsed = currentTime - m_lastHeartbeatTime;
        if (elapsed > m_config.heartbeatInterval * 3) { // 3倍心跳间隔算超时
            handleError("心跳超时");
            emit heartbeatTimeout();
            return;
        }
    }
    
    // 发送心跳
    sendHeartbeat();
}

void SerialCommunication::onReconnectTimer()
{
    if (m_connectionState == ConnectionState::Reconnecting) {
        if (m_currentReconnectAttempts < m_config.maxReconnectAttempts) {
            m_currentReconnectAttempts++;
            emit reconnectAttempt(m_currentReconnectAttempts);
            
            logMessage(
                QString("尝试重连 (%1/%2)").arg(m_currentReconnectAttempts).arg(m_config.maxReconnectAttempts),
                "INFO"
            );
            
            // 尝试重连
            if (connect(m_config)) {
                stopReconnectTimer();
                resetReconnectAttempts();
            } else {
                // 继续重连
                startReconnectTimer();
            }
        } else {
            // 重连次数用完
            stopReconnectTimer();
            setState(ConnectionState::Disconnected);
            handleError("重连失败，已达到最大重连次数");
        }
    }
}

void SerialCommunication::onConnectionTimeout()
{
    if (m_connectionState == ConnectionState::Connecting) {
        handleError("连接超时");
    }
}

void SerialCommunication::onDataReceived()
{
    // 在串口特定槽函数中处理
}

void SerialCommunication::onErrorOccurred()
{
    // 在串口特定槽函数中处理
}

// 串口特定槽函数
void SerialCommunication::onSerialDataReceived()
{
    if (!m_serialPort || !m_serialPort->isOpen()) {
        return;
    }
    
    QByteArray data = m_serialPort->readAll();
    if (data.isEmpty()) {
        return;
    }
    
    // 更新接收缓冲区
    {
        QMutexLocker locker(&m_dataMutex);
        m_receiveBuffer.append(data);
    }
    
    // 更新统计信息
    m_statistics.bytesReceived += data.size();
    m_statistics.framesReceived++;
    updateLastActivity();
    
    // 处理接收到的数据
    processReceivedData(data);
    
    // 发送信号
    emit dataReceived(data);
}

void SerialCommunication::onSerialBytesWritten(qint64 bytes)
{
    emit bytesWritten(bytes);
    updateLastActivity();
}

void SerialCommunication::onSerialErrorOccurred(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) {
        return;
    }
    
    QString errorString = serialErrorToString(error);
    handleError(errorString);
}

// 受保护的方法
void SerialCommunication::setState(ConnectionState state)
{
    if (m_connectionState != state) {
        m_connectionState = state;
        emit connectionStateChanged(state);
        
        if (state == ConnectionState::Connected) {
            m_isConnecting = false;
            stopConnectionTimer();
            emit connected();
        } else if (state == ConnectionState::Disconnected) {
            m_isConnecting = false;
            emit disconnected();
        }
    }
}

void SerialCommunication::handleError(const QString& error)
{
    m_lastError = error;
    m_statistics.errorCount++;
    
    logMessage(error, "ERROR");
    emit connectionError(error);
    
    // 如果已连接，设置为错误状态
    if (isConnected()) {
        setState(ConnectionState::Error);
    }
    
    // 自动重连
    if (m_autoReconnectEnabled && m_connectionState != ConnectionState::Reconnecting) {
        setState(ConnectionState::Reconnecting);
        startReconnectTimer();
    }
}

void SerialCommunication::updateLastActivity()
{
    m_statistics.lastActivityTime = QDateTime::currentDateTime();
}

void SerialCommunication::logMessage(const QString& message, const QString& level)
{
    LogManager* logger = LogManager::getInstance();
    if (level == "DEBUG") {
        logger->debug(message, "SerialCommunication");
    } else if (level == "INFO") {
        logger->info(message, "SerialCommunication");
    } else if (level == "WARNING") {
        logger->warning(message, "SerialCommunication");
    } else if (level == "ERROR") {
        logger->error(message, "SerialCommunication");
    }
}

bool SerialCommunication::validateConfig(const CommunicationConfig& config) const
{
    const SerialConfig& serialConfig = static_cast<const SerialConfig&>(config);
    
    // 检查端口名
    if (serialConfig.portName.isEmpty()) {
        return false;
    }
    
    // 检查波特率
    if (serialConfig.baudRate <= 0) {
        return false;
    }
    
    // 检查超时时间
    if (serialConfig.timeout <= 0) {
        return false;
    }
    
    return true;
}

bool SerialCommunication::validateData(const QByteArray& data) const
{
    if (data.isEmpty()) {
        return false;
    }
    
    if (data.size() > Protocol::MAX_FRAME_SIZE) {
        return false;
    }
    
    return true;
}

bool SerialCommunication::isValidFrame(const ProtocolFrame& frame) const
{
    if (!m_protocolParser) {
        return false;
    }
    
    return m_protocolParser->validateFrameIntegrity(frame);
}

// 串口特定方法
bool SerialCommunication::openSerialPort()
{
    if (!m_serialPort) {
        handleError("串口对象未初始化");
        return false;
    }
    
    // 设置串口名称
    m_serialPort->setPortName(m_config.portName);
    
    // 配置串口参数
    configureSerialPort();
    
    // 打开串口
    if (!m_serialPort->open(QIODevice::ReadWrite)) {
        handleError(QString("无法打开串口: %1").arg(m_serialPort->errorString()));
        return false;
    }
    
    // 设置连接状态
    setState(ConnectionState::Connected);
    
    return true;
}

void SerialCommunication::closeSerialPort()
{
    if (m_serialPort && m_serialPort->isOpen()) {
        m_serialPort->close();
    }
}

void SerialCommunication::configureSerialPort()
{
    if (!m_serialPort) {
        return;
    }
    
    m_serialPort->setBaudRate(m_config.baudRate);
    m_serialPort->setDataBits(m_config.dataBits);
    m_serialPort->setParity(m_config.parity);
    m_serialPort->setStopBits(m_config.stopBits);
    m_serialPort->setFlowControl(m_config.flowControl);
}

void SerialCommunication::startConnectionTimer()
{
    if (m_connectionTimer) {
        m_connectionTimer->start(m_config.timeout);
    }
}

void SerialCommunication::stopConnectionTimer()
{
    if (m_connectionTimer && m_connectionTimer->isActive()) {
        m_connectionTimer->stop();
    }
}

void SerialCommunication::startReconnectTimer()
{
    if (m_reconnectTimer) {
        m_reconnectTimer->start(m_config.reconnectInterval);
    }
}

void SerialCommunication::stopReconnectTimer()
{
    if (m_reconnectTimer && m_reconnectTimer->isActive()) {
        m_reconnectTimer->stop();
    }
}

QString SerialCommunication::serialErrorToString(QSerialPort::SerialPortError error) const
{
    switch (error) {
        case QSerialPort::NoError:
            return "无错误";
        case QSerialPort::DeviceNotFoundError:
            return "设备未找到";
        case QSerialPort::PermissionError:
            return "权限错误";
        case QSerialPort::OpenError:
            return "打开错误";
        case QSerialPort::WriteError:
            return "写入错误";
        case QSerialPort::ReadError:
            return "读取错误";
        case QSerialPort::ResourceError:
            return "资源错误";
        case QSerialPort::UnsupportedOperationError:
            return "不支持的操作";
        case QSerialPort::TimeoutError:
            return "超时错误";
        case QSerialPort::NotOpenError:
            return "串口未打开";
        default:
            return "未知错误";
    }
}

// 私有方法
void SerialCommunication::initializeTimers()
{
    // 心跳定时器
    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setSingleShot(false);
    QObject::connect(m_heartbeatTimer, &QTimer::timeout, this, &SerialCommunication::onHeartbeatTimer);
    
    // 重连定时器
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    QObject::connect(m_reconnectTimer, &QTimer::timeout, this, &SerialCommunication::onReconnectTimer);
    
    // 连接超时定时器
    m_connectionTimer = new QTimer(this);
    m_connectionTimer->setSingleShot(true);
    QObject::connect(m_connectionTimer, &QTimer::timeout, this, &SerialCommunication::onConnectionTimeout);
    
    // 统计定时器
    m_statisticsTimer = new QTimer(this);
    m_statisticsTimer->setSingleShot(false);
    QObject::connect(m_statisticsTimer, &QTimer::timeout, this, &SerialCommunication::updateStatistics);
}

void SerialCommunication::connectSignals()
{
    // 串口信号连接
    QObject::connect(m_serialPort, &QSerialPort::readyRead, this, &SerialCommunication::onSerialDataReceived);
    QObject::connect(m_serialPort, &QSerialPort::bytesWritten, this, &SerialCommunication::onSerialBytesWritten);
    QObject::connect(m_serialPort, QOverload<QSerialPort::SerialPortError>::of(&QSerialPort::errorOccurred),
            this, &SerialCommunication::onSerialErrorOccurred);
    
    // 协议解析器信号连接
    QObject::connect(m_protocolParser, &ProtocolParser::frameReceived, this, &ICommunication::frameReceived);
    QObject::connect(m_protocolParser, &ProtocolParser::parseError, this, &ICommunication::protocolError);
    QObject::connect(m_protocolParser, &ProtocolParser::heartbeatReceived, this, &ICommunication::heartbeatReceived);
}

void SerialCommunication::processReceivedData(const QByteArray& data)
{
    if (m_protocolParser) {
        m_protocolParser->parseData(data);
    }
}

void SerialCommunication::updateConnectionStatistics()
{
    if (isConnected()) {
        m_statistics.lastActivityTime = QDateTime::currentDateTime();
        
        // 计算平均延迟
        calculateLatency();
    }
}

void SerialCommunication::calculateLatency()
{
    // 串口通讯延迟计算相对简单，主要基于波特率
    if (m_config.baudRate > 0) {
        // 估算延迟 = 数据位数 / 波特率 * 1000 (ms)
        double bitsPerByte = 8 + 1 + 1; // 数据位 + 起始位 + 停止位
        double latency = (bitsPerByte / m_config.baudRate) * 1000;
        m_statistics.averageLatency = latency;
    }
} 