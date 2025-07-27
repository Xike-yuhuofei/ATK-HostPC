#pragma once

#include <QtGlobal>
#include <QByteArray>

// 协议常量定义
namespace Protocol {
    // 帧格式常量
    static constexpr quint16 FRAME_HEADER = 0xAA55;
    static constexpr quint8 FRAME_TAIL = 0x0D;
    static constexpr int MIN_FRAME_SIZE = 6;        // 帧头(2) + 命令(1) + 长度(1) + 校验(1) + 帧尾(1)
    static constexpr int MAX_FRAME_SIZE = 261;       // 帧头(2) + 命令(1) + 最大数据长度(255) + 校验(1) + 帧尾(1)
    static constexpr int MAX_DATA_SIZE = 255;       // 最大数据长度
    static constexpr int MAX_BUFFER_SIZE = 2048;    // 最大缓冲区大小
    
    // 参数类型定义
    static constexpr quint8 PARAM_TYPE_INT = 0x01;
    static constexpr quint8 PARAM_TYPE_DOUBLE = 0x02;
    static constexpr quint8 PARAM_TYPE_STRING = 0x03;
    static constexpr quint8 PARAM_TYPE_BOOL = 0x04;
    
    // 心跳相关常量
    static constexpr quint8 HEARTBEAT_TYPE_PING = 0x01;
    static constexpr quint8 HEARTBEAT_TYPE_PONG = 0x02;
    
    // 超时设置
    static constexpr int DEFAULT_TIMEOUT = 5000;     // 默认超时时间(ms)
    static constexpr int HEARTBEAT_INTERVAL = 3000;  // 心跳间隔(ms)
    static constexpr int CONNECTION_TIMEOUT = 10000; // 连接超时(ms)
    
    // 重连设置
    static constexpr int MAX_RECONNECT_ATTEMPTS = 3;
    static constexpr int RECONNECT_DELAY = 5000;     // 重连延迟(ms)
}

// 系统常量定义
namespace System {
    // 统计更新间隔
    static constexpr int STATISTICS_UPDATE_INTERVAL = 1000;
    
    // 日志相关
    static constexpr int MAX_LOG_LINES = 1000;
    static constexpr int MAX_LOG_FILE_SIZE = 10 * 1024 * 1024; // 10MB
    
    // 数据库相关
    static constexpr int DB_CONNECTION_TIMEOUT = 30000;
    static constexpr int DB_QUERY_TIMEOUT = 10000;
    static constexpr int DB_BACKUP_INTERVAL = 3600;  // 1小时
    
    // 界面更新间隔
    static constexpr int UI_UPDATE_INTERVAL = 100;   // 100ms
    static constexpr int STATUS_UPDATE_INTERVAL = 1000; // 1秒
    
    // 性能监控
    static constexpr int PERFORMANCE_MONITOR_INTERVAL = 5000; // 5秒
    
    // 自动保存间隔
    static constexpr int AUTO_SAVE_INTERVAL = 300000; // 5分钟
}

// 设备相关常量
namespace Device {
    // 运动控制限制
    static constexpr double MAX_POSITION_X = 1000.0;
    static constexpr double MAX_POSITION_Y = 1000.0;
    static constexpr double MAX_POSITION_Z = 100.0;
    static constexpr double MIN_POSITION = 0.0;
    static constexpr double MAX_SPEED = 100.0;
    static constexpr double MIN_SPEED = 0.1;
    
    // 点胶参数限制
    static constexpr double MAX_VOLUME = 10.0;     // 最大胶量(ml)
    static constexpr double MIN_VOLUME = 0.01;     // 最小胶量(ml)
    static constexpr double MAX_PRESSURE = 10.0;   // 最大压力(MPa)
    static constexpr double MIN_PRESSURE = 0.1;    // 最小压力(MPa)
    static constexpr double MAX_TEMPERATURE = 80.0; // 最大温度(℃)
    static constexpr double MIN_TEMPERATURE = 10.0; // 最小温度(℃)
    static constexpr int MAX_TIME = 60000;          // 最大时间(ms)
    static constexpr int MIN_TIME = 10;             // 最小时间(ms)
    
    // 状态检查间隔
    static constexpr int STATUS_CHECK_INTERVAL = 500;
    
    // 传感器数据格式
    static constexpr int SENSOR_DATA_SIZE = 12;     // 传感器数据字节数
    static constexpr int MOTION_DATA_SIZE = 16;     // 运动数据字节数
    static constexpr int GLUE_DATA_SIZE = 16;       // 点胶数据字节数
}

// 通讯配置常量
namespace Communication {
    // 串口默认配置
    static constexpr int DEFAULT_BAUD_RATE = 9600;
    static constexpr int DEFAULT_DATA_BITS = 8;
    static constexpr int DEFAULT_STOP_BITS = 1;
    
