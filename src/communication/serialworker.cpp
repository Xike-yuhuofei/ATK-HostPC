#include "serialworker.h"
#include "logger/logmanager.h"
#include "constants.h"
#include <QDebug>

SerialWorker::SerialWorker(QObject* parent)
    : QObject(parent)
    , serialPort(nullptr)
    , protocolParser(nullptr)
    , connectionState(SerialConnectionState::Disconnected)
    , maxReconnectAttempts(Protocol::MAX_RECONNECT_ATTEMPTS)  // 默认最多重连3次
    , currentReconnectAttempts(0)
    , silentMode(false)  // 默认非静默模式
    , bytesReceived(0)
    , bytesSent(0)
{
    // 创建串口对象
    serialPort = new QSerialPort(this);
    
    // 创建协议解析器
    protocolParser = new ProtocolParser(this);
    
    // 连接信号
    connect(serialPort, &QSerialPort::readyRead, this, &SerialWorker::onReadyRead);
    connect(serialPort, &QSerialPort::bytesWritten, this, &SerialWorker::onBytesWritten);
    connect(serialPort, QOverload<QSerialPort::SerialPortError>::of(&QSerialPort::errorOccurred),
            this, &SerialWorker::onErrorOccurred);
    
    connect(protocolParser, &ProtocolParser::frameReceived, 
            this, &SerialWorker::onProtocolFrameReceived);
    connect(protocolParser, &ProtocolParser::parseError, 
            this, &SerialWorker::onProtocolParseError);
    
    // 创建重连定时器
    reconnectTimer = new QTimer(this);
    reconnectTimer->setSingleShot(true);
    connect(reconnectTimer, &QTimer::timeout, this, &SerialWorker::onReconnectTimer);
    
    // 创建连接超时定时器
    connectionTimer = new QTimer(this);
    connectionTimer->setSingleShot(true);
    connect(connectionTimer, &QTimer::timeout, this, &SerialWorker::onConnectionTimeout);
    
    // 创建统计定时器
    statisticsTimer = new QTimer(this);
    connect(statisticsTimer, &QTimer::timeout, this, &SerialWorker::updateStatistics);
    statisticsTimer->start(System::STATISTICS_UPDATE_INTERVAL);
    
    LogManager::getInstance()->info("串口通讯模块已初始化", "Serial");
}

SerialWorker::~SerialWorker()
{
    closePort();
    LogManager::getInstance()->info("串口通讯模块已关闭", "Serial");
}

bool SerialWorker::openPort(const SerialConfig& config)
{
    if (connectionState == SerialConnectionState::Connected) {
        LogManager::getInstance()->warning("串口已经连接", "Serial");
        return true;
    }
    
    this->config = config;
    setState(SerialConnectionState::Connecting);
    
    // 配置串口
    serialPort->setPortName(config.portName);
    serialPort->setBaudRate(config.baudRate);
    serialPort->setDataBits(config.dataBits);
    serialPort->setParity(config.parity);
    serialPort->setStopBits(config.stopBits);
    serialPort->setFlowControl(config.flowControl);
    
    // 尝试打开串口
    if (!serialPort->open(QIODevice::ReadWrite)) {
        QString error = QString("无法打开串口 %1: %2")
                        .arg(config.portName, serialPort->errorString());
        handleError(error);
        return false;
    }
    
    // 启动连接超时定时器
    connectionTimer->start(Protocol::CONNECTION_TIMEOUT);
    
    // 连接成功
    setState(SerialConnectionState::Connected);
    connectionTimer->stop();
    
    LogManager::getInstance()->info(
        QString("串口连接成功: %1 @ %2")
        .arg(config.portName)
        .arg(config.baudRate),
        "Serial"
    );
    
    emit connected();
    return true;
}

void SerialWorker::closePort()
{
    if (serialPort && serialPort->isOpen()) {
        serialPort->close();
        setState(SerialConnectionState::Disconnected);
        stopReconnectTimer();
        connectionTimer->stop();
        
        LogManager::getInstance()->info(
            QString("串口已关闭: %1").arg(config.portName),
            "Serial"
        );
        
        emit disconnected();
    }
}

bool SerialWorker::isConnected() const
{
    return connectionState == SerialConnectionState::Connected;
}

