#include "testframework.h"
#include <QRandomGenerator>
#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <exception>

// TestBase 实现
TestBase::TestBase(QObject* parent)
    : QObject(parent)
    , m_testSkipped(false)
{
}

TestBase::~TestBase()
{
}

bool TestBase::runTest(const QString& testName)
{
    if (testName.isEmpty()) {
        // 运行所有测试
        setupTestCase();
        
        bool allPassed = true;
        for (auto it = m_testFunctions.begin(); it != m_testFunctions.end(); ++it) {
            runSingleTest(it.key());
            TestCase lastResult = m_testResults.last();
            if (lastResult.result == TestResult::Failed || lastResult.result == TestResult::Error) {
                allPassed = false;
            }
        }
        
        cleanupTestCase();
        return allPassed;
    } else {
        // 运行指定测试
        if (m_testFunctions.contains(testName)) {
            setupTestCase();
            runSingleTest(testName);
            cleanupTestCase();
            
            TestCase lastResult = m_testResults.last();
            return lastResult.result == TestResult::Passed;
        }
        return false;
    }
}

void TestBase::runSingleTest(const QString& testName)
{
    m_currentTestName = testName;
    m_testSkipped = false;
    
    qDebug() << QString("Running test: %1").arg(testName);
    
    m_testTimer.start();
    
    try {
        setupTest();
        
        if (!m_testSkipped) {
            m_testFunctions[testName]();
            
            if (!m_testSkipped) {
                recordTestResult(testName, TestResult::Passed, QString(), m_testTimer.elapsed());
                qDebug() << QString("Test %1 PASSED").arg(testName);
            }
        }
        
        cleanupTest();
        
    } catch (const std::exception& e) {
        cleanupTest();
        recordTestResult(testName, TestResult::Error, e.what(), m_testTimer.elapsed());
        qDebug() << QString("Test %1 ERROR: %2").arg(testName, e.what());
    } catch (...) {
        cleanupTest();
        recordTestResult(testName, TestResult::Error, "Unknown exception", m_testTimer.elapsed());
        qDebug() << QString("Test %1 ERROR: Unknown exception").arg(testName);
    }
}

void TestBase::recordTestResult(const QString& testName, TestResult result, 
                               const QString& errorMessage, qint64 executionTime)
{
    TestCase testCase;
    testCase.name = testName;
    testCase.result = result;
    testCase.errorMessage = errorMessage;
    testCase.executionTime = executionTime;
    testCase.timestamp = QDateTime::currentDateTime();
    
    m_testResults.append(testCase);
}

QList<TestCase> TestBase::getTestResults() const
{
    return m_testResults;
}

void TestBase::assertEqual(const QVariant& expected, const QVariant& actual, const QString& message)
{
    if (expected != actual) {
        QString error = QString("Expected '%1' but got '%2'")
                       .arg(expected.toString(), actual.toString());
        if (!message.isEmpty()) {
            error = message + ": " + error;
        }
        
        recordTestResult(m_currentTestName, TestResult::Failed, error, m_testTimer.elapsed());
        qDebug() << QString("Test %1 FAILED: %2").arg(m_currentTestName, error);
        throw std::runtime_error(error.toStdString());
    }
}

void TestBase::assertTrue(bool condition, const QString& message)
{
    if (!condition) {
        QString error = "Expected true but got false";
        if (!message.isEmpty()) {
            error = message + ": " + error;
        }
        
        recordTestResult(m_currentTestName, TestResult::Failed, error, m_testTimer.elapsed());
        qDebug() << QString("Test %1 FAILED: %2").arg(m_currentTestName, error);
        throw std::runtime_error(error.toStdString());
    }
}

void TestBase::assertFalse(bool condition, const QString& message)
{
    if (condition) {
        QString error = "Expected false but got true";
        if (!message.isEmpty()) {
            error = message + ": " + error;
        }
        
        recordTestResult(m_currentTestName, TestResult::Failed, error, m_testTimer.elapsed());
        qDebug() << QString("Test %1 FAILED: %2").arg(m_currentTestName, error);
        throw std::runtime_error(error.toStdString());
    }
}

bool TestBase::waitForSignal(QObject* sender, const char* signal, int timeout)
{
    QSignalSpy spy(sender, signal);
    return spy.wait(timeout);
}

bool TestBase::waitForCondition(std::function<bool()> condition, int timeout)
{
    QElapsedTimer timer;
    timer.start();
    
    while (timer.elapsed() < timeout) {
        if (condition()) {
            return true;
        }
        QCoreApplication::processEvents();
        QThread::msleep(10);
    }
    
    return false;
}

