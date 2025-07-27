#include "checksum.h"
#include <QCryptographicHash>
#include <QDebug>
#include <cstring>

// 静态成员初始化
uint16_t EnhancedChecksum::crc16Table[256];
uint32_t EnhancedChecksum::crc32Table[256];
bool EnhancedChecksum::crcTablesInitialized = false;

// CRC16多项式定义
const uint16_t CRC16_IBM_POLY = 0x8005;      // x^16 + x^15 + x^2 + 1
const uint16_t CRC16_CCITT_POLY = 0x1021;    // x^16 + x^12 + x^5 + 1
const uint16_t CRC16_MODBUS_POLY = 0xA001;   // x^16 + x^15 + x^2 + 1 (reversed)

// CRC32多项式定义
const uint32_t CRC32_POLY = 0xEDB88320;      // IEEE 802.3 CRC32 (reversed)
const uint32_t CRC32C_POLY = 0x82F63B78;     // Castagnoli CRC32C (reversed)

ChecksumResult EnhancedChecksum::calculate(const QByteArray& data, ChecksumType type)
{
    if (!crcTablesInitialized) {
        initializeCRC16Table();
        initializeCRC32Table();
        crcTablesInitialized = true;
    }
    
    switch (type) {
        case ChecksumType::Simple: {
            uint8_t result = calculateSimple(data);
            return ChecksumResult(type, QByteArray(1, static_cast<char>(result)));
        }
        case ChecksumType::XOR: {
            uint8_t result = calculateXOR(data);
            return ChecksumResult(type, QByteArray(1, static_cast<char>(result)));
        }
        case ChecksumType::CRC8: {
            uint8_t result = calculateCRC8(data);
            return ChecksumResult(type, QByteArray(1, static_cast<char>(result)));
        }
        case ChecksumType::CRC16_IBM: {
            uint16_t result = calculateCRC16_IBM(data);
            QByteArray bytes = ChecksumUtils::uint16ToBytes(result);
            return ChecksumResult(type, bytes);
        }
        case ChecksumType::CRC16_CCITT: {
            uint16_t result = calculateCRC16_CCITT(data);
            QByteArray bytes = ChecksumUtils::uint16ToBytes(result);
            return ChecksumResult(type, bytes);
        }
        case ChecksumType::CRC16_MODBUS: {
            uint16_t result = calculateCRC16_Modbus(data);
            QByteArray bytes = ChecksumUtils::uint16ToBytes(result);
            return ChecksumResult(type, bytes);
        }
        case ChecksumType::CRC32: {
            uint32_t result = calculateCRC32(data);
            QByteArray bytes = ChecksumUtils::uint32ToBytes(result);
            return ChecksumResult(type, bytes);
        }
        case ChecksumType::CRC32C: {
            uint32_t result = calculateCRC32C(data);
            QByteArray bytes = ChecksumUtils::uint32ToBytes(result);
            return ChecksumResult(type, bytes);
        }
        case ChecksumType::MD5: {
            QByteArray result = calculateMD5(data);
            return ChecksumResult(type, result);
        }
        case ChecksumType::SHA1: {
            QByteArray result = calculateSHA1(data);
            return ChecksumResult(type, result);
        }
        case ChecksumType::SHA256: {
            QByteArray result = calculateSHA256(data);
            return ChecksumResult(type, result);
        }
        default:
            return ChecksumResult(); // 无效结果
    }
}

bool EnhancedChecksum::verify(const QByteArray& data, const ChecksumResult& expectedChecksum)
{
    if (!expectedChecksum.isValid) {
        return false;
    }
    
    ChecksumResult calculated = calculate(data, expectedChecksum.type);
    return calculated == expectedChecksum;
}

bool EnhancedChecksum::verify(const QByteArray& data, ChecksumType type, const QByteArray& expectedValue)
{
    ChecksumResult expected(type, expectedValue);
    return verify(data, expected);
}

uint8_t EnhancedChecksum::calculateSimple(const QByteArray& data)
{
    uint8_t checksum = 0;
    for (char byte : data) {
        checksum += static_cast<uint8_t>(byte);
    }
    return checksum;
}

uint8_t EnhancedChecksum::calculateXOR(const QByteArray& data)
{
    uint8_t checksum = 0;
    for (char byte : data) {
        checksum ^= static_cast<uint8_t>(byte);
    }
    return checksum;
}

