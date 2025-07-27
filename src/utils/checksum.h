#pragma once

#include <QByteArray>
#include <QString>
#include <cstdint>

// 校验算法类型枚举
enum class ChecksumType {
    Simple,        // 简单累加校验
    XOR,          // 异或校验
    CRC8,         // CRC8校验
    CRC16_IBM,    // CRC16 (IBM/ANSI)
    CRC16_CCITT,  // CRC16 (CCITT)
    CRC16_MODBUS, // CRC16 (Modbus)
    CRC32,        // CRC32校验
    CRC32C,       // CRC32C (Castagnoli)
    MD5,          // MD5哈希
    SHA1,         // SHA1哈希
    SHA256        // SHA256哈希
};

// 校验结果结构
struct ChecksumResult {
    ChecksumType type;      // 校验类型
    QByteArray value;       // 校验值
    int length;             // 校验值长度
    bool isValid;           // 是否有效
    
    ChecksumResult() 
        : type(ChecksumType::Simple)
        , length(0)
        , isValid(false)
    {}
    
    ChecksumResult(ChecksumType t, const QByteArray& v) 
        : type(t)
        , value(v)
        , length(v.size())
        , isValid(true)
    {}
    
    // 获取数值形式的校验值
    uint8_t asUInt8() const {
        return (length >= 1) ? static_cast<uint8_t>(value[0]) : 0;
    }
    
    uint16_t asUInt16() const {
        if (length >= 2) {
            return (static_cast<uint16_t>(value[0]) << 8) | static_cast<uint16_t>(value[1]);
        }
        return 0;
    }
    
    uint32_t asUInt32() const {
        if (length >= 4) {
            return (static_cast<uint32_t>(value[0]) << 24) |
                   (static_cast<uint32_t>(value[1]) << 16) |
                   (static_cast<uint32_t>(value[2]) << 8) |
                   static_cast<uint32_t>(value[3]);
        }
        return 0;
    }
    
    // 比较操作符
    bool operator==(const ChecksumResult& other) const {
        return type == other.type && value == other.value;
    }
    
    bool operator!=(const ChecksumResult& other) const {
        return !(*this == other);
    }
};

// 增强校验算法类
class EnhancedChecksum
{
public:
    // 静态方法 - 计算各种校验值
    static ChecksumResult calculate(const QByteArray& data, ChecksumType type);
    
    // 验证校验值
    static bool verify(const QByteArray& data, const ChecksumResult& expectedChecksum);
    static bool verify(const QByteArray& data, ChecksumType type, const QByteArray& expectedValue);
    
    // 简单校验算法
    static uint8_t calculateSimple(const QByteArray& data);
    static uint8_t calculateXOR(const QByteArray& data);
    
    // CRC校验算法
    static uint8_t calculateCRC8(const QByteArray& data, uint8_t polynomial = 0x07, uint8_t init = 0x00);
    static uint16_t calculateCRC16_IBM(const QByteArray& data);
    static uint16_t calculateCRC16_CCITT(const QByteArray& data);
    static uint16_t calculateCRC16_Modbus(const QByteArray& data);
    static uint32_t calculateCRC32(const QByteArray& data);
    static uint32_t calculateCRC32C(const QByteArray& data);
    
    // 哈希算法
    static QByteArray calculateMD5(const QByteArray& data);
    static QByteArray calculateSHA1(const QByteArray& data);
    static QByteArray calculateSHA256(const QByteArray& data);
    
    // 高级功能
    static bool isChecksumTypeSupported(ChecksumType type);
    static int getChecksumLength(ChecksumType type);
    static QString checksumTypeToString(ChecksumType type);
    static ChecksumType stringToChecksumType(const QString& typeString);
    
    // 多级校验
    struct MultiLevelChecksum {
        ChecksumResult primary;     // 主校验
        ChecksumResult secondary;   // 次校验
        ChecksumResult tertiary;    // 三级校验
        bool isValid;              // 整体有效性
        
        MultiLevelChecksum() : isValid(false) {}
    };
    
    // 生成多级校验
    static MultiLevelChecksum generateMultiLevel(const QByteArray& data, 
                                                 ChecksumType primary = ChecksumType::CRC16_MODBUS,
                                                 ChecksumType secondary = ChecksumType::CRC8,
                                                 ChecksumType tertiary = ChecksumType::XOR);
    