QString TestBase::generateRandomString(int length)
{
    const QString charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    QString result;
    
    for (int i = 0; i < length; ++i) {
        int index = QRandomGenerator::global()->bounded(charset.length());
        result.append(charset.at(index));
    }
    
    return result;
}

int TestBase::generateRandomInt(int min, int max)
{
    return QRandomGenerator::global()->bounded(min, max + 1);
}

QVariantMap TestBase::generateTestData(const QStringList& fields)
{
    QVariantMap data;
    
    for (const QString& field : fields) {
        if (field.contains("name", Qt::CaseInsensitive)) {
            data[field] = generateRandomString(10);
        } else if (field.contains("id", Qt::CaseInsensitive)) {
            data[field] = generateRandomInt(1, 1000);
        } else {
            data[field] = generateRandomString(8);
        }
    }
    
    return data;
}

void TestBase::registerTest(const QString& testName, std::function<void()> testFunc)
{
    m_testFunctions[testName] = testFunc;
}

void TestBase::skipTest(const QString& reason)
{
    m_testSkipped = true;
    recordTestResult(m_currentTestName, TestResult::Skipped, reason, m_testTimer.elapsed());
    qDebug() << QString("Test %1 SKIPPED: %2").arg(m_currentTestName, reason);
}

// TestRunner 实现
TestRunner* TestRunner::instance = nullptr;

TestRunner* TestRunner::getInstance()
{
    if (!instance) {
        instance = new TestRunner();
    }
    return instance;
}

TestRunner::TestRunner(QObject* parent)
    : QObject(parent)
{
}

void TestRunner::registerTestSuite(TestBase* testSuite, const QString& suiteName)
{
    QString name = suiteName.isEmpty() ? testSuite->metaObject()->className() : suiteName;
    m_testSuites[name] = testSuite;
    qDebug() << QString("Registered test suite: %1").arg(name);
}

bool TestRunner::runAllTests()
{
    qDebug() << "=== Running All Test Suites ===";
    
    bool allPassed = true;
    
    for (auto it = m_testSuites.begin(); it != m_testSuites.end(); ++it) {
        const QString& suiteName = it.key();
        TestBase* testSuite = it.value();
        
        emit testSuiteStarted(suiteName);
        qDebug() << QString("Running Test Suite: %1").arg(suiteName);
        
        bool suiteResult = testSuite->runTest();
        if (!suiteResult) {
            allPassed = false;
        }
        
        emit testSuiteFinished(suiteName);
    }
    
    return allPassed;
}

bool TestRunner::runTestSuite(const QString& suiteName)
{
    if (!m_testSuites.contains(suiteName)) {
        qDebug() << QString("Test suite '%1' not found").arg(suiteName);
        return false;
    }
    
    emit testSuiteStarted(suiteName);
    TestBase* testSuite = m_testSuites[suiteName];
    bool result = testSuite->runTest();
    emit testSuiteFinished(suiteName);
    
    return result;
}

QString TestRunner::generateTextReport() const
{
    QString report;
    QTextStream stream(&report);
    
    stream << "========================================\n";
    stream << "           TEST REPORT\n";
    stream << "========================================\n";
    stream << QString("Generated: %1\n\n").arg(QDateTime::currentDateTime().toString());
    
    for (auto it = m_testSuites.begin(); it != m_testSuites.end(); ++it) {
        const QString& suiteName = it.key();
        TestBase* testSuite = it.value();
        QList<TestCase> results = testSuite->getTestResults();
        
        stream << QString("Test Suite: %1\n").arg(suiteName);
        stream << "----------------------------------------\n";
        
        for (const TestCase& testCase : results) {
            QString status;
            switch (testCase.result) {
            case TestResult::Passed: status = "PASSED"; break;
            case TestResult::Failed: status = "FAILED"; break;
            case TestResult::Skipped: status = "SKIPPED"; break;
            case TestResult::Error: status = "ERROR"; break;
            }
            
            stream << QString("  %1: %2 (%3ms)\n")
                      .arg(testCase.name, status)
                      .arg(testCase.executionTime);
            
            if (!testCase.errorMessage.isEmpty()) {
                stream << QString("    Error: %1\n").arg(testCase.errorMessage);
            }
        }
        stream << "\n";
    }
    
    return report;
}

bool TestRunner::saveReport(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream stream(&file);
    stream << generateTextReport();
    
    return true;
}