uint8_t EnhancedChecksum::calculateCRC8(const QByteArray& data, uint8_t polynomial, uint8_t init)
{
    uint8_t crc = init;
    
    for (char byte : data) {
        crc ^= static_cast<uint8_t>(byte);
        
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ polynomial;
            } else {
                crc <<= 1;
            }
        }
    }
    
    return crc;
}

uint16_t EnhancedChecksum::calculateCRC16_IBM(const QByteArray& data)
{
    uint16_t crc = 0x0000;
    
    for (char byte : data) {
        crc ^= static_cast<uint8_t>(byte);
        
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ CRC16_IBM_POLY;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return crc;
}

uint16_t EnhancedChecksum::calculateCRC16_CCITT(const QByteArray& data)
{
    uint16_t crc = 0xFFFF;
    
    for (char byte : data) {
        crc ^= (static_cast<uint16_t>(byte) << 8);
        
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ CRC16_CCITT_POLY;
            } else {
                crc <<= 1;
            }
        }
    }
    
    return crc;
}

uint16_t EnhancedChecksum::calculateCRC16_Modbus(const QByteArray& data)
{
    uint16_t crc = 0xFFFF;
    
    for (char byte : data) {
        crc ^= static_cast<uint8_t>(byte);
        
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ CRC16_MODBUS_POLY;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return crc;
}

uint32_t EnhancedChecksum::calculateCRC32(const QByteArray& data)
{
    uint32_t crc = 0xFFFFFFFF;
    
    for (char byte : data) {
        crc = crc32Table[(crc ^ static_cast<uint8_t>(byte)) & 0xFF] ^ (crc >> 8);
    }
    
    return crc ^ 0xFFFFFFFF;
}

uint32_t EnhancedChecksum::calculateCRC32C(const QByteArray& data)
{
    uint32_t crc = 0xFFFFFFFF;
    
    for (char byte : data) {
        crc ^= static_cast<uint8_t>(byte);
        
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x00000001) {
                crc = (crc >> 1) ^ CRC32C_POLY;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return crc ^ 0xFFFFFFFF;
}

QByteArray EnhancedChecksum::calculateMD5(const QByteArray& data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Md5);
}

QByteArray EnhancedChecksum::calculateSHA1(const QByteArray& data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha1);
}

QByteArray EnhancedChecksum::calculateSHA256(const QByteArray& data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256);
}

bool EnhancedChecksum::isChecksumTypeSupported(ChecksumType type)
{
    switch (type) {
        case ChecksumType::Simple:
        case ChecksumType::XOR:
        case ChecksumType::CRC8:
        case ChecksumType::CRC16_IBM:
        case ChecksumType::CRC16_CCITT:
        case ChecksumType::CRC16_MODBUS:
        case ChecksumType::CRC32:
        case ChecksumType::CRC32C:
        case ChecksumType::MD5:
        case ChecksumType::SHA1:
        case ChecksumType::SHA256:
            return true;
        default:
            return false;
    }
}

int EnhancedChecksum::getChecksumLength(ChecksumType type)
{
    switch (type) {
        case ChecksumType::Simple:
        case ChecksumType::XOR:
        case ChecksumType::CRC8:
            return 1;
        case ChecksumType::CRC16_IBM:
        case ChecksumType::CRC16_CCITT:
        case ChecksumType::CRC16_MODBUS:
            return 2;
        case ChecksumType::CRC32:
        case ChecksumType::CRC32C:
            return 4;
        case ChecksumType::MD5:
            return 16;
        case ChecksumType::SHA1:
            return 20;
        case ChecksumType::SHA256:
            return 32;
        default:
            return 0;
    }
}

QString EnhancedChecksum::checksumTypeToString(ChecksumType type)
{
    switch (type) {
        case ChecksumType::Simple: return "Simple";
        case ChecksumType::XOR: return "XOR";
        case ChecksumType::CRC8: return "CRC8";
        case ChecksumType::CRC16_IBM: return "CRC16_IBM";
        case ChecksumType::CRC16_CCITT: return "CRC16_CCITT";
        case ChecksumType::CRC16_MODBUS: return "CRC16_MODBUS";
        case ChecksumType::CRC32: return "CRC32";
        case ChecksumType::CRC32C: return "CRC32C";
        case ChecksumType::MD5: return "MD5";
        case ChecksumType::SHA1: return "SHA1";
        case ChecksumType::SHA256: return "SHA256";
        default: return "Unknown";
    }
}

