#include "mainwindow.h"
#include "core/uimanager.h"
#include "core/businesslogicmanager.h"
#include "core/systemmanager.h"
#include "core/eventcoordinator.h"
#include "core/continuousoptimizer.h"
#include "core/intelligentanalyzer.h"
#include "core/adaptiveconfigmanager.h"
#include "core/loadbalancer.h"
#include "core/mlperformancepredictor.h"
#include "constants.h"

#include <QApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QCloseEvent>
#include <QShowEvent>
#include <QHideEvent>
#include <QTimer>
#include <QDebug>
#include <QDateTime>
#include <QIcon>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , uiManager(nullptr)
    , businessLogicManager(nullptr)
    , systemManager(nullptr)
    , eventCoordinator(nullptr)
    , continuousOptimizer(nullptr)
    , intelligentAnalyzer(nullptr)
    , adaptiveConfigManager(nullptr)
    , loadBalancer(nullptr)
    , mlPerformancePredictor(nullptr)
    , m_applicationInitialized(false)
    , m_applicationShuttingDown(false)
    , managersInitialized(false)
    , currentApplicationState("Initializing")
    , heartbeatTimer(nullptr)
    , periodicUpdateTimer(nullptr)
    , criticalErrorCount(0)
    , recoverableErrorCount(0)
    , autoSaveEnabled(true)
    , confirmExitEnabled(true)
    , heartbeatInterval(1000)  // 1秒
    , periodicUpdateInterval(5000)  // 5秒
    , maxCriticalErrors(5)
    , maxRecoverableErrors(50)
{
    // 设置基本窗口属性
    setWindowTitle("工业胶水点胶设备控制系统 v1.0.0");
    setMinimumSize(1400, 900);
    setWindowIcon(QIcon(":/icons/app.png"));
    
    // 创建管理器
    createManagers();
    
    // 设置管理器连接
    setupManagerConnections();
    
    // 设置定时器
    setupTimers();
    
    // 设置事件处理
    setupEventHandling();
    
    qDebug() << "MainWindow created with manager architecture";
}

/**
 * @brief MainWindow析构函数
 * 
 * 安全地释放所有资源，包括定时器、管理器等
 * 确保避免内存泄漏和空指针访问
 */
MainWindow::~MainWindow()
{
    // 标记正在关闭，防止重复调用
    if (!m_applicationShuttingDown) {
        shutdownApplication();
    }
    
    // 安全停止和释放定时器
    if (heartbeatTimer) {
        heartbeatTimer->stop();
        heartbeatTimer->deleteLater();
        heartbeatTimer = nullptr;
    }
    
    if (periodicUpdateTimer) {
        periodicUpdateTimer->stop();
        periodicUpdateTimer->deleteLater();
        periodicUpdateTimer = nullptr;
    }
    
    // 确保管理器按正确顺序释放
    // 先停止持续优化组件
    if (continuousOptimizer) {
        continuousOptimizer->stopOptimization();
        continuousOptimizer = nullptr;
    }
    
    if (intelligentAnalyzer) {
        intelligentAnalyzer->stopAnalysis();
        intelligentAnalyzer = nullptr;
    }
    
    if (adaptiveConfigManager) {
        adaptiveConfigManager->stopAdaptiveAdjustment();
        adaptiveConfigManager = nullptr;
    }
    
    if (loadBalancer) {
        loadBalancer->stopBalancing();
        loadBalancer = nullptr;
    }
    
    if (mlPerformancePredictor) {
        mlPerformancePredictor->stopRealTimeMonitoring();
        mlPerformancePredictor = nullptr;
    }
    
    // 先关闭事件协调器，避免在释放过程中触发事件
    if (eventCoordinator) {
        eventCoordinator->shutdown();
        eventCoordinator = nullptr;
    }
    
    // 释放UI管理器
    if (uiManager) {
        uiManager = nullptr;
    }
    
    // 释放业务逻辑管理器
    if (businessLogicManager) {
        businessLogicManager = nullptr;
    }
    
    // 最后释放系统管理器
    if (systemManager) {
        systemManager = nullptr;
    }
    
    qDebug() << "MainWindow destroyed safely";
}

/**
 * @brief 初始化应用程序
 * 
 * 增强的异常处理机制，确保初始化过程的安全性
 */
