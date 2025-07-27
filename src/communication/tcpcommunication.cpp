#include "tcpcommunication.h"
#include "logger/logmanager.h"
#include "constants.h"
#include <QDebug>
#include <QDateTime>
#include <QThread>
#include <QHostInfo>

TcpCommunication::TcpCommunication(QObject* parent)
    : ICommunication(parent)
    , m_tcpSocket(nullptr)
    , m_protocolParser(nullptr)
    , m_heartbeatTimer(nullptr)
    , m_reconnectTimer(nullptr)
    , m_connectionTimer(nullptr)
    , m_statisticsTimer(nullptr)
    , m_keepAliveTimer(nullptr)
    , m_isConnecting(false)
    , m_connectStartTime(0)
{
    // 创建TCP套接字
    m_tcpSocket = new QTcpSocket(this);
    
    // 创建协议解析器
    m_protocolParser = new ProtocolParser(this);
    
    // 初始化定时器
    initializeTimers();
    
    // 连接信号
    connectSignals();
    
    // 设置初始状态
    setState(ConnectionState::Disconnected);
    
    LogManager::getInstance()->info("TCP通讯对象已创建", "TcpCommunication");
}

TcpCommunication::~TcpCommunication()
{
    // 停止心跳
    stopHeartbeat();
    
    // 关闭连接
    if (isConnected()) {
        disconnect();
    }
    
    LogManager::getInstance()->info("TCP通讯对象已销毁", "TcpCommunication");
}

