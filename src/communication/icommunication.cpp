#include "icommunication.h"
#include "serialcommunication.h"
#include "tcpcommunication.h"
// #include "udpcommunication.h"  // 暂未实现
// #include "cancommunication.h"  // 暂未实现
// #include "modbuscommunication.h"  // 暂未实现
#include "logger/logmanager.h"

// 工厂类实现
ICommunication* CommunicationFactory::createCommunication(CommunicationType type, QObject* parent)
{
    switch (type) {
        case CommunicationType::Serial:
            return new SerialCommunication(parent);
            
        case CommunicationType::TCP:
            return new TcpCommunication(parent);
            
        case CommunicationType::UDP:
            // return new UdpCommunication(parent);  // 暂未实现
            LogManager::getInstance()->warning("UDP通讯暂未实现", "CommunicationFactory");
            return nullptr;
            
        case CommunicationType::CAN:
            // return new CanCommunication(parent);  // 暂未实现
            LogManager::getInstance()->warning("CAN通讯暂未实现", "CommunicationFactory");
            return nullptr;
            
        case CommunicationType::Modbus:
            // return new ModbusCommunication(parent);  // 暂未实现
            LogManager::getInstance()->warning("Modbus通讯暂未实现", "CommunicationFactory");
            return nullptr;
            
        default:
            LogManager::getInstance()->error(
                QString("不支持的通讯类型: %1").arg(static_cast<int>(type)),
                "CommunicationFactory"
            );
            return nullptr;
    }
}

QStringList CommunicationFactory::getSupportedTypes()
{
    return {
        communicationTypeToString(CommunicationType::Serial),
        communicationTypeToString(CommunicationType::TCP),
        // communicationTypeToString(CommunicationType::UDP),    // 暂未实现
        // communicationTypeToString(CommunicationType::CAN),    // 暂未实现
        // communicationTypeToString(CommunicationType::Modbus)  // 暂未实现
    };
}

QString CommunicationFactory::getTypeDescription(CommunicationType type)
{
    switch (type) {
        case CommunicationType::Serial:
            return "串口通讯 - 支持RS232/RS485串行通讯";
        case CommunicationType::TCP:
            return "TCP网络通讯 - 基于TCP/IP协议的网络通讯";
        case CommunicationType::UDP:
            return "UDP网络通讯 - 基于UDP协议的网络通讯";
        case CommunicationType::CAN:
            return "CAN总线通讯 - 控制器局域网络通讯";
        case CommunicationType::Modbus:
            return "Modbus通讯 - 工业标准Modbus协议通讯";
        default:
            return "未知通讯类型";
    }
}

bool CommunicationFactory::isTypeSupported(CommunicationType type)
{
    switch (type) {
        case CommunicationType::Serial:
        case CommunicationType::TCP:
            return true;
        case CommunicationType::UDP:
        case CommunicationType::CAN:
        case CommunicationType::Modbus:
            return false;  // 暂未实现
        default:
            return false;
    }
}

// 辅助函数实现
QString connectionStateToString(ConnectionState state)
{
    switch (state) {
        case ConnectionState::Disconnected:
            return Strings::STATUS_DISCONNECTED;
        case ConnectionState::Connecting:
            return Strings::STATUS_CONNECTING;
        case ConnectionState::Connected:
            return Strings::STATUS_CONNECTED;
        case ConnectionState::Reconnecting:
            return Strings::STATUS_RECONNECTING;
        case ConnectionState::Error:
            return Strings::STATUS_ERROR;
        default:
            return "未知状态";
    }
}

QString communicationTypeToString(CommunicationType type)
{
    switch (type) {
        case CommunicationType::Serial:
            return "串口";
        case CommunicationType::TCP:
            return "TCP";
        case CommunicationType::UDP:
            return "UDP";
        case CommunicationType::CAN:
            return "CAN";
        case CommunicationType::Modbus:
            return "Modbus";
        default:
            return "未知";
    }
}

CommunicationType stringToCommunicationType(const QString& typeString)
{
    QString lowerType = typeString.toLower();
    
    if (lowerType == "串口" || lowerType == "serial") {
        return CommunicationType::Serial;
    } else if (lowerType == "tcp") {
        return CommunicationType::TCP;
    } else if (lowerType == "udp") {
        return CommunicationType::UDP;
    } else if (lowerType == "can") {
        return CommunicationType::CAN;
    } else if (lowerType == "modbus") {
        return CommunicationType::Modbus;
    } else {
        return CommunicationType::Serial;  // 默认返回串口
    }
}

ConnectionState stringToConnectionState(const QString& stateString)
{
    QString lowerState = stateString.toLower();
    
    if (lowerState == "disconnected" || lowerState == "未连接") {
        return ConnectionState::Disconnected;
    } else if (lowerState == "connecting" || lowerState == "连接中") {
        return ConnectionState::Connecting;
    } else if (lowerState == "connected" || lowerState == "已连接") {
        return ConnectionState::Connected;
    } else if (lowerState == "reconnecting" || lowerState == "重连中") {
        return ConnectionState::Reconnecting;
    } else if (lowerState == "error" || lowerState == "错误") {
        return ConnectionState::Error;
    } else {
        return ConnectionState::Disconnected;  // 默认返回断开连接
    }
} 