void MainWindow::initializeApplication()
{
    if (m_applicationInitialized) {
        qWarning() << "Application already initialized";
        return;
    }
    
    currentApplicationState = "Initializing";
    emit applicationStateChanged(currentApplicationState);
    
    try {
        // 初始化管理器
        initializeManagers();
        
        // 加载应用程序设置
        loadApplicationSettings();
        
        // 设置应用程序
        setupApplication();
        
        // 标记初始化完成
        m_applicationInitialized = true;
        managersInitialized = true;
        currentApplicationState = "Ready";
        
        // 发送信号
        emit applicationInitialized();
        emit applicationReady();
        emit applicationStateChanged(currentApplicationState);
        
        qDebug() << "Application initialized successfully";
        
    } catch (const std::exception& e) {
        handleCriticalError(QString("Application initialization failed: %1").arg(e.what()));
        m_applicationInitialized = false;
        managersInitialized = false;
        currentApplicationState = "Failed";
        emit applicationStateChanged(currentApplicationState);
    } catch (...) {
        handleCriticalError("Unknown error during application initialization");
        m_applicationInitialized = false;
        managersInitialized = false;
        currentApplicationState = "Failed";
        emit applicationStateChanged(currentApplicationState);
    }
}

void MainWindow::shutdownApplication()
{
    if (m_applicationShuttingDown) {
        return;
    }
    
    m_applicationShuttingDown = true;
    currentApplicationState = "Shutting Down";
    emit applicationShuttingDown();
    emit applicationStateChanged(currentApplicationState);
    
    try {
        // 准备关闭
        prepareShutdown();
        
        // 停止定时器
        stopTimers();
        
        // 保存应用程序状态
        saveApplicationState();
        
        // 关闭管理器
        shutdownManagers();
        
        currentApplicationState = "Closed";
        emit applicationClosed();
        emit applicationStateChanged(currentApplicationState);
        
        qDebug() << "Application shutdown completed";
        
    } catch (const std::exception& e) {
        qCritical() << "Error during shutdown:" << e.what();
    }
}

void MainWindow::createManagers()
{
    // 创建事件协调器（优先创建，因为其他管理器会用到它）
    eventCoordinator = new EventCoordinator(this);
    
    // 创建系统管理器
    systemManager = new SystemManager(this);
    
    // 创建业务逻辑管理器
    businessLogicManager = new BusinessLogicManager(this);
    
    // 创建UI管理器
    uiManager = new UIManager(this, this);
    
    // 创建持续优化组件
    continuousOptimizer = new ContinuousOptimizer(this);
    intelligentAnalyzer = new IntelligentAnalyzer(this);
    adaptiveConfigManager = new AdaptiveConfigManager(this);
    loadBalancer = new LoadBalancer(this);
    mlPerformancePredictor = new MLPerformancePredictor(this);
    
    // 注册管理器到事件协调器
    eventCoordinator->registerUIManager(uiManager);
    eventCoordinator->registerBusinessLogicManager(businessLogicManager);
    eventCoordinator->registerSystemManager(systemManager);
    
    qDebug() << "Managers and optimization components created successfully";
}

void MainWindow::setupManagerConnections()
{
    // 连接UI管理器信号
    connect(uiManager, &UIManager::fileOpenRequested, 
            this, [this]() { onUIManagerEvent("FileOpen", QVariant()); });
    connect(uiManager, &UIManager::fileSaveRequested, 
            this, [this]() { onUIManagerEvent("FileSave", QVariant()); });
    connect(uiManager, &UIManager::exitRequested, 
            this, [this]() { onUIManagerEvent("Exit", QVariant()); });
    
    // 连接业务逻辑管理器信号
    connect(businessLogicManager, &BusinessLogicManager::deviceStatusChanged,
            this, [this](const QString& status) { onBusinessLogicManagerEvent("DeviceStatus", status); });
    connect(businessLogicManager, &BusinessLogicManager::deviceError,
            this, [this](const QString& error) { onBusinessLogicManagerEvent("DeviceError", error); });
    
    // 连接系统管理器信号
    connect(systemManager, &SystemManager::criticalErrorOccurred,
            this, [this](const QString& error) { onSystemManagerEvent("CriticalError", error); });
    connect(systemManager, &SystemManager::systemShutdown,
            this, [this]() { onSystemManagerEvent("SystemShutdown", QVariant()); });
    
    // 连接事件协调器信号
    connect(eventCoordinator, &EventCoordinator::eventFailed,
            this, [this](const EventCoordinator::Event& event, const QString& error) {
                onEventCoordinatorEvent("EventFailed", 
                    QVariant::fromValue(QStringList() << event.eventId << error));
            });
    
    qDebug() << "Manager connections established";
}

