#pragma once

#include "testframework.h"
#include "../src/data/datamodels.h"

class DataModelsTest : public TestBase
{
    Q_OBJECT

public:
    explicit DataModelsTest(QObject* parent = nullptr);

protected:
    void setupTestCase() override;
    void cleanupTestCase() override;
    void setupTest() override;
    void cleanupTest() override;

private:
    // ProductionRecord 测试
    void testProductionRecordCreation();
    void testProductionRecordValidation();
    void testProductionRecordSerialization();
    void testProductionRecordAccuracyCalculation();
    
    // BatchRecord 测试
    void testBatchRecordCreation();
    void testBatchRecordValidation();
    void testBatchRecordStatistics();
    
    // SensorData 测试
    void testSensorDataCreation();
    void testSensorDataValidation();
    void testSensorDataRangeChecking();

private:
    // 测试数据
    ProductionRecord* m_testProductionRecord;
    BatchRecord* m_testBatchRecord;
    SensorData* m_testSensorData;
};