ChecksumType EnhancedChecksum::stringToChecksumType(const QString& typeString)
{
    QString upper = typeString.toUpper();
    
    if (upper == "SIMPLE") return ChecksumType::Simple;
    if (upper == "XOR") return ChecksumType::XOR;
    if (upper == "CRC8") return ChecksumType::CRC8;
    if (upper == "CRC16_IBM") return ChecksumType::CRC16_IBM;
    if (upper == "CRC16_CCITT") return ChecksumType::CRC16_CCITT;
    if (upper == "CRC16_MODBUS") return ChecksumType::CRC16_MODBUS;
    if (upper == "CRC32") return ChecksumType::CRC32;
    if (upper == "CRC32C") return ChecksumType::CRC32C;
    if (upper == "MD5") return ChecksumType::MD5;
    if (upper == "SHA1") return ChecksumType::SHA1;
    if (upper == "SHA256") return ChecksumType::SHA256;
    
    return ChecksumType::Simple; // 默认值
}

EnhancedChecksum::MultiLevelChecksum EnhancedChecksum::generateMultiLevel(
    const QByteArray& data, 
    ChecksumType primary, 
    ChecksumType secondary, 
    ChecksumType tertiary)
{
    MultiLevelChecksum result;
    
    result.primary = calculate(data, primary);
    result.secondary = calculate(data, secondary);
    result.tertiary = calculate(data, tertiary);
    
    result.isValid = result.primary.isValid && 
                     result.secondary.isValid && 
                     result.tertiary.isValid;
    
    return result;
}

bool EnhancedChecksum::verifyMultiLevel(const QByteArray& data, const MultiLevelChecksum& expected)
{
    if (!expected.isValid) {
        return false;
    }
    
    MultiLevelChecksum calculated = generateMultiLevel(
        data, 
        expected.primary.type,
        expected.secondary.type, 
        expected.tertiary.type
    );
    
    return calculated.primary == expected.primary &&
           calculated.secondary == expected.secondary &&
           calculated.tertiary == expected.tertiary;
}

EnhancedChecksum::ErrorDetectionResult EnhancedChecksum::detectErrors(
    const QByteArray& data, 
    const ChecksumResult& expectedChecksum)
{
    ErrorDetectionResult result;
    
    ChecksumResult calculated = calculate(data, expectedChecksum.type);
    
    if (calculated == expectedChecksum) {
        // 校验正确，无错误
        result.hasError = false;
        result.errorCount = 0;
        result.canCorrect = false;
        result.correctedData = data;
        result.errorDescription = "数据完整";
        return result;
    }
    
    result.hasError = true;
    result.errorDescription = "校验失败";
    
    // 尝试单比特错误检测和纠正
    if (expectedChecksum.type == ChecksumType::CRC16_MODBUS || 
        expectedChecksum.type == ChecksumType::CRC32) {
        
        // 遍历每个比特位，尝试翻转
        for (int byteIndex = 0; byteIndex < data.size(); ++byteIndex) {
            for (int bitIndex = 0; bitIndex < 8; ++bitIndex) {
                QByteArray testData = data;
                testData[byteIndex] ^= (1 << bitIndex);
                
                ChecksumResult testChecksum = calculate(testData, expectedChecksum.type);
                if (testChecksum == expectedChecksum) {
                    // 找到错误位置
                    result.errorPosition = byteIndex * 8 + bitIndex;
                    result.errorCount = 1;
                    result.canCorrect = true;
                    result.correctedData = testData;
                    result.errorDescription = QString("检测到第%1字节第%2位的单比特错误")
                                            .arg(byteIndex)
                                            .arg(bitIndex);
                    return result;
                }
            }
        }
    }
    
    // 无法纠正错误
    result.errorCount = -1; // 未知错误数量
    result.canCorrect = false;
    result.correctedData = data;
    result.errorDescription = "无法纠正的错误";
    
    return result;
}