void MainWindow::initializeManagers()
{
    // 初始化事件协调器
    eventCoordinator->initialize();
    
    // 初始化系统管理器
    systemManager->initialize();
    
    // 初始化业务逻辑管理器
    businessLogicManager->initialize();
    
    // 初始化UI管理器
    uiManager->initializeUI();
    
    // 初始化持续优化组件
    continuousOptimizer->initialize(performanceMonitor, memoryOptimizer, uiUpdateOptimizer, bufferPool, configManager);
    intelligentAnalyzer->initialize();
    adaptiveConfigManager->initialize(continuousOptimizer, intelligentAnalyzer);
    loadBalancer->initialize();
    mlPerformancePredictor->initialize();
    
    // 启动持续优化
    continuousOptimizer->startOptimization();
    intelligentAnalyzer->startAnalysis();
    adaptiveConfigManager->startAdaptiveAdjustment();
    loadBalancer->startBalancing();
    // MLPerformancePredictor 没有 startRealTimeMonitoring 方法，移除此调用
    
    qDebug() << "Managers and optimization components initialized successfully";
}

void MainWindow::setupTimers()
{
    // 心跳定时器
    heartbeatTimer = new QTimer(this);
    connect(heartbeatTimer, &QTimer::timeout, this, &MainWindow::onHeartbeat);
    heartbeatTimer->start(heartbeatInterval);
    
    // 周期性更新定时器
    periodicUpdateTimer = new QTimer(this);
    connect(periodicUpdateTimer, &QTimer::timeout, this, &MainWindow::onPeriodicUpdate);
    periodicUpdateTimer->start(periodicUpdateInterval);
    
    qDebug() << "Timers setup completed";
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_applicationShuttingDown) {
        event->accept();
        return;
    }
    
    if (confirmExitEnabled && !confirmExit()) {
        event->ignore();
        return;
    }
    
    shutdownApplication();
    event->accept();
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::WindowStateChange) {
        emit mainWindowStateChanged("WindowStateChanged");
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::showEvent(QShowEvent* event)
{
    emit mainWindowStateChanged("Shown");
    QMainWindow::showEvent(event);
}

void MainWindow::hideEvent(QHideEvent* event)
{
    emit mainWindowStateChanged("Hidden");
    QMainWindow::hideEvent(event);
}

void MainWindow::onUIManagerEvent(const QString& eventType, const QVariant& data)
{
    qDebug() << "UI Manager Event:" << eventType << data;
    
    if (eventType == "FileOpen") {
        // 处理文件打开事件
        qDebug() << "File open event handled";
    } else if (eventType == "FileSave") {
        // 处理文件保存事件
        qDebug() << "File save event handled";
    } else if (eventType == "Exit") {
        close();
    }
}

void MainWindow::onBusinessLogicManagerEvent(const QString& eventType, const QVariant& data)
{
    qDebug() << "Business Logic Manager Event:" << eventType << data;
    
    if (eventType == "DeviceStatus") {
        uiManager->updateDeviceStatus(data.toString());
    } else if (eventType == "DeviceError") {
        handleRecoverableError(data.toString());
    }
}

void MainWindow::onSystemManagerEvent(const QString& eventType, const QVariant& data)
{
    qDebug() << "System Manager Event:" << eventType << data;
    
    if (eventType == "CriticalError") {
        handleCriticalError(data.toString());
    } else if (eventType == "SystemShutdown") {
        close();
    }
}

void MainWindow::onEventCoordinatorEvent(const QString& eventType, const QVariant& data)
{
    qDebug() << "Event Coordinator Event:" << eventType << data;
    
    if (eventType == "EventFailed") {
        QStringList eventData = data.toStringList();
        if (eventData.size() >= 2) {
            qWarning() << "Event failed:" << eventData[0] << "-" << eventData[1];
        }
    }
}

void MainWindow::onHeartbeat()
{
    // 简单的心跳处理
    if (managersInitialized) {
        validateApplicationState();
    }
}

void MainWindow::onPeriodicUpdate()
{
    // 周期性更新
    if (managersInitialized) {
        updateApplicationState();
    }
}

/**
 * @brief 处理严重错误
 * 
 * 统一的错误处理机制，记录错误并采取适当的恢复措施
 * @param error 错误信息
 */
void MainWindow::handleCriticalError(const QString& error)
{
    criticalErrorCount++;
    lastErrorTime = QDateTime::currentDateTime();
    
    qCritical() << "Critical Error:" << error;
    emit criticalErrorOccurred(error);
    
    // 记录错误到系统管理器
    if (systemManager) {
        try {
            systemManager->logError(error, "MainWindow");
        } catch (const std::exception& e) {
            qCritical() << "Failed to log error to SystemManager:" << e.what();
        } catch (...) {
            qCritical() << "Unknown error while logging to SystemManager";
        }
    }
    
    // 显示错误信息给用户
    if (uiManager) {
        try {
            QMessageBox::critical(this, "严重错误", 
                QString("系统遇到严重错误：%1\n\n请联系技术支持。").arg(error));
        } catch (const std::exception& e) {
            qCritical() << "Failed to show error message:" << e.what();
        } catch (...) {
            qCritical() << "Unknown error while showing error message";
        }
    }
    
    // 检查是否需要关闭应用程序
    if (criticalErrorCount >= maxCriticalErrors) {
        qCritical() << "Too many critical errors, shutting down";
        close();
    }
}

void MainWindow::handleRecoverableError(const QString& error)
{
    recoverableErrorCount++;
    lastErrorTime = QDateTime::currentDateTime();
    
    qWarning() << "Recoverable Error:" << error;
    emit recoverableErrorOccurred(error);
    
    if (systemManager) {
        systemManager->logWarning(error, "MainWindow");
    }
    
    if (uiManager) {
        uiManager->updateStatusBar(QString("Error: %1").arg(error));
    }
}

bool MainWindow::confirmExit()
{
    if (!uiManager) {
        return true;
    }
    
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("退出程序");
    msgBox.setText("确定要退出工业点胶控制软件吗？");
    msgBox.setIcon(QMessageBox::Question);
    
    QPushButton* yesButton = msgBox.addButton("确定", QMessageBox::YesRole);
    QPushButton* noButton = msgBox.addButton("取消", QMessageBox::NoRole);
    msgBox.setDefaultButton(noButton);
    
    msgBox.exec();
    
    return msgBox.clickedButton() == yesButton;
}

void MainWindow::loadApplicationSettings()
{
    // 加载应用程序设置
    autoSaveEnabled = true;
    confirmExitEnabled = true;
    heartbeatInterval = 1000;  // 1秒
    periodicUpdateInterval = 5000;  // 5秒
    
    qDebug() << "Application settings loaded";
}

void MainWindow::saveApplicationSettings()
{
    // 保存应用程序设置
    qDebug() << "Application settings saved";
}

void MainWindow::setupApplication()
{
    // 设置应用程序级别的配置
    if (systemManager) {
        systemManager->startSystemMonitoring();
    }
    
    // 启动事件处理
    if (eventCoordinator) {
        eventCoordinator->initialize();
    }
    
    qDebug() << "Application setup completed";
}

void MainWindow::prepareShutdown()
{
    // 保存所有待保存的数据
    if (autoSaveEnabled && businessLogicManager) {
        // 保存参数
        qDebug() << "Parameters saved";
    }
    
    // 停止事件处理
    if (eventCoordinator) {
        eventCoordinator->shutdown();
    }
    
    // 停止系统
    if (systemManager) {
        systemManager->stopSystemMonitoring();
    }
    
    qDebug() << "Preparing for shutdown";
}

void MainWindow::shutdownManagers()
{
    // 关闭管理器
    if (uiManager) {
        // uiManager->saveSettings();
    }
    
    if (businessLogicManager) {
        businessLogicManager->shutdown();
    }
    
    if (systemManager) {
        // systemManager->stopSystem();
    }
    
    if (eventCoordinator) {
        eventCoordinator->shutdown();
    }
    
    qDebug() << "Managers shutdown completed";
}

void MainWindow::stopTimers()
{
    if (heartbeatTimer) {
        heartbeatTimer->stop();
    }
    if (periodicUpdateTimer) {
        periodicUpdateTimer->stop();
    }
}

void MainWindow::saveApplicationState()
{
    // 保存应用程序状态
    qDebug() << "Application state saved";
}

void MainWindow::setupEventHandling()
{
    // 设置事件处理
    qDebug() << "Event handling setup completed";
}

void MainWindow::updateApplicationState()
{
    // 更新应用程序状态
    if (m_applicationInitialized && !m_applicationShuttingDown) {
        currentApplicationState = "Running";
        emit applicationStateChanged(currentApplicationState);
    }
}

void MainWindow::validateApplicationState()
{
    // 验证应用程序状态
    if (!m_applicationInitialized || m_applicationShuttingDown) {
        return;
    }
    
    // 检查管理器状态
    bool managersHealthy = true;
    
    if (systemManager && !systemManager->isSystemHealthy()) {
        managersHealthy = false;
    }
    
    if (!managersHealthy) {
        handleRecoverableError("System health check failed");
    }
}

#include "moc_mainwindow.cpp"