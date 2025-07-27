#pragma once

#include <QString>
#include <QByteArray>
#include <QDateTime>
#include <QStringList>

class Utils
{
public:
    // 字节数组转换
    static QString bytesToHexString(const QByteArray& data, const QString& separator = " ");
    static QByteArray hexStringToBytes(const QString& hexString);
    
    // 数据类型转换
    static QString formatFileSize(qint64 size);
    static QString formatDuration(qint64 milliseconds);
    static QString formatDateTime(const QDateTime& dateTime, const QString& format = "yyyy-MM-dd hh:mm:ss");
    
    // 字符串处理
    static QString removeWhitespace(const QString& str);
    static QStringList splitString(const QString& str, const QString& separator, bool removeEmpty = true);
    static QString joinStrings(const QStringList& strings, const QString& separator);
    
    // 数值处理
    static bool isNumeric(const QString& str);
    static double stringToDouble(const QString& str, bool* ok = nullptr);
    static int stringToInt(const QString& str, bool* ok = nullptr);
    static QString doubleToString(double value, int precision = 2);
    
    // 文件和路径处理
    static QString getApplicationPath();
    static QString getConfigPath();
    static QString getLogPath();
    static QString getDataPath();
    static bool ensureDirectoryExists(const QString& path);
    
    // 校验和计算
    static quint8 calculateChecksum(const QByteArray& data);
    static quint16 calculateCRC16(const QByteArray& data);
    
    // 随机数生成
    static int randomInt(int min, int max);
    static double randomDouble(double min, double max);
    
    // 系统信息
    static QString getSystemInfo();
    static QString getApplicationInfo();
    
private:
    Utils() = default;
}; 