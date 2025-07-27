#include "protocolparser.h"
#include "logger/logmanager.h"
#include "constants.h"
#include <QDebug>
#include <QElapsedTimer>
#include <cstring>

ProtocolParser::ProtocolParser(QObject* parent)
    : QObject(parent)
    , timeoutMs(5000)
    , m_checksumType(ChecksumType::CRC16_MODBUS)
    , m_enhancedChecksumEnabled(true)
    , m_lastHeaderPos(-1)
    , m_bufferSearchStart(0)
    , m_bufferOptimized(false)
{
    timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);
    connect(timeoutTimer, &QTimer::timeout, this, &ProtocolParser::onTimeout);
    
    // 初始化性能统计
    m_perfStats.totalBytesProcessed = 0;
    m_perfStats.totalFramesProcessed = 0;
    m_perfStats.totalParseTime = 0;
    m_perfStats.averageParseTime = 0;
    m_perfStats.lastStatsUpdate = QDateTime::currentDateTime();
    
    // 预分配内存
    preallocateMemory();
}

void ProtocolParser::parseData(const QByteArray& data)
{
    if (data.isEmpty()) return;
    
    // 性能统计开始
    QElapsedTimer timer;
    timer.start();
    
    // 添加数据到接收缓冲区
    receiveBuffer.append(data);
    m_perfStats.totalBytesProcessed += data.size();
    
    // 如果缓冲区太大，清空防止内存溢出
    if (receiveBuffer.size() > Protocol::MAX_BUFFER_SIZE) {
        LogManager::getInstance()->warning("接收缓冲区溢出，清空缓冲区", "Protocol");
        receiveBuffer.clear();
        m_lastHeaderPos = -1;
        m_bufferSearchStart = 0;
        emit parseError("接收缓冲区溢出");
        return;
    }
    
    // 优化缓冲区
    if (!m_bufferOptimized && receiveBuffer.size() > 1024) {
        optimizeBuffer();
    }
    
    // 尝试解析帧 - 使用优化的查找算法
    QByteArray frameData;
    while (findFrameOptimized(frameData)) {
        ProtocolFrame frame;
        if (validateFrame(frameData, frame)) {
            processCompleteFrame(frame);
            m_perfStats.totalFramesProcessed++;
        }
    }
    
    // 性能统计结束
    qint64 parseTime = timer.elapsed();
    m_perfStats.totalParseTime += parseTime;
    m_perfStats.averageParseTime = m_perfStats.totalFramesProcessed > 0 ? 
                                   m_perfStats.totalParseTime / m_perfStats.totalFramesProcessed : 0;
}

QByteArray ProtocolParser::buildFrame(ProtocolCommand command, const QByteArray& data)
{
    if (data.size() > Protocol::MAX_DATA_SIZE) {
        LogManager::getInstance()->error("数据长度超过最大限制", "Protocol");
        return QByteArray();
    }
    
    QByteArray frame;
    
    // 帧头 (2字节)
    frame.append(static_cast<char>((Protocol::FRAME_HEADER >> 8) & 0xFF));
    frame.append(static_cast<char>(Protocol::FRAME_HEADER & 0xFF));
    
    // 命令码 (1字节)
    frame.append(static_cast<char>(command));
    
    // 数据长度 (1字节)
    frame.append(static_cast<char>(data.size()));
    
    // 数据区
    frame.append(data);
    
    // 校验和 - 使用配置的校验类型
    QByteArray checksumData = frame.mid(2); // 从命令码开始计算校验和
    
    if (m_enhancedChecksumEnabled) {
        ChecksumResult checksumResult = EnhancedChecksum::calculate(checksumData, m_checksumType);
        if (checksumResult.isValid) {
            frame.append(checksumResult.value);
            
            LogManager::getInstance()->debug(
                QString("使用%1校验，校验值: %2")
                .arg(EnhancedChecksum::checksumTypeToString(m_checksumType))
                .arg(ChecksumUtils::formatChecksum(checksumResult)),
                "Protocol"
            );
        } else {
            // 备用简单校验
            frame.append(static_cast<char>(calculateChecksum(checksumData)));
            LogManager::getInstance()->warning("增强校验失败，使用简单校验", "Protocol");
        }
    } else {
        // 传统简单校验
        frame.append(static_cast<char>(calculateChecksum(checksumData)));
    }
    
    // 帧尾 (1字节)
    frame.append(static_cast<char>(Protocol::FRAME_TAIL));
    
    LogManager::getInstance()->debug(
        QString("构建协议帧: 命令=%1, 长度=%2").arg(commandToString(command)).arg(data.size()),
        "Protocol"
    );
    
    return frame;
}

