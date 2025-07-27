#include "logmanager.h"
#include "config/configmanager.h"
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QCoreApplication>
#include <QFileInfo>
#include <QTextStream>
#include <iostream>

LogManager* LogManager::instance = nullptr;
QMutex LogManager::mutex;

LogManager* LogManager::getInstance()
{
    QMutexLocker locker(&mutex);
    if (!instance) {
        instance = new LogManager();
    }
    return instance;
}

LogManager::LogManager(QObject* parent)
    : QObject(parent)
    , currentLogLevel(LogLevel::Info)
    , logFile(nullptr)
    , logStream(nullptr)
    , consoleOutputEnabled(true)
    , maxFileSize(10 * 1024 * 1024) // 10MB
    , maxLogFiles(10)
    , maxMemoryEntries(5000)
{
    // 设置日志文件路径
    QString logPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs";
    QDir().mkpath(logPath);
    
    QString logFilePath = logPath + "/app.log";
    setLogFile(logFilePath);
    
    // 创建定时器
    processTimer = new QTimer(this);
    connect(processTimer, &QTimer::timeout, this, &LogManager::processLogQueue);
    processTimer->start(100); // 每100ms处理一次日志队列
    
    fileSizeTimer = new QTimer(this);
    connect(fileSizeTimer, &QTimer::timeout, this, &LogManager::checkFileSize);
    fileSizeTimer->start(60000); // 每分钟检查一次文件大小
    
    // 从配置加载设置
    ConfigManager* config = ConfigManager::getInstance();
    QString levelStr = config->getLogLevel();
    if (levelStr == "Debug") currentLogLevel = LogLevel::Debug;
    else if (levelStr == "Info") currentLogLevel = LogLevel::Info;
    else if (levelStr == "Warning") currentLogLevel = LogLevel::Warning;
    else if (levelStr == "Error") currentLogLevel = LogLevel::Error;
    else if (levelStr == "Critical") currentLogLevel = LogLevel::Critical;
    
    maxLogFiles = config->getLogMaxFiles();
    maxFileSize = config->getLogMaxSize();
    
    // 记录启动日志
    info("日志管理器已启动");
}

LogManager::~LogManager()
{
    info("日志管理器正在关闭");
    
    // 处理剩余的日志队列
    processLogQueue();
    
    if (logStream) {
        logStream->flush();
        delete logStream;
    }
    
    if (logFile) {
        logFile->close();
        delete logFile;
    }
}

void LogManager::log(LogLevel level, const QString& category, const QString& message, 
                    const QString& file, int line, const QString& function)
{
    if (level < currentLogLevel) {
        return;
    }
    
    LogEntry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.level = level;
    entry.category = category;
    entry.message = message;
    entry.file = file;
    entry.line = line;
    entry.function = function;
    
    {
        QMutexLocker locker(&logMutex);
        logQueue.enqueue(entry);
        
        // 添加到内存中的日志条目
        logEntries.append(entry);
        if (logEntries.size() > maxMemoryEntries) {
            logEntries.removeFirst();
        }
    }
    
    emit newLogEntry(entry);
}

void LogManager::debug(const QString& message, const QString& category)
{
    log(LogLevel::Debug, category, message);
}

void LogManager::info(const QString& message, const QString& category)
{
    log(LogLevel::Info, category, message);
}

void LogManager::warning(const QString& message, const QString& category)
{
    log(LogLevel::Warning, category, message);
}

void LogManager::error(const QString& message, const QString& category)
{
    log(LogLevel::Error, category, message);
}

void LogManager::critical(const QString& message, const QString& category)
{
    log(LogLevel::Critical, category, message);
}

void LogManager::logCommunication(const QString& direction, const QByteArray& data, const QString& port)
{
    QString hexData = data.toHex(' ').toUpper();
    QString message = QString("[%1] %2: %3").arg(direction, port.isEmpty() ? "Unknown" : port, hexData);
    log(LogLevel::Debug, "Communication", message);
}

void LogManager::setLogLevel(LogLevel level)
{
    currentLogLevel = level;
    info(QString("日志级别已设置为: %1").arg(logLevelToString(level)));
}