bool TcpCommunication::connect(const CommunicationConfig& config)
{
    if (isConnected()) {
        LogManager::getInstance()->warning("TCP已连接", "TcpCommunication");
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
    
    // 连接到主机
    if (connectToHost()) {
        // 重置重连计数
        resetReconnectAttempts();
        
        // 启动心跳
        if (m_config.enableHeartbeat) {
            startHeartbeat();
        }
        
        // 启动Keep-Alive
        if (m_config.keepAlive) {
            m_keepAliveTimer->start(30000); // 30秒发送一次Keep-Alive
        }
        
        // 启动统计定时器
        m_statisticsTimer->start(System::STATISTICS_UPDATE_INTERVAL);
        
        LogManager::getInstance()->info(
            QString("TCP连接成功: %1:%2").arg(m_config.hostAddress).arg(m_config.port),
            "TcpCommunication"
        );
        
        return true;
    } else {
        handleError("无法连接到主机");
        return false;
    }
}

void TcpCommunication::disconnect()
{
    if (!isConnected()) {
        return;
    }
    
    // 停止所有定时器
    stopHeartbeat();
    stopConnectionTimer();
    stopReconnectTimer();
    m_statisticsTimer->stop();
    if (m_keepAliveTimer) {
        m_keepAliveTimer->stop();
    }
    
    // 关闭TCP连接
    disconnectFromHost();
    
    // 设置状态
    setState(ConnectionState::Disconnected);
    m_isConnecting = false;
    
    LogManager::getInstance()->info("TCP连接已断开", "TcpCommunication");
}

bool TcpCommunication::isConnected() const
{
    return m_tcpSocket && (m_tcpSocket->state() == QAbstractSocket::ConnectedState) 
           && (m_connectionState == ConnectionState::Connected);
}

ConnectionState TcpCommunication::getConnectionState() const
{
    return m_connectionState;
}

CommunicationType TcpCommunication::getType() const
{
    return CommunicationType::TCP;
}

QString TcpCommunication::getName() const
{
    return m_config.name;
}

bool TcpCommunication::sendData(const QByteArray& data)
{
    if (!validateData(data)) {
        return false;
    }
    
    if (!isConnected()) {
        handleError("TCP未连接");
        return false;
    }
    
    QMutexLocker locker(&m_dataMutex);
    
    qint64 bytesWritten = m_tcpSocket->write(data);
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

bool TcpCommunication::sendFrame(ProtocolCommand command, const QByteArray& data)
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

QByteArray TcpCommunication::receiveData()
{
    if (!isConnected()) {
        return QByteArray();
    }
    
    QMutexLocker locker(&m_dataMutex);
    QByteArray data = m_receiveBuffer;
    m_receiveBuffer.clear();
    
    return data;
}

void TcpCommunication::setConfig(const CommunicationConfig& config)
{
    // 转换为TCP配置
    if (config.type == CommunicationType::TCP) {
        m_config = TcpConfig::fromBase(config);
    } else {
        m_config = TcpConfig::fromBase(config);
        m_config.type = CommunicationType::TCP;
    }
    
    // 如果已连接，需要重新配置
    if (isConnected()) {
        configureTcpSocket();
    }
    
    emit configurationChanged();
}

CommunicationConfig TcpCommunication::getConfig() const
{
    return m_config;
}

bool TcpCommunication::updateConfig(const QString& key, const QVariant& value)
{
    bool updated = false;
    
    if (key == "hostAddress") {
        m_config.hostAddress = value.toString();
        updated = true;
    } else if (key == "port") {
        m_config.port = value.toUInt();
        updated = true;
    } else if (key == "connectTimeout") {
        m_config.connectTimeout = value.toInt();
        updated = true;
    } else if (key == "readTimeout") {
        m_config.readTimeout = value.toInt();
        updated = true;
    } else if (key == "writeTimeout") {
        m_config.writeTimeout = value.toInt();
        updated = true;
    } else if (key == "keepAlive") {
        m_config.keepAlive = value.toBool();
        if (m_config.keepAlive) {
            if (m_keepAliveTimer) {
                m_keepAliveTimer->start(30000);
            }
        } else {
            if (m_keepAliveTimer) {
                m_keepAliveTimer->stop();
            }
        }
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

QString TcpCommunication::getLastError() const
{
    return m_lastError;
}

CommunicationStats TcpCommunication::getStatistics() const
{
    return m_statistics;
}

void TcpCommunication::resetStatistics()
{
    m_statistics.reset();
    emit statisticsUpdated(m_statistics);
}

void TcpCommunication::enableHeartbeat(bool enabled)
{
    m_heartbeatEnabled = enabled;
    updateConfig("enableHeartbeat", enabled);
}

bool TcpCommunication::isHeartbeatEnabled() const
{
    return m_heartbeatEnabled;
}

void TcpCommunication::sendHeartbeat()
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

qint64 TcpCommunication::getLastHeartbeatTime() const
{
    return m_lastHeartbeatTime;
}

void TcpCommunication::enableAutoReconnect(bool enabled)
{
    m_autoReconnectEnabled = enabled;
    updateConfig("autoReconnect", enabled);
}

bool TcpCommunication::isAutoReconnectEnabled() const
{
    return m_autoReconnectEnabled;
}

void TcpCommunication::setMaxReconnectAttempts(int maxAttempts)
{
    m_config.maxReconnectAttempts = maxAttempts;
}

int TcpCommunication::getCurrentReconnectAttempts() const
{
    return m_currentReconnectAttempts;
}

void TcpCommunication::resetReconnectAttempts()
{
    m_currentReconnectAttempts = 0;
}

void TcpCommunication::flush()
{
    if (m_tcpSocket && m_tcpSocket->state() == QAbstractSocket::ConnectedState) {
        m_tcpSocket->flush();
    }
}

void TcpCommunication::clearBuffers()
{
    QMutexLocker locker(&m_dataMutex);
    m_receiveBuffer.clear();
    m_sendQueue.clear();
}

bool TcpCommunication::testConnection()
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

QStringList TcpCommunication::getAvailableConnections() const
{
    // TCP连接没有固定的可用连接列表，返回空列表
    return QStringList();
}

void TcpCommunication::setProperty(const QString& name, const QVariant& value)
{
    m_properties[name] = value;
    emit propertyChanged(name, value);
}

QVariant TcpCommunication::getProperty(const QString& name) const
{
    return m_properties.value(name);
}

// TCP特定功能实现
bool TcpCommunication::setHostAddress(const QString& address)
{
    return updateConfig("hostAddress", address);
}

bool TcpCommunication::setPort(quint16 port)
{
    return updateConfig("port", port);
}

bool TcpCommunication::setConnectTimeout(int timeout)
{
    return updateConfig("connectTimeout", timeout);
}

bool TcpCommunication::setKeepAlive(bool enabled)
{
    return updateConfig("keepAlive", enabled);
}

QString TcpCommunication::getHostAddress() const
{
    return m_config.hostAddress;
}

quint16 TcpCommunication::getPort() const
{
    return m_config.port;
}

int TcpCommunication::getConnectTimeout() const
{
    return m_config.connectTimeout;
}

bool TcpCommunication::isKeepAliveEnabled() const
{
    return m_config.keepAlive;
}

QString TcpCommunication::getPeerAddress() const
{
    if (m_tcpSocket) {
        return m_tcpSocket->peerAddress().toString();
    }
    return QString();
}

quint16 TcpCommunication::getPeerPort() const
{
    if (m_tcpSocket) {
        return m_tcpSocket->peerPort();
    }
    return 0;
}

QString TcpCommunication::getLocalAddress() const
{
    if (m_tcpSocket) {
        return m_tcpSocket->localAddress().toString();
    }
    return QString();
}

quint16 TcpCommunication::getLocalPort() const
{
    if (m_tcpSocket) {
        return m_tcpSocket->localPort();
    }
    return 0;
}

// 公共槽函数
void TcpCommunication::reconnect()
{
    if (isConnected()) {
        disconnect();
    }
    
    // 延迟重连
    QTimer::singleShot(m_config.reconnectInterval, this, [this]() {
        connect(m_config);
    });
}

void TcpCommunication::startHeartbeat()
{
    if (!m_heartbeatTimer) {
        return;
    }
    
    if (m_config.enableHeartbeat && isConnected()) {
        m_heartbeatTimer->start(m_config.heartbeatInterval);
        logMessage("心跳检测已启动", "INFO");
    }
}

void TcpCommunication::stopHeartbeat()
{
    if (m_heartbeatTimer && m_heartbeatTimer->isActive()) {
        m_heartbeatTimer->stop();
        logMessage("心跳检测已停止", "INFO");
    }
}

void TcpCommunication::updateStatistics()
{
    updateConnectionStatistics();
    emit statisticsUpdated(m_statistics);
}

// 内部处理槽函数
void TcpCommunication::onHeartbeatTimer()
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

void TcpCommunication::onReconnectTimer()
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

void TcpCommunication::onConnectionTimeout()
{
    if (m_connectionState == ConnectionState::Connecting) {
        handleError("连接超时");
    }
}

void TcpCommunication::onDataReceived()
{
    // 在TCP特定槽函数中处理
}

void TcpCommunication::onErrorOccurred()
{
    // 在TCP特定槽函数中处理
}

// TCP特定槽函数
void TcpCommunication::onTcpConnected()
{
    setState(ConnectionState::Connected);
    stopConnectionTimer();
    
    logMessage("TCP连接已建立", "INFO");
    
    // 配置TCP套接字
    configureTcpSocket();
}

void TcpCommunication::onTcpDisconnected()
{
    setState(ConnectionState::Disconnected);
    
    logMessage("TCP连接已断开", "INFO");
    
    // 如果启用了自动重连且不是主动断开
    if (m_autoReconnectEnabled && m_connectionState != ConnectionState::Disconnected) {
        setState(ConnectionState::Reconnecting);
        startReconnectTimer();
    }
}

void TcpCommunication::onTcpDataReceived()
{
    if (!m_tcpSocket) {
        return;
    }
    
    QByteArray data = m_tcpSocket->readAll();
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

void TcpCommunication::onTcpBytesWritten(qint64 bytes)
{
    emit bytesWritten(bytes);
    updateLastActivity();
}

void TcpCommunication::onTcpErrorOccurred(QAbstractSocket::SocketError error)
{
    if (error == QAbstractSocket::RemoteHostClosedError) {
        // 远程主机关闭连接，这是正常情况
        logMessage("远程主机关闭连接", "INFO");
        return;
    }
    
    QString errorString = tcpErrorToString(error);
    handleError(errorString);
}

void TcpCommunication::onTcpStateChanged(QAbstractSocket::SocketState state)
{
    QString stateString = tcpStateToString(state);
    logMessage(QString("TCP状态变更: %1").arg(stateString), "DEBUG");
    
    switch (state) {
        case QAbstractSocket::ConnectingState:
            setState(ConnectionState::Connecting);
            break;
        case QAbstractSocket::ConnectedState:
            setState(ConnectionState::Connected);
            break;
        case QAbstractSocket::UnconnectedState:
            setState(ConnectionState::Disconnected);
            break;
        default:
            break;
    }
}

// 受保护的方法
void TcpCommunication::setState(ConnectionState state)
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

void TcpCommunication::handleError(const QString& error)
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

void TcpCommunication::updateLastActivity()
{
    m_statistics.lastActivityTime = QDateTime::currentDateTime();
}

void TcpCommunication::logMessage(const QString& message, const QString& level)
{
    LogManager* logger = LogManager::getInstance();
    if (level == "DEBUG") {
        logger->debug(message, "TcpCommunication");
    } else if (level == "INFO") {
        logger->info(message, "TcpCommunication");
    } else if (level == "WARNING") {
        logger->warning(message, "TcpCommunication");
    } else if (level == "ERROR") {
        logger->error(message, "TcpCommunication");
    }
}

bool TcpCommunication::validateConfig(const CommunicationConfig& config) const
{
    const TcpConfig& tcpConfig = static_cast<const TcpConfig&>(config);
    
    // 检查主机地址
    if (tcpConfig.hostAddress.isEmpty()) {
        return false;
    }
    
    // 检查端口
    if (tcpConfig.port == 0) {
        return false;
    }
    
    // 检查超时时间
    if (tcpConfig.timeout <= 0) {
        return false;
    }
    
    return true;
}

bool TcpCommunication::validateData(const QByteArray& data) const
{
    if (data.isEmpty()) {
        return false;
    }
    
    if (data.size() > Protocol::MAX_FRAME_SIZE) {
        return false;
    }
    
    return true;
}

bool TcpCommunication::isValidFrame(const ProtocolFrame& frame) const
{
    if (!m_protocolParser) {
        return false;
    }
    
    return m_protocolParser->validateFrameIntegrity(frame);
}

// TCP特定方法
bool TcpCommunication::connectToHost()
{
    if (!m_tcpSocket) {
        handleError("TCP套接字未初始化");
        return false;
    }
    
    // 连接到主机
    m_tcpSocket->connectToHost(m_config.hostAddress, m_config.port);
    
    // 等待连接完成
    if (!m_tcpSocket->waitForConnected(m_config.connectTimeout)) {
        handleError(QString("连接超时: %1").arg(m_tcpSocket->errorString()));
        return false;
    }
    
    return true;
}

void TcpCommunication::disconnectFromHost()
{
    if (m_tcpSocket && m_tcpSocket->state() == QAbstractSocket::ConnectedState) {
        m_tcpSocket->disconnectFromHost();
        
        // 等待断开完成
        if (m_tcpSocket->state() != QAbstractSocket::UnconnectedState) {
            m_tcpSocket->waitForDisconnected(3000);
        }
    }
}

void TcpCommunication::configureTcpSocket()
{
    if (!m_tcpSocket) {
        return;
    }
    
    // 设置套接字选项
    m_tcpSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    m_tcpSocket->setSocketOption(QAbstractSocket::KeepAliveOption, m_config.keepAlive ? 1 : 0);
    
    // 设置缓冲区大小
    m_tcpSocket->setReadBufferSize(Protocol::MAX_BUFFER_SIZE);
}

void TcpCommunication::startConnectionTimer()
{
    if (m_connectionTimer) {
        m_connectionTimer->start(m_config.connectTimeout);
    }
}

void TcpCommunication::stopConnectionTimer()
{
    if (m_connectionTimer && m_connectionTimer->isActive()) {
        m_connectionTimer->stop();
    }
}

void TcpCommunication::startReconnectTimer()
{
    if (m_reconnectTimer) {
        m_reconnectTimer->start(m_config.reconnectInterval);
    }
}

void TcpCommunication::stopReconnectTimer()
{
    if (m_reconnectTimer && m_reconnectTimer->isActive()) {
        m_reconnectTimer->stop();
    }
}

QString TcpCommunication::tcpErrorToString(QAbstractSocket::SocketError error) const
{
    switch (error) {
        case QAbstractSocket::RemoteHostClosedError:
            return "远程主机关闭连接";
        case QAbstractSocket::HostNotFoundError:
            return "主机未找到";
        case QAbstractSocket::ConnectionRefusedError:
            return "连接被拒绝";
        case QAbstractSocket::NetworkError:
            return "网络错误";
        case QAbstractSocket::SocketTimeoutError:
            return "套接字超时";
        case QAbstractSocket::SocketResourceError:
            return "套接字资源错误";
        case QAbstractSocket::DatagramTooLargeError:
            return "数据报过大";
        case QAbstractSocket::AddressInUseError:
            return "地址已被使用";
        case QAbstractSocket::SocketAddressNotAvailableError:
            return "套接字地址不可用";
        case QAbstractSocket::UnsupportedSocketOperationError:
            return "不支持的套接字操作";
        case QAbstractSocket::UnfinishedSocketOperationError:
            return "未完成的套接字操作";
        case QAbstractSocket::ProxyAuthenticationRequiredError:
            return "代理需要认证";
        case QAbstractSocket::SslHandshakeFailedError:
            return "SSL握手失败";
        case QAbstractSocket::ProxyConnectionRefusedError:
            return "代理连接被拒绝";
        case QAbstractSocket::ProxyConnectionClosedError:
            return "代理连接关闭";
        case QAbstractSocket::ProxyConnectionTimeoutError:
            return "代理连接超时";
        case QAbstractSocket::ProxyNotFoundError:
            return "代理未找到";
        case QAbstractSocket::ProxyProtocolError:
            return "代理协议错误";
        case QAbstractSocket::OperationError:
            return "操作错误";
        case QAbstractSocket::SslInternalError:
            return "SSL内部错误";
        case QAbstractSocket::SslInvalidUserDataError:
            return "SSL无效用户数据";
        case QAbstractSocket::TemporaryError:
            return "临时错误";
        default:
            return "未知错误";
    }
}

QString TcpCommunication::tcpStateToString(QAbstractSocket::SocketState state) const
{
    switch (state) {
        case QAbstractSocket::UnconnectedState:
            return "未连接";
        case QAbstractSocket::HostLookupState:
            return "主机查找中";
        case QAbstractSocket::ConnectingState:
            return "连接中";
        case QAbstractSocket::ConnectedState:
            return "已连接";
        case QAbstractSocket::BoundState:
            return "已绑定";
        case QAbstractSocket::ListeningState:
            return "监听中";
        case QAbstractSocket::ClosingState:
            return "关闭中";
        default:
            return "未知状态";
    }
}

// 私有方法
void TcpCommunication::initializeTimers()
{
    // 心跳定时器
    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setSingleShot(false);
    QObject::connect(m_heartbeatTimer, &QTimer::timeout, this, &TcpCommunication::onHeartbeatTimer);
    
    // 重连定时器
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    QObject::connect(m_reconnectTimer, &QTimer::timeout, this, &TcpCommunication::onReconnectTimer);
    
    // 连接超时定时器
    m_connectionTimer = new QTimer(this);
    m_connectionTimer->setSingleShot(true);
    QObject::connect(m_connectionTimer, &QTimer::timeout, this, &TcpCommunication::onConnectionTimeout);
    
    // 统计定时器
    m_statisticsTimer = new QTimer(this);
    m_statisticsTimer->setSingleShot(false);
    QObject::connect(m_statisticsTimer, &QTimer::timeout, this, &TcpCommunication::updateStatistics);
    
    // Keep-Alive定时器
    m_keepAliveTimer = new QTimer(this);
    m_keepAliveTimer->setSingleShot(false);
    QObject::connect(m_keepAliveTimer, &QTimer::timeout, this, &TcpCommunication::sendKeepAlive);
}

void TcpCommunication::connectSignals()
{
    // TCP套接字信号连接
    QObject::connect(m_tcpSocket, &QTcpSocket::connected, this, &TcpCommunication::onTcpConnected);
    QObject::connect(m_tcpSocket, &QTcpSocket::disconnected, this, &TcpCommunication::onTcpDisconnected);
    QObject::connect(m_tcpSocket, &QTcpSocket::readyRead, this, &TcpCommunication::onTcpDataReceived);
    QObject::connect(m_tcpSocket, &QTcpSocket::bytesWritten, this, &TcpCommunication::onTcpBytesWritten);
    QObject::connect(m_tcpSocket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &TcpCommunication::onTcpErrorOccurred);
    QObject::connect(m_tcpSocket, &QTcpSocket::stateChanged, this, &TcpCommunication::onTcpStateChanged);
    
    // 协议解析器信号连接
    QObject::connect(m_protocolParser, &ProtocolParser::frameReceived, this, &ICommunication::frameReceived);
    QObject::connect(m_protocolParser, &ProtocolParser::parseError, this, &ICommunication::protocolError);
    QObject::connect(m_protocolParser, &ProtocolParser::heartbeatReceived, this, &ICommunication::heartbeatReceived);
}

void TcpCommunication::processReceivedData(const QByteArray& data)
{
    if (m_protocolParser) {
        m_protocolParser->parseData(data);
    }
}

void TcpCommunication::updateConnectionStatistics()
{
    if (isConnected()) {
        m_statistics.lastActivityTime = QDateTime::currentDateTime();
        
        // 计算平均延迟
        calculateLatency();
    }
}

void TcpCommunication::calculateLatency()
{
    // TCP网络延迟计算可以通过ping或者心跳响应时间来估算
    // 这里使用简化的方法，基于本地和远程地址计算大概的延迟
    if (m_lastHeartbeatTime > 0) {
        qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        qint64 elapsed = currentTime - m_lastHeartbeatTime;
        
        // 更新平均延迟（简单移动平均）
        if (m_statistics.averageLatency == 0.0) {
            m_statistics.averageLatency = elapsed;
        } else {
            m_statistics.averageLatency = (m_statistics.averageLatency * 0.8) + (elapsed * 0.2);
        }
    }
}

void TcpCommunication::sendKeepAlive()
{
    if (!isConnected()) {
        return;
    }
    
    // 发送简单的Keep-Alive数据包
    QByteArray keepAliveData;
    keepAliveData.append(static_cast<char>(0x00)); // Keep-Alive标识
    
    if (sendData(keepAliveData)) {
        logMessage("Keep-Alive包已发送", "DEBUG");
    } else {
        logMessage("Keep-Alive包发送失败", "WARNING");
    }
} 