QByteArray ProtocolParser::buildResponseFrame(ProtocolCommand originalCommand, const QByteArray& data)
{
    // 构建响应帧，数据区包含原始命令码
    QByteArray responseData;
    responseData.append(static_cast<char>(originalCommand));
    responseData.append(data);
    
    return buildFrame(ProtocolCommand::Response, responseData);
}

QByteArray ProtocolParser::buildErrorFrame(ProtocolError error, const QString& message)
{
    QByteArray errorData;
    errorData.append(static_cast<char>(error));
    
    if (!message.isEmpty()) {
        QByteArray messageData = message.toUtf8();
        if (messageData.size() < Protocol::MAX_DATA_SIZE - 1) {
            errorData.append(messageData);
        }
    }
    
    return buildFrame(ProtocolCommand::Error, errorData);
}

QByteArray ProtocolParser::buildHeartbeatFrame()
{
    QByteArray data;
    data.append(static_cast<char>(Protocol::HEARTBEAT_TYPE_PING)); // 心跳标识
    
    QDateTime currentTime = QDateTime::currentDateTime();
    qint64 timestamp = currentTime.toMSecsSinceEpoch();
    
    // 添加时间戳 (8字节)
    for (int i = 7; i >= 0; --i) {
        data.append(static_cast<char>((timestamp >> (i * 8)) & 0xFF));
    }
    
    return buildFrame(ProtocolCommand::Heartbeat, data);
}

QByteArray ProtocolParser::buildParameterFrame(const QString& paramName, const QVariant& value)
{
    QByteArray data;
    
    // 参数名长度 (1字节)
    QByteArray nameData = paramName.toUtf8();
    if (nameData.size() > 255) {
        LogManager::getInstance()->error("参数名过长", "Protocol");
        return QByteArray();
    }
    
    data.append(static_cast<char>(nameData.size()));
    data.append(nameData);
    
    // 参数值类型 (1字节)
    QByteArray valueData;
    switch (value.typeId()) {
        case QMetaType::Int:
        case QMetaType::UInt: {
            data.append(static_cast<char>(Protocol::PARAM_TYPE_INT)); // 整数类型
            qint32 intValue = value.toInt();
            for (int i = 3; i >= 0; --i) {
                valueData.append(static_cast<char>((intValue >> (i * 8)) & 0xFF));
            }
            break;
        }
        case QMetaType::Double: {
            data.append(static_cast<char>(Protocol::PARAM_TYPE_DOUBLE)); // 浮点类型
            double doubleValue = value.toDouble();
            QByteArray doubleBytes(reinterpret_cast<const char*>(&doubleValue), sizeof(double));
            valueData = doubleBytes;
            break;
        }
        case QMetaType::QString: {
            data.append(static_cast<char>(Protocol::PARAM_TYPE_STRING)); // 字符串类型
            valueData = value.toString().toUtf8();
            if (valueData.size() > 255) {
                LogManager::getInstance()->error("参数值过长", "Protocol");
                return QByteArray();
            }
            QByteArray lengthByte;
            lengthByte.append(static_cast<char>(valueData.size()));
            valueData.prepend(lengthByte);
            break;
        }
        case QMetaType::Bool: {
            data.append(static_cast<char>(Protocol::PARAM_TYPE_BOOL)); // 布尔类型
            valueData.append(value.toBool() ? static_cast<char>(0x01) : static_cast<char>(0x00));
            break;
        }
        default:
            LogManager::getInstance()->error("不支持的参数类型", "Protocol");
            return QByteArray();
    }
    
    data.append(valueData);
    
    return buildFrame(ProtocolCommand::WriteParameter, data);
}