LogLevel LogManager::getLogLevel() const
{
    return currentLogLevel;
}

/**
 * @brief 线程安全的设置日志文件
 * 
 * 安全地切换日志文件，确保资源正确释放
 * @param filename 新的日志文件路径
 */
void LogManager::setLogFile(const QString& filename)
{
    QMutexLocker locker(&logMutex);
    
    try {
        // 安全关闭现有资源
        if (logStream) {
            logStream->flush();
            delete logStream;
            logStream = nullptr;
        }
        
        if (logFile) {
            if (logFile->isOpen()) {
                logFile->flush();
                logFile->close();
            }
            delete logFile;
            logFile = nullptr;
        }
        
        logFilename = filename;
        
        // 确保目录存在
        QFileInfo fileInfo(filename);
        QDir dir = fileInfo.dir();
        if (!dir.exists()) {
            dir.mkpath(".");
        }
        
        logFile = new QFile(filename, this);
        
        if (logFile->open(QIODevice::WriteOnly | QIODevice::Append)) {
            logStream = new QTextStream(logFile);
            logStream->setEncoding(QStringConverter::Utf8);
            qDebug() << "日志文件已设置:" << filename;
        } else {
            qCritical() << "无法打开日志文件:" << filename << "错误:" << logFile->errorString();
            delete logFile;
            logFile = nullptr;
        }
        
    } catch (const std::exception& e) {
        qCritical() << "设置日志文件时发生异常:" << e.what();
        // 清理资源
        if (logStream) {
            delete logStream;
            logStream = nullptr;
        }
        if (logFile) {
            delete logFile;
            logFile = nullptr;
        }
    } catch (...) {
        qCritical() << "设置日志文件时发生未知异常";
        // 清理资源
        if (logStream) {
            delete logStream;
            logStream = nullptr;
        }
        if (logFile) {
            delete logFile;
            logFile = nullptr;
        }
    }
}

void LogManager::setConsoleOutput(bool enabled)
{
    consoleOutputEnabled = enabled;
}

void LogManager::rotateLogFile()
{
    if (!logFile) return;
    
    QMutexLocker locker(&logMutex);
    
    QString baseName = QFileInfo(logFilename).baseName();
    QString suffix = QFileInfo(logFilename).suffix();
    QString path = QFileInfo(logFilename).path();
    
    // 移动现有文件
    for (int i = maxLogFiles - 1; i > 0; --i) {
        QString oldName = QString("%1/%2.%3.%4").arg(path, baseName).arg(i).arg(suffix);
        QString newName = QString("%1/%2.%3.%4").arg(path, baseName).arg(i + 1).arg(suffix);
        
        if (QFile::exists(oldName)) {
            QFile::remove(newName);
            QFile::rename(oldName, newName);
        }
    }
    
    // 移动当前文件
    QString backupName = QString("%1/%2.1.%3").arg(path, baseName, suffix);
    if (logStream) {
        logStream->flush();
        delete logStream;
        logStream = nullptr;
    }
    
    if (logFile) {
        logFile->close();
        delete logFile;
        logFile = nullptr;
    }
    
    QFile::rename(logFilename, backupName);
    
    // 重新打开新文件
    logFile = new QFile(logFilename, this);
    if (logFile->open(QIODevice::WriteOnly | QIODevice::Append)) {
        logStream = new QTextStream(logFile);
        logStream->setEncoding(QStringConverter::Utf8);
    }
    
    info("日志文件已轮转");
}

void LogManager::cleanupOldLogFiles()
{
    QString baseName = QFileInfo(logFilename).baseName();
    QString suffix = QFileInfo(logFilename).suffix();
    QString path = QFileInfo(logFilename).path();
    
    // 删除超过最大数量的日志文件
    for (int i = maxLogFiles + 1; i <= maxLogFiles + 10; ++i) {
        QString fileName = QString("%1/%2.%3.%4").arg(path, baseName).arg(i).arg(suffix);
        if (QFile::exists(fileName)) {
            QFile::remove(fileName);
        }
    }
}

QList<LogEntry> LogManager::getLogEntries(int count) const
{
    QMutexLocker locker(&logMutex);
    
    if (count >= logEntries.size()) {
        return logEntries;
    }
    
    return logEntries.mid(logEntries.size() - count);
}

