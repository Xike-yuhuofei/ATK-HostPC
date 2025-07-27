#include <QCoreApplication>
#include <QDebug>
#include <QDir>

#include "testframework.h"
#include "test_datamodels.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    qDebug() << "========================================";
    qDebug() << "     ATK 工业点胶设备单元测试";
    qDebug() << "========================================";
    
    // 创建测试运行器
    TestRunner* runner = TestRunner::getInstance();
    
    // 注册测试套件
    DataModelsTest* dataModelsTest = new DataModelsTest();
    runner->registerTestSuite(dataModelsTest, "DataModelsTest");
    
    // 连接测试信号
    QObject::connect(runner, &TestRunner::testSuiteStarted, 
                     [](const QString& suiteName) {
        qDebug() << QString("开始运行测试套件: %1").arg(suiteName);
    });
    
    QObject::connect(runner, &TestRunner::testSuiteFinished, 
                     [](const QString& suiteName) {
        qDebug() << QString("测试套件完成: %1").arg(suiteName);
    });
    
    // 运行所有测试
    qDebug() << "开始运行所有测试用例...";
    bool allTestsPassed = runner->runAllTests();
    
    // 生成测试报告
    QString reportPath = "test_report.txt";
    bool reportSaved = runner->saveReport(reportPath);
    
    qDebug() << "========================================";
    qDebug() << "           测试执行完成";
    qDebug() << "========================================";
    qDebug() << QString("总体结果: %1").arg(allTestsPassed ? "通过" : "失败");
    
    if (reportSaved) {
        qDebug() << QString("测试报告已保存到: %1").arg(reportPath);
    }
    
    qDebug() << "测试详细报告:";
    qDebug() << runner->generateTextReport();
    
    // 清理资源
    delete dataModelsTest;
    
    return allTestsPassed ? 0 : 1;
}