QByteArray ProtocolParser::buildMotionFrame(double x, double y, double z, double speed)
{
    QByteArray data;
    
    // 位置数据 (每个坐标4字节，共12字节)
    QList<double> positions = {x, y, z};
    for (double pos : positions) {
        float floatPos = static_cast<float>(pos);
        QByteArray posBytes(reinterpret_cast<const char*>(&floatPos), sizeof(float));
        data.append(posBytes);
    }
    
    // 速度数据 (4字节)
    float floatSpeed = static_cast<float>(speed);
    QByteArray speedBytes(reinterpret_cast<const char*>(&floatSpeed), sizeof(float));
    data.append(speedBytes);
    
    return buildFrame(ProtocolCommand::MoveToPosition, data);
}

QByteArray ProtocolParser::buildGlueFrame(double volume, double pressure, double temperature, int time)
{
    QByteArray data;
    
    // 胶量 (4字节)
    float floatVolume = static_cast<float>(volume);
    QByteArray volumeBytes(reinterpret_cast<const char*>(&floatVolume), sizeof(float));
    data.append(volumeBytes);
    
    // 压力 (4字节)
    float floatPressure = static_cast<float>(pressure);
    QByteArray pressureBytes(reinterpret_cast<const char*>(&floatPressure), sizeof(float));
    data.append(pressureBytes);
    
    // 温度 (4字节)
    float floatTemperature = static_cast<float>(temperature);
    QByteArray tempBytes(reinterpret_cast<const char*>(&floatTemperature), sizeof(float));
    data.append(tempBytes);
    
    // 时间 (4字节)
    for (int i = 3; i >= 0; --i) {
        data.append(static_cast<char>((time >> (i * 8)) & 0xFF));
    }
    
    return buildFrame(ProtocolCommand::SetGlueParameters, data);
}

bool ProtocolParser::parseParameterResponse(const QByteArray& data, QString& paramName, QVariant& value)
{
    if (data.isEmpty()) return false;
    
    int index = 0;
    
    // 读取参数名长度
    if (index >= data.size()) return false;
    quint8 nameLength = static_cast<quint8>(data[index++]);
    
    // 读取参数名
    if (index + nameLength > data.size()) return false;
    paramName = QString::fromUtf8(data.mid(index, nameLength));
    index += nameLength;
    
    // 读取参数值类型
    if (index >= data.size()) return false;
    quint8 valueType = static_cast<quint8>(data[index++]);
    
    // 根据类型解析参数值
    switch (valueType) {
        case Protocol::PARAM_TYPE_INT: { // 整数类型
            if (index + 4 > data.size()) return false;
            qint32 intValue = 0;
            for (int i = 0; i < 4; ++i) {
                intValue = (intValue << 8) | static_cast<quint8>(data[index++]);
            }
            value = intValue;
            break;
        }
        case Protocol::PARAM_TYPE_DOUBLE: { // 浮点类型
            if (index + 8 > data.size()) return false;
            double doubleValue;
            memcpy(&doubleValue, data.constData() + index, sizeof(double));
            value = doubleValue;
            index += 8;
            break;
        }
        case Protocol::PARAM_TYPE_STRING: { // 字符串类型
            if (index >= data.size()) return false;
            quint8 strLength = static_cast<quint8>(data[index++]);
            if (index + strLength > data.size()) return false;
            value = QString::fromUtf8(data.mid(index, strLength));
            index += strLength;
            break;
        }
        case Protocol::PARAM_TYPE_BOOL: { // 布尔类型
            if (index >= data.size()) return false;
            value = (data[index++] != 0);
            break;
        }
        default:
            LogManager::getInstance()->error("未知的参数值类型", "Protocol");
            return false;
    }
    
    return true;
}