    // TCP默认配置
    static constexpr quint16 DEFAULT_TCP_PORT = 502;
    static constexpr int TCP_CONNECT_TIMEOUT = 5000;
    static constexpr int TCP_READ_TIMEOUT = 3000;
    
    // CAN默认配置
    static constexpr int DEFAULT_CAN_BITRATE = 250000;
    static constexpr int CAN_FRAME_TIMEOUT = 1000;
    
    // 缓冲区大小
    static constexpr int SEND_BUFFER_SIZE = 1024;
    static constexpr int RECEIVE_BUFFER_SIZE = 2048;
    
    // 重试配置
    static constexpr int MAX_SEND_RETRIES = 3;
    static constexpr int RETRY_DELAY = 1000;
}

// 错误码定义
namespace ErrorCodes {
    // 通讯错误
    static constexpr int COMM_SUCCESS = 0;
    static constexpr int COMM_TIMEOUT = 1001;
    static constexpr int COMM_DISCONNECTED = 1002;
    static constexpr int COMM_INVALID_DATA = 1003;
    static constexpr int COMM_CHECKSUM_ERROR = 1004;
    static constexpr int COMM_BUFFER_OVERFLOW = 1005;
    
    // 设备错误
    static constexpr int DEVICE_NOT_READY = 2001;
    static constexpr int DEVICE_BUSY = 2002;
    static constexpr int DEVICE_FAULT = 2003;
    static constexpr int DEVICE_EMERGENCY_STOP = 2004;
    
    // 参数错误
    static constexpr int PARAM_OUT_OF_RANGE = 3001;
    static constexpr int PARAM_INVALID_TYPE = 3002;
    static constexpr int PARAM_READ_ONLY = 3003;
    
    // 系统错误
    static constexpr int SYSTEM_INSUFFICIENT_MEMORY = 4001;
    static constexpr int SYSTEM_FILE_ERROR = 4002;
    static constexpr int SYSTEM_DATABASE_ERROR = 4003;
}

// 应用程序信息
namespace AppInfo {
    static constexpr const char* APP_NAME = "工业点胶设备上位机控制软件";
    static constexpr const char* APP_VERSION = "2.0.0";
    static constexpr const char* APP_ORGANIZATION = "Industrial Solutions";
    static constexpr const char* APP_DOMAIN = "industrial-solutions.com";
    static constexpr const char* CONFIG_FILE_NAME = "config.ini";
    static constexpr const char* LOG_DIR_NAME = "logs";
    static constexpr const char* DATA_DIR_NAME = "data";
    static constexpr const char* BACKUP_DIR_NAME = "backup";
}

// 字符串常量
namespace Strings {
    // 连接状态
    static constexpr const char* STATUS_CONNECTED = "已连接";
    static constexpr const char* STATUS_DISCONNECTED = "未连接";
    static constexpr const char* STATUS_CONNECTING = "连接中";
    static constexpr const char* STATUS_RECONNECTING = "重连中";
    static constexpr const char* STATUS_ERROR = "错误";
    
    // 设备状态
    static constexpr const char* DEVICE_IDLE = "空闲";
    static constexpr const char* DEVICE_RUNNING = "运行中";
    static constexpr const char* DEVICE_PAUSED = "暂停";
    static constexpr const char* DEVICE_STOPPED = "已停止";
    static constexpr const char* DEVICE_ERROR = "错误";
    static constexpr const char* DEVICE_EMERGENCY = "紧急停止";
    
    // 日志级别
    static constexpr const char* LOG_DEBUG = "DEBUG";
    static constexpr const char* LOG_INFO = "INFO";
    static constexpr const char* LOG_WARNING = "WARNING";
    static constexpr const char* LOG_ERROR = "ERROR";
    static constexpr const char* LOG_CRITICAL = "CRITICAL";
}

// 颜色定义
namespace Colors {
    static constexpr const char* COLOR_SUCCESS = "#4CAF50";
    static constexpr const char* COLOR_WARNING = "#FF9800";
    static constexpr const char* COLOR_ERROR = "#F44336";
    static constexpr const char* COLOR_INFO = "#2196F3";
    static constexpr const char* COLOR_DISABLED = "#9E9E9E";
    static constexpr const char* COLOR_BACKGROUND = "#FAFAFA";
    static constexpr const char* COLOR_BORDER = "#E0E0E0";
}

// 验证规则
namespace Validation {
    // 字符串长度限制
    static constexpr int MAX_NAME_LENGTH = 64;
    static constexpr int MAX_MESSAGE_LENGTH = 512;
    static constexpr int MAX_PATH_LENGTH = 260;
    
    // 数值范围验证
    static constexpr double EPSILON = 1e-6;        // 浮点数比较精度
    static constexpr int MAX_RETRY_COUNT = 10;
    static constexpr int MIN_RETRY_COUNT = 1;
} 