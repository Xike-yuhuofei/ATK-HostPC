#pragma once

#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMutex>
#include <QTimer>
#include <QQueue>

enum class LogLevel {
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
    Critical = 4
};

struct LogEntry {
    QDateTime timestamp;
    LogLevel level;
    QString category;
    QString message;
    QString file;
    int line;
    QString function;
};

class LogManager : public QObject
{
    Q_OBJECT

public:
    static LogManager* getInstance();
    
    // 日志记录方法
    void log(LogLevel level, const QString& category, const QString& message, 
             const QString& file = QString(), int line = 0, const QString& function = QString());
    
    void debug(const QString& message, const QString& category = "App");
    void info(const QString& message, const QString& category = "App");
    void warning(const QString& message, const QString& category = "App");
    void error(const QString& message, const QString& category = "App");
    void critical(const QString& message, const QString& category = "App");
    
    // 通讯日志
    void logCommunication(const QString& direction, const QByteArray& data, const QString& port = QString());
    
    // 设置日志级别
    void setLogLevel(LogLevel level);
    LogLevel getLogLevel() const;
    
    // 设置日志文件
    void setLogFile(const QString& filename);
    
    // 启用/禁用控制台输出
    void setConsoleOutput(bool enabled);
    
    // 日志文件管理
    void rotateLogFile();
    void cleanupOldLogFiles();
    
    // 获取日志条目
    QList<LogEntry> getLogEntries(int count = 1000) const;
    
    // 导出日志
    bool exportLogs(const QString& filename, const QDateTime& startTime = QDateTime(), 
                    const QDateTime& endTime = QDateTime()) const;

signals:
    void newLogEntry(const LogEntry& entry);

private slots:
    void processLogQueue();
    void checkFileSize();

private:
    explicit LogManager(QObject* parent = nullptr);
    ~LogManager();
    
    void writeToFile(const LogEntry& entry);
    void writeToConsole(const LogEntry& entry);
    QString formatLogEntry(const LogEntry& entry) const;
    QString logLevelToString(LogLevel level) const;
    
    static LogManager* instance;
    static QMutex mutex;
    
    LogLevel currentLogLevel;
    QFile* logFile;
    QTextStream* logStream;
    QString logFilename;
    bool consoleOutputEnabled;
    
    mutable QMutex logMutex;
    QQueue<LogEntry> logQueue;
    QTimer* processTimer;
    QTimer* fileSizeTimer;
    
    qint64 maxFileSize;
    int maxLogFiles;
    
    // 内存中的日志条目（用于界面显示）
    QList<LogEntry> logEntries;
    int maxMemoryEntries;
};

// 便捷宏定义
#define LOG_DEBUG(msg) LogManager::getInstance()->debug(msg, __FILE__)
#define LOG_INFO(msg) LogManager::getInstance()->info(msg, __FILE__)
#define LOG_WARNING(msg) LogManager::getInstance()->warning(msg, __FILE__)
#define LOG_ERROR(msg) LogManager::getInstance()->error(msg, __FILE__)
#define LOG_CRITICAL(msg) LogManager::getInstance()->critical(msg, __FILE__)

#define LOG_COMM_TX(data, port) LogManager::getInstance()->logCommunication("TX", data, port)
#define LOG_COMM_RX(data, port) LogManager::getInstance()->logCommunication("RX", data, port) 