bool ProtocolParser::parseMotionResponse(const QByteArray& data, double& x, double& y, double& z, double& speed)
{
    if (data.size() < Device::MOTION_DATA_SIZE) return false; // 需要至少16字节 (3个位置 + 1个速度)
    
    int index = 0;
    
    // 解析位置数据
    float floatX, floatY, floatZ, floatSpeed;
    
    memcpy(&floatX, data.constData() + index, sizeof(float));
    index += sizeof(float);
    
    memcpy(&floatY, data.constData() + index, sizeof(float));
    index += sizeof(float);
    
    memcpy(&floatZ, data.constData() + index, sizeof(float));
    index += sizeof(float);
    
    memcpy(&floatSpeed, data.constData() + index, sizeof(float));
    
    x = static_cast<double>(floatX);
    y = static_cast<double>(floatY);
    z = static_cast<double>(floatZ);
    speed = static_cast<double>(floatSpeed);
    
    return true;
}

bool ProtocolParser::parseGlueResponse(const QByteArray& data, double& volume, double& pressure, double& temperature, int& time)
{
    if (data.size() < Device::GLUE_DATA_SIZE) return false; // 需要至少16字节
    
    int index = 0;
    
    // 解析胶量
    float floatVolume;
    memcpy(&floatVolume, data.constData() + index, sizeof(float));
    volume = static_cast<double>(floatVolume);
    index += sizeof(float);
    
    // 解析压力
    float floatPressure;
    memcpy(&floatPressure, data.constData() + index, sizeof(float));
    pressure = static_cast<double>(floatPressure);
    index += sizeof(float);
    
    // 解析温度
    float floatTemperature;
    memcpy(&floatTemperature, data.constData() + index, sizeof(float));
    temperature = static_cast<double>(floatTemperature);
    index += sizeof(float);
    
    // 解析时间
    time = 0;
    for (int i = 0; i < 4; ++i) {
        time = (time << 8) | static_cast<quint8>(data[index++]);
    }
    
    return true;
}

bool ProtocolParser::validateFrameIntegrity(const ProtocolFrame& frame)
{
    // 检查命令码有效性
    if (static_cast<int>(frame.command) < 0x01 || static_cast<int>(frame.command) > 0xFF) {
        return false;
    }
    
    // 检查数据长度一致性
    if (frame.data.size() != frame.dataLength) {
        return false;
    }
    
    // 检查特定命令的数据长度要求
    switch (frame.command) {
        case ProtocolCommand::MoveToPosition:
            return frame.data.size() == Device::MOTION_DATA_SIZE; // 4个float值
        case ProtocolCommand::SetGlueParameters:
            return frame.data.size() == Device::GLUE_DATA_SIZE; // 3个float + 1个int
        case ProtocolCommand::Heartbeat:
            return frame.data.size() >= 1; // 至少包含心跳标识
        default:
            break; // 其他命令暂不检查
    }
    
    return true;
}

void ProtocolParser::processAdvancedFrame(const ProtocolFrame& frame)
{
    LogManager::getInstance()->debug(
        QString("处理高级协议帧: 命令=%1, 数据长度=%2")
        .arg(commandToString(frame.command))
        .arg(frame.data.size()),
        "Protocol"
    );
    
    switch (frame.command) {
        case ProtocolCommand::Heartbeat:
            processHeartbeatFrame(frame);
            break;
        case ProtocolCommand::MoveToPosition:
            processMotionFrame(frame);
            break;
        case ProtocolCommand::SetGlueParameters:
            processGlueFrame(frame);
            break;
        case ProtocolCommand::ReadParameter:
        case ProtocolCommand::WriteParameter:
            processParameterFrame(frame);
            break;
        default:
            // 发送给基础处理器
            emit frameReceived(frame);
            break;
    }
}

