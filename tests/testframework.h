#pragma once

#include <QObject>
#include <QTest>
#include <QSignalSpy>
#include <QTimer>
#include <QEventLoop>
#include <QDebug>
#include <QDateTime>
#include <QJsonObject>
#include <QElapsedTimer>
#include <QVariantMap>
#include <functional>

// 测试结果类型
enum class TestResult {
    Passed,
    Failed,
    Skipped,
    Error
};

// 测试用例信息
struct TestCase {
    QString name;
    QString description;
    TestResult result;
    QString errorMessage;
    qint64 executionTime;
    QDateTime timestamp;
    
    TestCase(const QString& n = QString(), const QString& desc = QString())
        : name(n), description(desc), result(TestResult::Skipped)
        , executionTime(0), timestamp(QDateTime::currentDateTime())
    {}
};

// 测试基础类
class TestBase : public QObject
{
    Q_OBJECT

public:
    explicit TestBase(QObject* parent = nullptr);
    virtual ~TestBase();
    
    // 测试运行接口
    bool runTest(const QString& testName = QString());
    QList<TestCase> getTestResults() const;
    
    // 测试工具方法
    void assertEqual(const QVariant& expected, const QVariant& actual, const QString& message = QString());
    void assertTrue(bool condition, const QString& message = QString());
    void assertFalse(bool condition, const QString& message = QString());
    
    // 异步测试工具
    bool waitForSignal(QObject* sender, const char* signal, int timeout = 5000);
    bool waitForCondition(std::function<bool()> condition, int timeout = 5000);
    
    // 数据生成
    QString generateRandomString(int length = 10);
    int generateRandomInt(int min = 0, int max = 100);
    QVariantMap generateTestData(const QStringList& fields);

protected:
    // 测试生命周期方法
    virtual void setupTestCase() {}
    virtual void cleanupTestCase() {}
    virtual void setupTest() {}
    virtual void cleanupTest() {}
    
    // 测试注册
    void registerTest(const QString& testName, std::function<void()> testFunc);
    void skipTest(const QString& reason = QString());

private:
    // 测试执行
    void runSingleTest(const QString& testName);
    void recordTestResult(const QString& testName, TestResult result, 
                         const QString& errorMessage = QString(), qint64 executionTime = 0);
    
    // 测试状态
    QMap<QString, std::function<void()>> m_testFunctions;
    QList<TestCase> m_testResults;
    QString m_currentTestName;
    bool m_testSkipped;
    QElapsedTimer m_testTimer;
};

// 测试运行器
class TestRunner : public QObject
{
    Q_OBJECT

public:
    static TestRunner* getInstance();
    
    // 测试套件管理
    void registerTestSuite(TestBase* testSuite, const QString& suiteName = QString());
    
    // 测试执行
    bool runAllTests();
    bool runTestSuite(const QString& suiteName);
    
    // 报告生成
    QString generateTextReport() const;
    bool saveReport(const QString& filePath) const;

signals:
    void testSuiteStarted(const QString& suiteName);
    void testSuiteFinished(const QString& suiteName);

private:
    explicit TestRunner(QObject* parent = nullptr);
    
    // 单例实例
    static TestRunner* instance;
    
    // 测试套件管理
    QMap<QString, TestBase*> m_testSuites;
};

// 测试注册宏
#define ASSERT_EQ(expected, actual) assertEqual(expected, actual, QString("Assert failed at %1:%2").arg(__FILE__).arg(__LINE__))
#define ASSERT_TRUE(condition) assertTrue(condition, QString("Assert failed at %1:%2").arg(__FILE__).arg(__LINE__))
#define ASSERT_FALSE(condition) assertFalse(condition, QString("Assert failed at %1:%2").arg(__FILE__).arg(__LINE__))