bool SerialWorker::sendData(const QByteArray& data)
{
    if (!isConnected()) {
        LogManager::getInstance()->warning("串口未连接，无法发送数据", "Serial");
        return false;
    }
    
    if (data.isEmpty()) {
        LogManager::getInstance()->warning("发送数据为空", "Serial");
        return false;
    }
    
    QMutexLocker locker(&dataMutex);
    
    qint64 bytesWritten = serialPort->write(data);
    if (bytesWritten == -1) {
        QString error = QString("发送数据失败: %1").arg(serialPort->errorString());
        handleError(error);
        return false;
    }
    
    if (bytesWritten != data.size()) {
        LogManager::getInstance()->warning(
            QString("发送数据不完整: 期望=%1, 实际=%2")
            .arg(data.size())
            .arg(bytesWritten),
            "Serial"
        );
    }
    
    // 记录发送的数据
    LOG_COMM_TX(data, config.portName);
    
    return true;
}

bool SerialWorker::sendFrame(ProtocolCommand command, const QByteArray& data)
{
    QByteArray frame = protocolParser->buildFrame(command, data);
    if (frame.isEmpty()) {
        LogManager::getInstance()->error("构建协议帧失败", "Serial");
        return false;
    }
    
    return sendData(frame);
}

void SerialWorker::setConfig(const SerialConfig& config)
{
    this->config = config;
}

SerialConfig SerialWorker::getConfig() const
{
    return config;
}

SerialConnectionState SerialWorker::getConnectionState() const
{
    return connectionState;
}

QString SerialWorker::getLastError() const
{
    return lastError;
}

QStringList SerialWorker::getAvailablePorts()
{
    QStringList ports;
    const QList<QSerialPortInfo> portInfos = QSerialPortInfo::availablePorts();
    
    for (const QSerialPortInfo& info : portInfos) {
        ports.append(info.portName());
    }
    
    return ports;
}

void SerialWorker::setAutoReconnect(bool enabled)
{
    config.autoReconnect = enabled;
    if (!enabled) {
        stopReconnectTimer();
    }
}

bool SerialWorker::getAutoReconnect() const
{
    return config.autoReconnect;
}

// 重连次数控制
void SerialWorker::setMaxReconnectAttempts(int maxAttempts)
{
    maxReconnectAttempts = maxAttempts;
    LogManager::getInstance()->info(
        QString("设置最大重连次数: %1").arg(maxAttempts),
        "Serial"
    );
}

int SerialWorker::getMaxReconnectAttempts() const
{
    return maxReconnectAttempts;
}

int SerialWorker::getCurrentReconnectAttempts() const
{
    return currentReconnectAttempts;
}

void SerialWorker::resetReconnectAttempts()
{
    currentReconnectAttempts = 0;
    LogManager::getInstance()->debug("重连次数已重置", "Serial");
}

// 静默模式控制
void SerialWorker::setSilentMode(bool silent)
{
    silentMode = silent;
    LogManager::getInstance()->info(
        QString("静默模式: %1").arg(silent ? "开启" : "关闭"),
        "Serial"
    );
}

bool SerialWorker::getSilentMode() const
{
    return silentMode;
}

qint64 SerialWorker::getBytesReceived() const
{
    return bytesReceived;
}

qint64 SerialWorker::getBytesSent() const
{
    return bytesSent;
}

void SerialWorker::resetStatistics()
{
    bytesReceived = 0;
    bytesSent = 0;
    LogManager::getInstance()->info("统计信息已重置", "Serial");
}

void SerialWorker::onReadyRead()
{
    if (!serialPort) return;
    
    QByteArray data = serialPort->readAll();
    if (data.isEmpty()) return;
    
    // 更新统计信息
    bytesReceived += data.size();
    
    // 记录接收的数据
    LOG_COMM_RX(data, config.portName);
    
    // 发送原始数据信号
    emit dataReceived(data);
    
    // 协议解析
    protocolParser->parseData(data);
}

void SerialWorker::onBytesWritten(qint64 bytes)
{
    bytesSent += bytes;
    emit bytesWritten(bytes);
}