bool LogManager::exportLogs(const QString& filename, const QDateTime& startTime, const QDateTime& endTime) const
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    
    QMutexLocker locker(&logMutex);
    
    for (const LogEntry& entry : logEntries) {
        bool includeEntry = true;
        
        if (startTime.isValid() && entry.timestamp < startTime) {
            includeEntry = false;
        }
        
        if (endTime.isValid() && entry.timestamp > endTime) {
            includeEntry = false;
        }
        
        if (includeEntry) {
            out << formatLogEntry(entry) << "\n";
        }
    }
    
    return true;
}

/**
 * @brief 处理日志队列
 * 
 * 批量处理日志队列中的条目，提高性能并确保线程安全
 */
void LogManager::processLogQueue()
{
    QList<LogEntry> entriesToProcess;
    
    // 快速获取待处理的日志条目，减少锁定时间
    {
        QMutexLocker locker(&logMutex);
        while (!logQueue.isEmpty()) {
            entriesToProcess.append(logQueue.dequeue());
        }
    }
    
    // 在锁外处理日志条目，避免长时间锁定
    for (const LogEntry& entry : entriesToProcess) {
        // writeToFile内部有自己的锁保护
        if (logStream) {
            writeToFile(entry);
        }
        
        if (consoleOutputEnabled) {
            writeToConsole(entry);
        }
    }
}

void LogManager::checkFileSize()
{
    if (!logFile) return;
    
    if (logFile->size() > maxFileSize) {
        rotateLogFile();
        cleanupOldLogFiles();
    }
}

/**
 * @brief 线程安全的文件写入方法
 * 
 * 使用互斥锁确保多线程环境下的文件写入安全
 * @param entry 日志条目
 */
void LogManager::writeToFile(const LogEntry& entry)
{
    // 使用QMutexLocker确保线程安全的文件写入
    QMutexLocker locker(&logMutex);
    
    if (!logStream || !logFile) {
        return;
    }
    
    try {
        // 检查文件是否仍然有效
        if (!logFile->isOpen()) {
            // 尝试重新打开文件
            if (!logFile->open(QIODevice::WriteOnly | QIODevice::Append)) {
                qCritical() << "无法重新打开日志文件:" << logFilename;
                return;
            }
            // 重新创建文本流
            if (logStream) {
                delete logStream;
            }
            logStream = new QTextStream(logFile);
            logStream->setEncoding(QStringConverter::Utf8);
        }
        
        *logStream << formatLogEntry(entry) << "\n";
        logStream->flush();
        
        // 确保数据写入磁盘
        if (logFile) {
            logFile->flush();
        }
        
    } catch (const std::exception& e) {
        qCritical() << "写入日志文件时发生异常:" << e.what();
    } catch (...) {
        qCritical() << "写入日志文件时发生未知异常";
    }
}

void LogManager::writeToConsole(const LogEntry& entry)
{
    QString formattedEntry = formatLogEntry(entry);
    
    if (entry.level >= LogLevel::Error) {
        std::cerr << formattedEntry.toStdString() << std::endl;
    } else {
        std::cout << formattedEntry.toStdString() << std::endl;
    }
}

QString LogManager::formatLogEntry(const LogEntry& entry) const
{
    QString levelStr = logLevelToString(entry.level);
    QString timestamp = entry.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz");
    
    QString formatted = QString("[%1] [%2] [%3] %4")
                        .arg(timestamp, levelStr, entry.category, entry.message);
    
    if (!entry.file.isEmpty() && entry.line > 0) {
        QString fileName = QFileInfo(entry.file).baseName();
        formatted += QString(" (%1:%2)").arg(fileName).arg(entry.line);
    }
    
    return formatted;
}

QString LogManager::logLevelToString(LogLevel level) const
{
    switch (level) {
        case LogLevel::Debug:    return "DEBUG";
        case LogLevel::Info:     return "INFO";
        case LogLevel::Warning:  return "WARN";
        case LogLevel::Error:    return "ERROR";
        case LogLevel::Critical: return "CRITICAL";
        default:                 return "UNKNOWN";
    }
}