void ProtocolParser::processHeartbeatFrame(const ProtocolFrame& frame)
{
    if (frame.data.isEmpty()) return;
    
    quint8 heartbeatType = static_cast<quint8>(frame.data[0]);
    
    if (heartbeatType == Protocol::HEARTBEAT_TYPE_PING && frame.data.size() >= 9) {
        // 解析时间戳
        qint64 timestamp = 0;
        for (int i = 1; i < 9; ++i) {
            timestamp = (timestamp << 8) | static_cast<quint8>(frame.data[i]);
        }
        
        QDateTime receivedTime = QDateTime::fromMSecsSinceEpoch(timestamp);
        QDateTime currentTime = QDateTime::currentDateTime();
        qint64 delay = currentTime.msecsTo(receivedTime);
        
        LogManager::getInstance()->debug(
            QString("收到心跳包，延迟: %1ms").arg(delay),
            "Protocol"
        );
        
        emit heartbeatReceived(delay);
    }
}

void ProtocolParser::processMotionFrame(const ProtocolFrame& frame)
{
    double x, y, z, speed;
    if (parseMotionResponse(frame.data, x, y, z, speed)) {
        emit motionDataReceived(x, y, z, speed);
    } else {
        LogManager::getInstance()->error("解析运动数据失败", "Protocol");
        emit parseError("运动数据格式错误");
    }
}

void ProtocolParser::processGlueFrame(const ProtocolFrame& frame)
{
    double volume, pressure, temperature;
    int time;
    if (parseGlueResponse(frame.data, volume, pressure, temperature, time)) {
        emit glueDataReceived(volume, pressure, temperature, time);
    } else {
        LogManager::getInstance()->error("解析点胶数据失败", "Protocol");
        emit parseError("点胶数据格式错误");
    }
}

void ProtocolParser::processParameterFrame(const ProtocolFrame& frame)
{
    QString paramName;
    QVariant value;
    if (parseParameterResponse(frame.data, paramName, value)) {
        emit parameterReceived(paramName, value);
    } else {
        LogManager::getInstance()->error("解析参数数据失败", "Protocol");
        emit parseError("参数数据格式错误");
    }
}

quint8 ProtocolParser::calculateChecksum(const QByteArray& data)
{
    quint8 checksum = 0;
    for (char byte : data) {
        checksum += static_cast<quint8>(byte);
    }
    return checksum;
}

QString ProtocolParser::commandToString(ProtocolCommand command)
{
    switch (command) {
        case ProtocolCommand::DeviceStart: return "设备启动";
        case ProtocolCommand::DeviceStop: return "设备停止";
        case ProtocolCommand::DeviceReset: return "设备复位";
        case ProtocolCommand::DeviceStatus: return "设备状态";
        case ProtocolCommand::ReadParameter: return "读取参数";
        case ProtocolCommand::WriteParameter: return "写入参数";
        case ProtocolCommand::ReadAllParameters: return "读取所有参数";
        case ProtocolCommand::WriteAllParameters: return "写入所有参数";
        case ProtocolCommand::ReadSensorData: return "读取传感器数据";
        case ProtocolCommand::ReadAllSensors: return "读取所有传感器";
        case ProtocolCommand::StartDataCollection: return "开始数据采集";
        case ProtocolCommand::StopDataCollection: return "停止数据采集";
        case ProtocolCommand::GetDeviceInfo: return "获取设备信息";
        case ProtocolCommand::GetVersionInfo: return "获取版本信息";
        case ProtocolCommand::SetDateTime: return "设置日期时间";
        case ProtocolCommand::GetDateTime: return "获取日期时间";
        case ProtocolCommand::StartUpgrade: return "开始升级";
        case ProtocolCommand::UpgradeData: return "升级数据";
        case ProtocolCommand::EndUpgrade: return "结束升级";
        case ProtocolCommand::Response: return "响应";
        case ProtocolCommand::Error: return "错误";
        case ProtocolCommand::Heartbeat: return "心跳包";
        case ProtocolCommand::MoveToPosition: return "移动到位置";
        case ProtocolCommand::SetGlueParameters: return "设置点胶参数";
        default: return QString("未知命令(0x%1)").arg(static_cast<int>(command), 2, 16, QChar('0')).toUpper();
    }
}