void SerialWorker::onErrorOccurred(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) {
        return;
    }
    
    QString errorString;
    switch (error) {
        case QSerialPort::DeviceNotFoundError:
            errorString = "设备未找到";
            break;
        case QSerialPort::PermissionError:
            errorString = "权限错误";
            break;
        case QSerialPort::OpenError:
            errorString = "打开错误";
            break;
        case QSerialPort::WriteError:
            errorString = "写入错误";
            break;
        case QSerialPort::ReadError:
            errorString = "读取错误";
            break;
        case QSerialPort::ResourceError:
            errorString = "资源错误";
            break;
        case QSerialPort::UnsupportedOperationError:
            errorString = "不支持的操作";
            break;
        case QSerialPort::TimeoutError:
            errorString = "超时错误";
            break;
        case QSerialPort::NotOpenError:
            errorString = "串口未打开";
            break;
        default:
            errorString = "未知错误";
            break;
    }
    
    QString fullError = QString("串口错误: %1 (%2)").arg(errorString).arg(serialPort->errorString());
    handleError(fullError);
}

void SerialWorker::onReconnectTimer()
{
    if (connectionState == SerialConnectionState::Reconnecting) {
        LogManager::getInstance()->info("尝试重新连接串口", "Serial");
        
        // 先关闭当前连接
        if (serialPort->isOpen()) {
            serialPort->close();
        }
        
        // 尝试重新连接
        if (openPort(config)) {
            // 重连成功，重置计数器
            currentReconnectAttempts = 0;
            LogManager::getInstance()->info("串口重连成功", "Serial");
        } else {
            // 重连失败，继续尝试（在handleError中会检查次数限制）
            LogManager::getInstance()->warning("串口重连失败", "Serial");
        }
    }
}

void SerialWorker::onProtocolFrameReceived(const ProtocolFrame& frame)
{
    emit frameReceived(frame);
}

void SerialWorker::onProtocolParseError(const QString& error)
{
    LogManager::getInstance()->warning(
        QString("协议解析错误: %1").arg(error),
        "Serial"
    );
}

void SerialWorker::onConnectionTimeout()
{
    if (connectionState == SerialConnectionState::Connecting) {
        handleError("连接超时");
    }
}

void SerialWorker::setState(SerialConnectionState state)
{
    if (connectionState != state) {
        connectionState = state;
        emit connectionStateChanged(state);
        
        QString stateString;
        switch (state) {
            case SerialConnectionState::Disconnected:
                stateString = "断开连接";
                break;
            case SerialConnectionState::Connecting:
                stateString = "正在连接";
                break;
            case SerialConnectionState::Connected:
                stateString = "已连接";
                break;
            case SerialConnectionState::Reconnecting:
                stateString = "重新连接中";
                break;
            case SerialConnectionState::Error:
                stateString = "错误状态";
                break;
        }
        
        LogManager::getInstance()->debug(
            QString("串口状态变化: %1").arg(stateString),
            "Serial"
        );
    }
}

void SerialWorker::handleError(const QString& error)
{
    lastError = error;
    LogManager::getInstance()->error(error, "Serial");
    
    setState(SerialConnectionState::Error);
    
    // 只在非静默模式下才发送错误信号（触发弹窗）
    if (!silentMode) {
        emit errorOccurred(error);
    }
    
    // 如果启用了自动重连，检查重连次数
    if (config.autoReconnect && currentReconnectAttempts < maxReconnectAttempts) {
        setState(SerialConnectionState::Reconnecting);
        currentReconnectAttempts++;
        
        LogManager::getInstance()->info(
            QString("开始第 %1/%2 次重连尝试").arg(currentReconnectAttempts).arg(maxReconnectAttempts),
            "Serial"
        );
        
        startReconnectTimer();
    } else if (currentReconnectAttempts >= maxReconnectAttempts) {
        LogManager::getInstance()->warning(
            QString("已达到最大重连次数 %1，停止重连").arg(maxReconnectAttempts),
            "Serial"
        );
        
        // 最后一次弹窗通知用户
        if (silentMode) {
            emit errorOccurred(QString("重连失败: %1 次尝试后仍无法连接").arg(maxReconnectAttempts));
        }
        
        setState(SerialConnectionState::Disconnected);
        stopReconnectTimer();
    }
}

void SerialWorker::startReconnectTimer()
{
    if (!reconnectTimer->isActive()) {
        reconnectTimer->start(config.reconnectInterval);
        LogManager::getInstance()->info(
            QString("将在 %1 秒后尝试重新连接").arg(config.reconnectInterval / 1000),
            "Serial"
        );
    }
}

void SerialWorker::stopReconnectTimer()
{
    if (reconnectTimer->isActive()) {
        reconnectTimer->stop();
        LogManager::getInstance()->info("重连定时器已停止", "Serial");
    }
}

void SerialWorker::updateStatistics()
{
    emit statisticsUpdated(bytesReceived, bytesSent);
} 