    // 验证多级校验
    static bool verifyMultiLevel(const QByteArray& data, const MultiLevelChecksum& expected);
    
    // 错误检测和纠正
    struct ErrorDetectionResult {
        bool hasError;              // 是否有错误
        int errorPosition;          // 错误位置（-1表示无法定位）
        int errorCount;             // 错误数量
        QByteArray correctedData;   // 纠正后的数据
        bool canCorrect;            // 是否可以纠正
        QString errorDescription;   // 错误描述
        
        ErrorDetectionResult() 
            : hasError(false)
            , errorPosition(-1)
            , errorCount(0)
            , canCorrect(false)
        {}
    };
    
    // 高级错误检测
    static ErrorDetectionResult detectErrors(const QByteArray& data, 
                                            const ChecksumResult& expectedChecksum);
    
    // 汉明码错误检测和纠正
    static ErrorDetectionResult hammingCheck(const QByteArray& data);
    
    // 帧完整性检查
    struct FrameIntegrityResult {
        bool isComplete;            // 是否完整
        bool hasValidHeader;        // 头部有效
        bool hasValidTail;          // 尾部有效
        bool hasValidLength;        // 长度有效
        bool hasValidChecksum;      // 校验有效
        bool hasValidSequence;      // 序列有效
        int confidence;             // 置信度 (0-100)
        QString issues;             // 问题描述
        
        FrameIntegrityResult() 
            : isComplete(false)
            , hasValidHeader(false)
            , hasValidTail(false)
            , hasValidLength(false)
            , hasValidChecksum(false)
            , hasValidSequence(false)
            , confidence(0)
        {}
    };
    
    // 综合帧完整性检查
    static FrameIntegrityResult checkFrameIntegrity(const QByteArray& frameData,
                                                    uint16_t expectedHeader = 0xAA55,
                                                    uint8_t expectedTail = 0x0D,
                                                    ChecksumType checksumType = ChecksumType::CRC16_MODBUS);

private:
    // CRC查找表
    static void initializeCRC16Table();
    static void initializeCRC32Table();
    static uint16_t crc16Table[256];
    static uint32_t crc32Table[256];
    static bool crcTablesInitialized;
    
    // 内部辅助方法
    static uint16_t updateCRC16(uint16_t crc, uint8_t data, const uint16_t* table);
    static uint32_t updateCRC32(uint32_t crc, uint8_t data);
    static uint8_t reverseBits8(uint8_t value);
    static uint16_t reverseBits16(uint16_t value);
    static uint32_t reverseBits32(uint32_t value);
    
    // 禁用实例化
    EnhancedChecksum() = delete;
    ~EnhancedChecksum() = delete;
    EnhancedChecksum(const EnhancedChecksum&) = delete;
    EnhancedChecksum& operator=(const EnhancedChecksum&) = delete;
};

// 便利函数
namespace ChecksumUtils {
    // 快速CRC16计算
    inline uint16_t quickCRC16(const QByteArray& data) {
        return EnhancedChecksum::calculateCRC16_Modbus(data);
    }
    
    // 快速CRC32计算
    inline uint32_t quickCRC32(const QByteArray& data) {
        return EnhancedChecksum::calculateCRC32(data);
    }
    
    // 将数值转换为字节数组
    QByteArray uint8ToBytes(uint8_t value);
    QByteArray uint16ToBytes(uint16_t value, bool bigEndian = true);
    QByteArray uint32ToBytes(uint32_t value, bool bigEndian = true);
    
    // 从字节数组读取数值
    uint8_t bytesToUInt8(const QByteArray& bytes);
    uint16_t bytesToUInt16(const QByteArray& bytes, bool bigEndian = true);
    uint32_t bytesToUInt32(const QByteArray& bytes, bool bigEndian = true);
    
    // 十六进制字符串转换
    QString bytesToHexString(const QByteArray& bytes, bool upperCase = true);
    QByteArray hexStringToBytes(const QString& hexString);
    
    // 校验值格式化
    QString formatChecksum(const ChecksumResult& checksum);
} 