QString ProtocolParser::errorToString(ProtocolError error)
{
    switch (error) {
        case ProtocolError::None: return "无错误";
        case ProtocolError::InvalidCommand: return "无效命令";
        case ProtocolError::InvalidParameter: return "无效参数";
        case ProtocolError::ChecksumError: return "校验错误";
        case ProtocolError::DeviceNotReady: return "设备未就绪";
        case ProtocolError::DataTooLong: return "数据过长";
        case ProtocolError::TimeoutError: return "超时错误";
        case ProtocolError::UnknownError: return "未知错误";
        default: return QString("未知错误码(0x%1)").arg(static_cast<int>(error), 2, 16, QChar('0')).toUpper();
    }
}

void ProtocolParser::setTimeout(int timeout)
{
    timeoutMs = timeout;
}

void ProtocolParser::clearBuffer()
{
    receiveBuffer.clear();
    frameQueue.clear();
    timeoutTimer->stop();
}

void ProtocolParser::onTimeout()
{
    LogManager::getInstance()->warning("协议解析超时", "Protocol");
    emit timeoutOccurred();
}

bool ProtocolParser::findFrame(QByteArray& frame)
{
    // 寻找帧头
    int headerIndex = -1;
    for (int i = 0; i < receiveBuffer.size() - 1; ++i) {
        quint16 header = (static_cast<quint8>(receiveBuffer[i]) << 8) | static_cast<quint8>(receiveBuffer[i + 1]);
        if (header == Protocol::FRAME_HEADER) {
            headerIndex = i;
            break;
        }
    }
    
    if (headerIndex == -1) {
        // 没有找到帧头，清空缓冲区
        if (receiveBuffer.size() > 100) {
            receiveBuffer.clear();
        }
        return false;
    }
    
    // 移除帧头之前的无效数据
    if (headerIndex > 0) {
        receiveBuffer.remove(0, headerIndex);
    }
    
    // 检查是否有足够的数据构成最小帧
    if (receiveBuffer.size() < Protocol::MIN_FRAME_SIZE) {
        return false;
    }
    
    // 读取数据长度
    quint8 dataLength = static_cast<quint8>(receiveBuffer[3]);
    int totalFrameSize = Protocol::MIN_FRAME_SIZE + dataLength;
    
    // 检查是否有完整的帧
    if (receiveBuffer.size() < totalFrameSize) {
        return false;
    }
    
    // 提取完整帧
    frame = receiveBuffer.left(totalFrameSize);
    receiveBuffer.remove(0, totalFrameSize);
    
    return true;
}

