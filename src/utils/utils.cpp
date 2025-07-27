#include "utils.h"
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QSysInfo>
#include <QRegularExpression>
#include <QRandomGenerator>
#include <QLocale>
#include <cmath>

QString Utils::bytesToHexString(const QByteArray& data, const QString& separator)
{
    QString result;
    for (int i = 0; i < data.size(); ++i) {
        if (i > 0) {
            result += separator;
        }
        result += QString("%1").arg(static_cast<quint8>(data[i]), 2, 16, QChar('0')).toUpper();
    }
    return result;
}

QByteArray Utils::hexStringToBytes(const QString& hexString)
{
    QByteArray result;
    QString cleanHex = hexString;
    cleanHex.remove(QRegularExpression("[^0-9A-Fa-f]"));
    
    if (cleanHex.length() % 2 != 0) {
        cleanHex.prepend("0");
    }
    
    for (int i = 0; i < cleanHex.length(); i += 2) {
        QString byteString = cleanHex.mid(i, 2);
        bool ok;
        quint8 byte = static_cast<quint8>(byteString.toInt(&ok, 16));
        if (ok) {
            result.append(static_cast<char>(byte));
        }
    }
    
    return result;
}

QString Utils::formatFileSize(qint64 size)
{
    const QStringList units = {"B", "KB", "MB", "GB", "TB"};
    double fileSize = size;
    int unitIndex = 0;
    
    while (fileSize >= 1024.0 && unitIndex < units.size() - 1) {
        fileSize /= 1024.0;
        ++unitIndex;
    }
    
    return QString("%1 %2").arg(fileSize, 0, 'f', 2).arg(units[unitIndex]);
}

QString Utils::formatDuration(qint64 milliseconds)
{
    qint64 seconds = milliseconds / 1000;
    qint64 minutes = seconds / 60;
    qint64 hours = minutes / 60;
    qint64 days = hours / 24;
    
    if (days > 0) {
        return QString("%1天 %2小时 %3分钟").arg(days).arg(hours % 24).arg(minutes % 60);
    } else if (hours > 0) {
        return QString("%1小时 %2分钟").arg(hours).arg(minutes % 60);
    } else if (minutes > 0) {
        return QString("%1分钟 %2秒").arg(minutes).arg(seconds % 60);
    } else {
        return QString("%1秒").arg(seconds);
    }
}

QString Utils::formatDateTime(const QDateTime& dateTime, const QString& format)
{
    return dateTime.toString(format);
}

QString Utils::removeWhitespace(const QString& str)
{
    return str.simplified().remove(' ');
}

QStringList Utils::splitString(const QString& str, const QString& separator, bool removeEmpty)
{
    QStringList result = str.split(separator);
    if (removeEmpty) {
        result.removeAll("");
    }
    return result;
}

QString Utils::joinStrings(const QStringList& strings, const QString& separator)
{
    return strings.join(separator);
}

bool Utils::isNumeric(const QString& str)
{
    QRegularExpression regex("^[+-]?([0-9]*[.])?[0-9]+$");
    return regex.match(str).hasMatch();
}

double Utils::stringToDouble(const QString& str, bool* ok)
{
    QLocale locale;
    return locale.toDouble(str, ok);
}

int Utils::stringToInt(const QString& str, bool* ok)
{
    return str.toInt(ok);
}

QString Utils::doubleToString(double value, int precision)
{
    return QString::number(value, 'f', precision);
}

QString Utils::getApplicationPath()
{
    return QCoreApplication::applicationDirPath();
}

QString Utils::getConfigPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
}

QString Utils::getLogPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs";
}

QString Utils::getDataPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/data";
}

bool Utils::ensureDirectoryExists(const QString& path)
{
    QDir dir;
    return dir.mkpath(path);
}

quint8 Utils::calculateChecksum(const QByteArray& data)
{
    quint8 checksum = 0;
    for (char byte : data) {
        checksum += static_cast<quint8>(byte);
    }
    return checksum;
}

quint16 Utils::calculateCRC16(const QByteArray& data)
{
    quint16 crc = 0xFFFF;
    const quint16 polynomial = 0xA001;
    
    for (char byte : data) {
        crc ^= static_cast<quint8>(byte);
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ polynomial;
            } else {
                crc = crc >> 1;
            }
        }
    }
    
    return crc;
}

int Utils::randomInt(int min, int max)
{
    return QRandomGenerator::global()->bounded(min, max + 1);
}

double Utils::randomDouble(double min, double max)
{
    return min + (max - min) * QRandomGenerator::global()->generateDouble();
}

QString Utils::getSystemInfo()
{
    QString info;
    info += QString("操作系统: %1\n").arg(QSysInfo::prettyProductName());
    info += QString("内核类型: %1\n").arg(QSysInfo::kernelType());
    info += QString("内核版本: %1\n").arg(QSysInfo::kernelVersion());
    info += QString("CPU架构: %1\n").arg(QSysInfo::currentCpuArchitecture());
    info += QString("机器名称: %1\n").arg(QSysInfo::machineHostName());
    return info;
}

QString Utils::getApplicationInfo()
{
    QString info;
    info += QString("应用程序名称: %1\n").arg(QCoreApplication::applicationName());
    info += QString("应用程序版本: %1\n").arg(QCoreApplication::applicationVersion());
    info += QString("组织名称: %1\n").arg(QCoreApplication::organizationName());
    info += QString("组织域名: %1\n").arg(QCoreApplication::organizationDomain());
    info += QString("Qt版本: %1\n").arg(qVersion());
    return info;
} 