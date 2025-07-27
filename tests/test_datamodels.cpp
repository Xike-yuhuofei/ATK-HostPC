#include "test_datamodels.h"

DataModelsTest::DataModelsTest(QObject* parent)
    : TestBase(parent)
    , m_testProductionRecord(nullptr)
    , m_testBatchRecord(nullptr)
    , m_testSensorData(nullptr)
{
    // 注册测试用例
    registerTest("testProductionRecordCreation", [this]() { testProductionRecordCreation(); });
    registerTest("testProductionRecordValidation", [this]() { testProductionRecordValidation(); });
    registerTest("testProductionRecordSerialization", [this]() { testProductionRecordSerialization(); });
    registerTest("testProductionRecordAccuracyCalculation", [this]() { testProductionRecordAccuracyCalculation(); });
    
    registerTest("testBatchRecordCreation", [this]() { testBatchRecordCreation(); });
    registerTest("testBatchRecordValidation", [this]() { testBatchRecordValidation(); });
    registerTest("testBatchRecordStatistics", [this]() { testBatchRecordStatistics(); });
    
    registerTest("testSensorDataCreation", [this]() { testSensorDataCreation(); });
    registerTest("testSensorDataValidation", [this]() { testSensorDataValidation(); });
    registerTest("testSensorDataRangeChecking", [this]() { testSensorDataRangeChecking(); });
}

void DataModelsTest::setupTestCase()
{
    qDebug() << "Setting up DataModels test suite";
}

void DataModelsTest::cleanupTestCase()
{
    qDebug() << "Cleaning up DataModels test suite";
}

void DataModelsTest::setupTest()
{
    // 为每个测试创建新的测试数据
    m_testProductionRecord = new ProductionRecord();
    m_testBatchRecord = new BatchRecord();
    m_testSensorData = new SensorData();
}

void DataModelsTest::cleanupTest()
{
    // 清理测试数据
    delete m_testProductionRecord;
    delete m_testBatchRecord;
    delete m_testSensorData;
    
    m_testProductionRecord = nullptr;
    m_testBatchRecord = nullptr;
    m_testSensorData = nullptr;
}

void DataModelsTest::testProductionRecordCreation()
{
    // 测试默认构造函数
    ASSERT_TRUE(m_testProductionRecord != nullptr);
    ASSERT_EQ(m_testProductionRecord->recordId, 0);
    ASSERT_EQ(m_testProductionRecord->batchId, 0);
    ASSERT_EQ(m_testProductionRecord->status, 0);
    
    // 测试设置数据
    m_testProductionRecord->productName = "测试产品";
    m_testProductionRecord->operatorName = "张三";
    
    ASSERT_EQ(m_testProductionRecord->productName, "测试产品");
    ASSERT_EQ(m_testProductionRecord->operatorName, "张三");
}

void DataModelsTest::testProductionRecordValidation()
{
    // 测试无效数据
    ASSERT_FALSE(m_testProductionRecord->isValid());
    
    // 设置有效数据
    m_testProductionRecord->productName = "测试产品";
    m_testProductionRecord->productCode = "TEST001";
    m_testProductionRecord->operatorName = "张三";
    m_testProductionRecord->deviceName = "设备001";
    m_testProductionRecord->startTime = QDateTime::currentDateTime();
    
    // 测试有效数据
    ASSERT_TRUE(m_testProductionRecord->isValid());
}

void DataModelsTest::testProductionRecordSerialization()
{
    // 设置测试数据
    m_testProductionRecord->recordId = 123;
    m_testProductionRecord->productName = "测试产品";
    m_testProductionRecord->targetX = 100.0;
    m_testProductionRecord->actualX = 99.8;
    
    // 测试转换为Map
    QVariantMap map = m_testProductionRecord->toMap();
    ASSERT_EQ(map["record_id"].toInt(), 123);
    ASSERT_EQ(map["product_name"].toString(), "测试产品");
    
    // 测试从Map恢复
    ProductionRecord newRecord;
    newRecord.fromMap(map);
    ASSERT_EQ(newRecord.recordId, 123);
    ASSERT_EQ(newRecord.productName, "测试产品");
}