bool ProtocolParser::validateFrame(const QByteArray& frameData, ProtocolFrame& frame)
{
    if (frameData.size() < Protocol::MIN_FRAME_SIZE) {
        LogManager::getInstance()->warning("帧数据长度不足", "Protocol");
        return false;
    }
    
    // 解析帧头
    frame.header = (static_cast<quint8>(frameData[0]) << 8) | static_cast<quint8>(frameData[1]);
    if (frame.header != Protocol::FRAME_HEADER) {
        LogManager::getInstance()->warning("帧头错误", "Protocol");
        return false;
    }
    
    // 解析命令码
    frame.command = static_cast<ProtocolCommand>(frameData[2]);
    
    // 解析数据长度
    frame.dataLength = static_cast<quint8>(frameData[3]);
    
    // 解析数据区
    if (frame.dataLength > 0) {
        frame.data = frameData.mid(4, frame.dataLength);
    }
    
    // 解析校验和
    frame.checksum = static_cast<quint8>(frameData[4 + frame.dataLength]);
    
    // 解析帧尾
    frame.tail = static_cast<quint8>(frameData[5 + frame.dataLength]);
    if (frame.tail != Protocol::FRAME_TAIL) {
        LogManager::getInstance()->warning("帧尾错误", "Protocol");
        return false;
    }
    
    // 验证校验和 - 支持CRC16和简单校验
    QByteArray checksumData = frameData.mid(2, 2 + frame.dataLength); // 从命令码到数据区结束
    
    // 检查是否为CRC16校验（帧长度足够且校验字段为2字节）
    if (frameData.size() >= Protocol::MIN_FRAME_SIZE + frame.dataLength + 1) {
        // 可能是CRC16校验
        QByteArray expectedCrcBytes = frameData.mid(4 + frame.dataLength, 2);
        ChecksumResult calculatedCrc = EnhancedChecksum::calculate(checksumData, ChecksumType::CRC16_MODBUS);
        
        if (calculatedCrc.isValid && calculatedCrc.value == expectedCrcBytes) {
            // CRC16校验通过
            frame.checksum = ChecksumUtils::bytesToUInt16(expectedCrcBytes);
        } else {
            // 尝试简单校验和
            quint8 calculatedChecksum = calculateChecksum(checksumData);
            if (frame.checksum != calculatedChecksum) {
                // 使用增强的错误检测
                ChecksumResult expectedChecksum(ChecksumType::Simple, 
                                              QByteArray(1, static_cast<char>(frame.checksum)));
                EnhancedChecksum::ErrorDetectionResult errorResult = 
                    EnhancedChecksum::detectErrors(checksumData, expectedChecksum);
                
                QString errorMsg = QString("校验错误: 期望=0x%1, 计算=0x%2")
                                 .arg(frame.checksum, 2, 16, QChar('0'))
                                 .arg(calculatedChecksum, 2, 16, QChar('0'));
                
                if (errorResult.hasError) {
                    errorMsg += QString(" - %1").arg(errorResult.errorDescription);
                    
                    if (errorResult.canCorrect) {
                        LogManager::getInstance()->warning(
                            QString("%1 (可纠正)").arg(errorMsg), "Protocol"
                        );
                        // 可以考虑使用纠正后的数据
                    } else {
                        LogManager::getInstance()->error(errorMsg, "Protocol");
                        return false;
                    }
                } else {
                    LogManager::getInstance()->warning(errorMsg, "Protocol");
                    return false;
                }
            }
        }
    } else {
        // 传统简单校验
        quint8 calculatedChecksum = calculateChecksum(checksumData);
        if (frame.checksum != calculatedChecksum) {
            LogManager::getInstance()->warning(
                QString("校验和错误: 期望=0x%1, 实际=0x%2")
                .arg(calculatedChecksum, 2, 16, QChar('0'))
                .arg(frame.checksum, 2, 16, QChar('0')),
                "Protocol"
            );
            return false;
        }
    }
    
    return true;
}

void ProtocolParser::processCompleteFrame(const ProtocolFrame& frame)
{
    LogManager::getInstance()->debug(
        QString("接收到完整帧: 命令=%1, 长度=%2")
        .arg(commandToString(frame.command))
        .arg(frame.dataLength),
        "Protocol"
    );
    
    emit frameReceived(frame);
}

// 增强的帧完整性检查实现
EnhancedChecksum::FrameIntegrityResult ProtocolParser::checkAdvancedFrameIntegrity(const QByteArray& frameData)
{
    return EnhancedChecksum::checkFrameIntegrity(
        frameData, 
        Protocol::FRAME_HEADER, 
        Protocol::FRAME_TAIL, 
        m_checksumType
    );
}

// 校验类型管理方法实现
void ProtocolParser::setChecksumType(ChecksumType type)
{
    if (EnhancedChecksum::isChecksumTypeSupported(type)) {
        m_checksumType = type;
        LogManager::getInstance()->info(
            QString("校验类型已设置为: %1").arg(EnhancedChecksum::checksumTypeToString(type)),
            "Protocol"
        );
    } else {
        LogManager::getInstance()->warning(
            QString("不支持的校验类型，保持当前设置: %1")
            .arg(EnhancedChecksum::checksumTypeToString(m_checksumType)),
            "Protocol"
        );
    }
}

ChecksumType ProtocolParser::getChecksumType() const
{
    return m_checksumType;
}

bool ProtocolParser::isEnhancedChecksumEnabled() const
{
    return m_enhancedChecksumEnabled;
}

void ProtocolParser::enableEnhancedChecksum(bool enabled)
{
    m_enhancedChecksumEnabled = enabled;
    LogManager::getInstance()->info(
        QString("增强校验已%1").arg(enabled ? "启用" : "禁用"),
        "Protocol"
    );
}