EnhancedChecksum::ErrorDetectionResult EnhancedChecksum::hammingCheck(const QByteArray& data)
{
    ErrorDetectionResult result;
    
    // 简化的汉明码实现（仅用于演示）
    if (data.size() < 2) {
        result.hasError = false;
        result.errorDescription = "数据太短，无法进行汉明码检查";
        return result;
    }
    
    // 计算奇偶校验位
    uint8_t p1 = 0, p2 = 0, p4 = 0;
    
    for (int i = 0; i < data.size(); ++i) {
        uint8_t byte = static_cast<uint8_t>(data[i]);
        
        // 计算各奇偶校验位
        for (int bit = 0; bit < 8; ++bit) {
            if (byte & (1 << bit)) {
                int position = i * 8 + bit + 1; // 位置从1开始
                
                if (position & 1) p1 ^= 1;
                if (position & 2) p2 ^= 1;
                if (position & 4) p4 ^= 1;
            }
        }
    }
    
    int syndrome = p1 | (p2 << 1) | (p4 << 2);
    
    if (syndrome == 0) {
        result.hasError = false;
        result.errorDescription = "汉明码检查通过";
    } else {
        result.hasError = true;
        result.errorPosition = syndrome - 1;
        result.errorCount = 1;
        result.canCorrect = true;
        result.errorDescription = QString("汉明码检测到第%1位错误").arg(syndrome);
        
        // 纠正错误
        result.correctedData = data;
        if (syndrome <= data.size() * 8) {
            int byteIndex = (syndrome - 1) / 8;
            int bitIndex = (syndrome - 1) % 8;
            
            if (byteIndex < result.correctedData.size()) {
                result.correctedData[byteIndex] ^= (1 << bitIndex);
            }
        }
    }
    
    return result;
}

EnhancedChecksum::FrameIntegrityResult EnhancedChecksum::checkFrameIntegrity(
    const QByteArray& frameData,
    uint16_t expectedHeader,
    uint8_t expectedTail,
    ChecksumType checksumType)
{
    FrameIntegrityResult result;
    
    if (frameData.size() < 6) {
        result.issues = "帧数据太短";
        return result;
    }
    
    int confidence = 0;
    
    // 检查帧头
    if (frameData.size() >= 2) {
        uint16_t header = ChecksumUtils::bytesToUInt16(frameData.left(2));
        if (header == expectedHeader) {
            result.hasValidHeader = true;
            confidence += 20;
        } else {
            result.issues += "帧头错误; ";
        }
    }
    
    // 检查帧尾
    uint8_t tail = static_cast<uint8_t>(frameData[frameData.size() - 1]);
    if (tail == expectedTail) {
        result.hasValidTail = true;
        confidence += 20;
    } else {
        result.issues += "帧尾错误; ";
    }
    
    // 检查长度字段一致性
    if (frameData.size() >= 4) {
        uint8_t declaredLength = static_cast<uint8_t>(frameData[3]);
        int expectedFrameSize = 6 + declaredLength; // 头(2) + 命令(1) + 长度(1) + 数据(n) + 校验(1) + 尾(1)
        
        if (frameData.size() == expectedFrameSize) {
            result.hasValidLength = true;
            confidence += 20;
        } else {
            result.issues += QString("长度不匹配(期望%1,实际%2); ")
                           .arg(expectedFrameSize)
                           .arg(frameData.size());
        }
    }
    
    // 检查校验和
    if (frameData.size() >= 6) {
        int checksumPos = frameData.size() - 2; // 校验位在倒数第二个位置
        QByteArray dataToCheck = frameData.mid(2, checksumPos - 2); // 从命令开始到数据结束
        
        ChecksumResult calculated = calculate(dataToCheck, checksumType);
        uint8_t expectedChecksum = static_cast<uint8_t>(frameData[checksumPos]);
        
        if (calculated.isValid && calculated.asUInt8() == expectedChecksum) {
            result.hasValidChecksum = true;
            confidence += 30;
        } else {
            result.issues += QString("校验错误(期望0x%1,计算0x%2); ")
                           .arg(expectedChecksum, 2, 16, QChar('0'))
                           .arg(calculated.asUInt8(), 2, 16, QChar('0'));
        }
    }
    
    // 序列检查（简化实现）
    result.hasValidSequence = true; // 暂时默认为有效
    confidence += 10;
    
    result.confidence = confidence;
    result.isComplete = (confidence >= 80);
    
    if (result.issues.isEmpty()) {
        result.issues = "帧完整性检查通过";
    }
    
    return result;
}