void DataModelsTest::testProductionRecordAccuracyCalculation()
{
    // 设置测试数据
    m_testProductionRecord->targetX = 100.0;
    m_testProductionRecord->actualX = 99.8;
    
    // 测试精度计算
    double accuracyX = m_testProductionRecord->getAccuracyX();
    ASSERT_EQ(accuracyX, 0.2);
    
    // 测试状态判断
    m_testProductionRecord->status = 1;
    ASSERT_TRUE(m_testProductionRecord->isCompleted());
}

void DataModelsTest::testBatchRecordCreation()
{
    // 测试默认构造函数
    ASSERT_TRUE(m_testBatchRecord != nullptr);
    ASSERT_EQ(m_testBatchRecord->batchId, 0);
    ASSERT_EQ(m_testBatchRecord->totalCount, 0);
    
    // 测试设置数据
    m_testBatchRecord->batchNumber = "BATCH001";
    m_testBatchRecord->totalCount = 100;
    
    ASSERT_EQ(m_testBatchRecord->batchNumber, "BATCH001");
    ASSERT_EQ(m_testBatchRecord->totalCount, 100);
}

void DataModelsTest::testBatchRecordValidation()
{
    // 测试无效数据
    ASSERT_FALSE(m_testBatchRecord->isValid());
    
    // 设置有效数据
    m_testBatchRecord->batchNumber = "BATCH001";
    m_testBatchRecord->productName = "测试产品";
    m_testBatchRecord->productCode = "TEST001";
    m_testBatchRecord->totalCount = 100;
    
    ASSERT_TRUE(m_testBatchRecord->isValid());
}

void DataModelsTest::testBatchRecordStatistics()
{
    // 设置测试数据
    m_testBatchRecord->totalCount = 100;
    m_testBatchRecord->completedCount = 80;
    
    // 测试完成率计算
    double completionRate = m_testBatchRecord->getCompletionRate();
    ASSERT_EQ(completionRate, 80.0);
    
    // 测试状态判断
    m_testBatchRecord->status = 1;
    ASSERT_TRUE(m_testBatchRecord->isActive());
}

void DataModelsTest::testSensorDataCreation()
{
    // 测试默认构造函数
    ASSERT_TRUE(m_testSensorData != nullptr);
    ASSERT_EQ(m_testSensorData->dataId, 0);
    ASSERT_EQ(m_testSensorData->value, 0.0);
    
    // 测试设置数据
    m_testSensorData->deviceName = "设备001";
    m_testSensorData->value = 25.5;
    
    ASSERT_EQ(m_testSensorData->deviceName, "设备001");
    ASSERT_EQ(m_testSensorData->value, 25.5);
}

void DataModelsTest::testSensorDataValidation()
{
    // 测试无效数据
    ASSERT_FALSE(m_testSensorData->isValid());
    
    // 设置有效数据
    m_testSensorData->deviceName = "设备001";
    m_testSensorData->sensorType = "温度传感器";
    m_testSensorData->parameterName = "温度";
    m_testSensorData->timestamp = QDateTime::currentDateTime();
    
    ASSERT_TRUE(m_testSensorData->isValid());
}

void DataModelsTest::testSensorDataRangeChecking()
{
    // 设置范围数据
    m_testSensorData->value = 25.0;
    m_testSensorData->minValue = 20.0;
    m_testSensorData->maxValue = 30.0;
    
    // 测试范围检查
    ASSERT_TRUE(m_testSensorData->isInRange());
    
    // 测试超出范围
    m_testSensorData->value = 35.0;
    ASSERT_FALSE(m_testSensorData->isInRange());
}