void ProtocolParser::preallocateMemory()
{
    // 预分配缓冲区
    receiveBuffer.reserve(Protocol::MAX_BUFFER_SIZE);
    m_tempFrameBuffer.reserve(Protocol::MAX_BUFFER_SIZE);
    
    // 预分配帧队列空间
    frameQueue.reserve(50);
}

void ProtocolParser::optimizeBuffer()
{
    if (m_bufferOptimized) return;
    
    // 如果缓冲区前半部分没有有效数据，清理掉
    if (m_bufferSearchStart > receiveBuffer.size() / 2) {
        receiveBuffer.remove(0, m_bufferSearchStart);
        m_bufferSearchStart = 0;
        m_lastHeaderPos = -1;
    }
    
    m_bufferOptimized = true;
}

bool ProtocolParser::findFrameOptimized(QByteArray& frame)
{
    // 从上次搜索位置开始寻找帧头
    int searchStart = qMax(m_bufferSearchStart, 0);
    int headerIndex = -1;
    
    // 优化的帧头搜索 - 使用快速内存比较
    for (int i = searchStart; i < receiveBuffer.size() - 1; ++i) {
        if (static_cast<quint8>(receiveBuffer[i]) == (Protocol::FRAME_HEADER >> 8) &&
            static_cast<quint8>(receiveBuffer[i + 1]) == (Protocol::FRAME_HEADER & 0xFF)) {
            headerIndex = i;
            break;
        }
    }
    
    if (headerIndex == -1) {
        // 没有找到帧头，优化搜索起始位置
        m_bufferSearchStart = qMax(0, receiveBuffer.size() - 1);
        
        // 如果缓冲区过大且没有找到帧头，清理前面的数据
        if (receiveBuffer.size() > 1024) {
            receiveBuffer.remove(0, receiveBuffer.size() - 512);
            m_bufferSearchStart = 0;
        }
        return false;
    }
    
    // 移除帧头之前的无效数据
    if (headerIndex > 0) {
        receiveBuffer.remove(0, headerIndex);
        m_bufferSearchStart = 0;
    }
    
    // 检查是否有足够的数据构成最小帧
    if (receiveBuffer.size() < Protocol::MIN_FRAME_SIZE) {
        return false;
    }
    
    // 读取数据长度
    quint8 dataLength = static_cast<quint8>(receiveBuffer[3]);
    int totalFrameSize = Protocol::MIN_FRAME_SIZE + dataLength;
    
    // 检查是否有完整的帧
    if (receiveBuffer.size() < totalFrameSize) {
        return false;
    }
    
    // 使用预分配的缓冲区
    if (m_tempFrameBuffer.size() < totalFrameSize) {
        m_tempFrameBuffer.resize(totalFrameSize);
    }
    
    // 快速内存拷贝
    memcpy(m_tempFrameBuffer.data(), receiveBuffer.constData(), totalFrameSize);
    frame = m_tempFrameBuffer.left(totalFrameSize);
    
    // 移除已处理的数据
    receiveBuffer.remove(0, totalFrameSize);
    
    return true;
}

// 获取性能统计信息
QString ProtocolParser::getPerformanceStats() const
{
    QDateTime now = QDateTime::currentDateTime();
    qint64 elapsedSecs = m_perfStats.lastStatsUpdate.secsTo(now);
    
    double throughput = elapsedSecs > 0 ? 
                       static_cast<double>(m_perfStats.totalBytesProcessed) / elapsedSecs : 0;
    
    return QString(
        "协议解析性能统计:\n"
        "总字节数: %1\n"
        "总帧数: %2\n"
        "平均解析时间: %3ms\n"
        "吞吐量: %4 字节/秒\n"
        "缓冲区大小: %5\n"
    ).arg(m_perfStats.totalBytesProcessed)
     .arg(m_perfStats.totalFramesProcessed)
     .arg(m_perfStats.averageParseTime)
     .arg(throughput, 0, 'f', 2)
     .arg(receiveBuffer.size());
} 