// CRC表初始化
void EnhancedChecksum::initializeCRC16Table()
{
    // 初始化CRC16表（这里使用Modbus多项式）
    for (int i = 0; i < 256; ++i) {
        uint16_t crc = i;
        
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ CRC16_MODBUS_POLY;
            } else {
                crc >>= 1;
            }
        }
        
        crc16Table[i] = crc;
    }
}

void EnhancedChecksum::initializeCRC32Table()
{
    for (int i = 0; i < 256; ++i) {
        uint32_t crc = i;
        
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x00000001) {
                crc = (crc >> 1) ^ CRC32_POLY;
            } else {
                crc >>= 1;
            }
        }
        
        crc32Table[i] = crc;
    }
}

// 工具函数实现
namespace ChecksumUtils {

QByteArray uint8ToBytes(uint8_t value)
{
    return QByteArray(1, static_cast<char>(value));
}

QByteArray uint16ToBytes(uint16_t value, bool bigEndian)
{
    QByteArray result(2, 0);
    
    if (bigEndian) {
        result[0] = static_cast<char>((value >> 8) & 0xFF);
        result[1] = static_cast<char>(value & 0xFF);
    } else {
        result[0] = static_cast<char>(value & 0xFF);
        result[1] = static_cast<char>((value >> 8) & 0xFF);
    }
    
    return result;
}

QByteArray uint32ToBytes(uint32_t value, bool bigEndian)
{
    QByteArray result(4, 0);
    
    if (bigEndian) {
        result[0] = static_cast<char>((value >> 24) & 0xFF);
        result[1] = static_cast<char>((value >> 16) & 0xFF);
        result[2] = static_cast<char>((value >> 8) & 0xFF);
        result[3] = static_cast<char>(value & 0xFF);
    } else {
        result[0] = static_cast<char>(value & 0xFF);
        result[1] = static_cast<char>((value >> 8) & 0xFF);
        result[2] = static_cast<char>((value >> 16) & 0xFF);
        result[3] = static_cast<char>((value >> 24) & 0xFF);
    }
    
    return result;
}

uint8_t bytesToUInt8(const QByteArray& bytes)
{
    if (bytes.size() >= 1) {
        return static_cast<uint8_t>(bytes[0]);
    }
    return 0;
}

uint16_t bytesToUInt16(const QByteArray& bytes, bool bigEndian)
{
    if (bytes.size() >= 2) {
        if (bigEndian) {
            return (static_cast<uint16_t>(bytes[0]) << 8) | static_cast<uint16_t>(bytes[1]);
        } else {
            return static_cast<uint16_t>(bytes[0]) | (static_cast<uint16_t>(bytes[1]) << 8);
        }
    }
    return 0;
}

uint32_t bytesToUInt32(const QByteArray& bytes, bool bigEndian)
{
    if (bytes.size() >= 4) {
        if (bigEndian) {
            return (static_cast<uint32_t>(bytes[0]) << 24) |
                   (static_cast<uint32_t>(bytes[1]) << 16) |
                   (static_cast<uint32_t>(bytes[2]) << 8) |
                   static_cast<uint32_t>(bytes[3]);
        } else {
            return static_cast<uint32_t>(bytes[0]) |
                   (static_cast<uint32_t>(bytes[1]) << 8) |
                   (static_cast<uint32_t>(bytes[2]) << 16) |
                   (static_cast<uint32_t>(bytes[3]) << 24);
        }
    }
    return 0;
}

QString bytesToHexString(const QByteArray& bytes, bool upperCase)
{
    return bytes.toHex(upperCase ? 0 : 1);
}

QByteArray hexStringToBytes(const QString& hexString)
{
    return QByteArray::fromHex(hexString.toUtf8());
}

QString formatChecksum(const ChecksumResult& checksum)
{
    if (!checksum.isValid) {
        return "Invalid";
    }
    
    QString typeStr = EnhancedChecksum::checksumTypeToString(checksum.type);
    QString valueStr = bytesToHexString(checksum.value, true);
    
    return QString("%1: 0x%2").arg(typeStr, valueStr);
}

} // namespace ChecksumUtils 