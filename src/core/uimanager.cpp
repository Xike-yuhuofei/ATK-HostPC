#include "uimanager.h"
#include "../ui/devicecontrolwidget.h"
#include "../ui/datamonitorwidget.h"
#include "../ui/parameterwidget.h"
#include "../ui/alarmwidget.h"
#include "../ui/chartwidget.h"
#include "../ui/datarecordwidget.h"
#include "../ui/securitywidget.h"
#include "../ui/communicationwidget.h"
#include <QDebug>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QTabWidget>
#include <QSplitter>
#include <QDockWidget>
#include <QProgressBar>
#include <QLabel>
#include <QAction>
#include <QTimer>
#include <QVBoxLayout>
#include <QDateTime>

UIManager::UIManager(QMainWindow* mainWindow, QObject* parent)
    : QObject(parent)
    , mainWindow(mainWindow)
    , centralWidget(nullptr)
    , tabWidget(nullptr)
    , mainSplitter(nullptr)
    , isFullScreenMode(false)
    , isMinimizedToTrayMode(false)
{
    qDebug() << "UIManager created";
}

UIManager::~UIManager()
{
    qDebug() << "UIManager destroyed";
}

void UIManager::initializeUI()
{
    try {
        // 创建基本UI组件
        createMenus();
        createToolBars();
        createStatusBar();
        createCentralWidget();
        createDockWidgets();
        
        // 设置布局和连接
        setupLayoutAndConnections();
        
        // 加载设置
        loadSettings();
        
        qDebug() << "UI initialized successfully";
        
    } catch (const std::exception& e) {
        qCritical() << "UI initialization failed:" << e.what();
        throw;
    }
}

void UIManager::createMenus()
{
    if (!mainWindow) {
        qWarning() << "MainWindow is null, cannot create menus";
        return;
    }
    
    // 创建菜单栏
    QMenuBar* menuBar = mainWindow->menuBar();
    
    // 文件菜单
    fileMenu = menuBar->addMenu("文件(&F)");
    fileMenu->addAction("打开(&O)", QKeySequence::Open, this, &UIManager::onFileOpen);
    fileMenu->addAction("保存(&S)", QKeySequence::Save, this, &UIManager::onFileSave);
    fileMenu->addSeparator();
    fileMenu->addAction("导入(&I)", this, &UIManager::onFileImport);
    fileMenu->addAction("导出(&E)", this, &UIManager::onFileExport);
    fileMenu->addSeparator();
    fileMenu->addAction("退出(&X)", QKeySequence::Quit, this, &UIManager::onFileExit);
    
    // 编辑菜单
    editMenu = menuBar->addMenu("编辑(&E)");
    editMenu->addAction("撤销(&U)", QKeySequence::Undo, this, &UIManager::onEditUndo);
    editMenu->addAction("重做(&R)", QKeySequence::Redo, this, &UIManager::onEditRedo);
    editMenu->addSeparator();
    editMenu->addAction("剪切(&T)", QKeySequence::Cut, this, &UIManager::onEditCut);
    editMenu->addAction("复制(&C)", QKeySequence::Copy, this, &UIManager::onEditCopy);
    editMenu->addAction("粘贴(&P)", QKeySequence::Paste, this, &UIManager::onEditPaste);
    editMenu->addSeparator();
    editMenu->addAction("首选项(&P)", this, &UIManager::onEditPreferences);
    
    // 视图菜单
    viewMenu = menuBar->addMenu("视图(&V)");
    viewMenu->addAction("全屏(&F)", QKeySequence::FullScreen, this, &UIManager::onViewFullScreen);
    viewMenu->addAction("重置布局(&R)", this, &UIManager::onViewResetLayout);
    viewMenu->addSeparator();
    viewMenu->addAction("显示状态栏(&S)", this, &UIManager::onViewShowStatusBar)->setCheckable(true);
    viewMenu->addAction("显示工具栏(&T)", this, &UIManager::onViewShowToolBar)->setCheckable(true);
    
    // 工具菜单
    toolsMenu = menuBar->addMenu("工具(&T)");
    toolsMenu->addAction("设备控制(&D)");
    toolsMenu->addAction("数据监控(&M)");
    toolsMenu->addAction("参数设置(&P)");
    toolsMenu->addAction("报警管理(&A)");
    toolsMenu->addAction("图表显示(&C)");
    toolsMenu->addAction("数据记录(&R)");
    toolsMenu->addAction("安全管理(&S)");
    toolsMenu->addAction("通信管理(&C)");
    
    // 帮助菜单
    helpMenu = menuBar->addMenu("帮助(&H)");
    helpMenu->addAction("关于(&A)", this, &UIManager::onHelpAbout);
    helpMenu->addAction("用户手册(&M)", this, &UIManager::onHelpManual);
    helpMenu->addAction("技术支持(&S)", this, &UIManager::onHelpSupport);
    helpMenu->addAction("检查更新(&U)", this, &UIManager::onHelpUpdate);
    
    qDebug() << "Menus created successfully";
}

void UIManager::createToolBars()
{
    if (!mainWindow) {
        qWarning() << "MainWindow is null, cannot create toolbars";
        return;
    }
    
    // 主工具栏
    mainToolBar = mainWindow->addToolBar("主工具栏");
    mainToolBar->addAction("新建", this, &UIManager::onFileOpen);
    mainToolBar->addAction("打开", this, &UIManager::onFileOpen);
    mainToolBar->addAction("保存", this, &UIManager::onFileSave);
    mainToolBar->addSeparator();
    mainToolBar->addAction("连接", this, [this]() { updateConnectionStatus(true); });
    mainToolBar->addAction("断开", this, [this]() { updateConnectionStatus(false); });
    
    // 设备工具栏
    deviceToolBar = mainWindow->addToolBar("设备工具栏");
    deviceToolBar->addAction("启动设备");
    deviceToolBar->addAction("停止设备");
    deviceToolBar->addAction("复位设备");
    
    // 通信工具栏
    communicationToolBar = mainWindow->addToolBar("通信工具栏");
    communicationToolBar->addAction("串口设置");
    communicationToolBar->addAction("网络设置");
    communicationToolBar->addAction("CAN设置");
    
    // 数据工具栏
    dataToolBar = mainWindow->addToolBar("数据工具栏");
    dataToolBar->addAction("开始记录");
    dataToolBar->addAction("停止记录");
    dataToolBar->addAction("导出数据");
    
    qDebug() << "Toolbars created successfully";
}

void UIManager::createStatusBar()
{
    if (!mainWindow) {
        qWarning() << "MainWindow is null, cannot create status bar";
        return;
    }
    
    QStatusBar* statusBar = mainWindow->statusBar();
    
    // 状态标签
    statusLabel = new QLabel("就绪", statusBar);
    statusBar->addWidget(statusLabel);
    
    // 连接状态标签
    connectionLabel = new QLabel("未连接", statusBar);
    statusBar->addPermanentWidget(connectionLabel);
    
    // 设备状态标签
    deviceStatusLabel = new QLabel("设备离线", statusBar);
    statusBar->addPermanentWidget(deviceStatusLabel);
    
    // 用户标签
    userLabel = new QLabel("用户: 未登录", statusBar);
    statusBar->addPermanentWidget(userLabel);
    
    // 时间标签
    timeLabel = new QLabel("", statusBar);
    statusBar->addPermanentWidget(timeLabel);
    
    // 统计标签
    statisticsLabel = new QLabel("发送: 0 接收: 0", statusBar);
    statusBar->addPermanentWidget(statisticsLabel);
    
    // 进度条
    progressBar = new QProgressBar(statusBar);
    progressBar->setVisible(false);
    statusBar->addPermanentWidget(progressBar);
    
    // 启动时间更新定时器
    QTimer* timeTimer = new QTimer(this);
    connect(timeTimer, &QTimer::timeout, this, &UIManager::updateTimeDisplay);
    timeTimer->start(1000); // 每秒更新一次
    
    qDebug() << "Status bar created successfully";
}

void UIManager::createCentralWidget()
{
    if (!mainWindow) {
        qWarning() << "MainWindow is null, cannot create central widget";
        return;
    }
    
    // 简化布局：直接使用标签页组件作为中央部件，占用全部空间
    tabWidget = new QTabWidget(mainWindow);
    mainWindow->setCentralWidget(tabWidget);
    
    // 设置标签页样式，让它看起来更专业
    tabWidget->setTabPosition(QTabWidget::North);
    tabWidget->setTabsClosable(false);
    tabWidget->setMovable(false);
    
    // 创建功能窗口部件
    createFunctionalWidgets();
    
    qDebug() << "Central widget created successfully - full-width layout";
}

void UIManager::createDockWidgets()
{
    if (!mainWindow) {
        qWarning() << "MainWindow is null, cannot create dock widgets";
        return;
    }
    
    // 暂时禁用停靠窗口，让主功能区域占用全部空间
    // 这样可以避免左右边栏挡住主要功能，给用户更大的工作区域
    
    /* 
    // 如果将来需要停靠窗口，可以取消注释并设置合适的尺寸
    QDockWidget* deviceControlDock = new QDockWidget("设备控制", mainWindow);
    deviceControlDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    deviceControlDock->setMinimumWidth(250);  // 设置最小宽度
    deviceControlDock->resize(300, 400);       // 设置默认尺寸
    mainWindow->addDockWidget(Qt::LeftDockWidgetArea, deviceControlDock);
    
    QDockWidget* dataMonitorDock = new QDockWidget("数据监控", mainWindow);
    dataMonitorDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    dataMonitorDock->setMinimumWidth(250);
    dataMonitorDock->resize(300, 400);
    mainWindow->addDockWidget(Qt::RightDockWidgetArea, dataMonitorDock);
    */
    
    qDebug() << "Dock widgets skipped to maximize main area";
}

void UIManager::createFunctionalWidgets()
{
    // 先创建简单的组件，测试哪个组件导致崩溃
    
    try {
        qDebug() << "Creating DeviceControlWidget...";
        DeviceControlWidget* deviceControlWidget = new DeviceControlWidget(tabWidget);
        tabWidget->addTab(deviceControlWidget, "设备控制");
        qDebug() << "DeviceControlWidget created successfully";
        
        qDebug() << "Creating DataMonitorWidget...";
        DataMonitorWidget* dataMonitorWidget = new DataMonitorWidget(tabWidget);
        tabWidget->addTab(dataMonitorWidget, "数据监控");
        qDebug() << "DataMonitorWidget created successfully";
        
        qDebug() << "Creating ParameterWidget...";
        ParameterWidget* parameterWidget = new ParameterWidget(tabWidget);
        tabWidget->addTab(parameterWidget, "参数设置");
        qDebug() << "ParameterWidget created successfully";
        
        qDebug() << "Creating ChartWidget...";
        ChartWidget* chartWidget = new ChartWidget(tabWidget);
        tabWidget->addTab(chartWidget, "图表显示");
        qDebug() << "ChartWidget created successfully";
        
        qDebug() << "Creating DataRecordWidget...";
        DataRecordWidget* dataRecordWidget = new DataRecordWidget(tabWidget);
        tabWidget->addTab(dataRecordWidget, "数据记录");
        qDebug() << "DataRecordWidget created successfully";
        
        qDebug() << "Creating SecurityWidget...";
        SecurityWidget* securityWidget = new SecurityWidget(tabWidget);
        tabWidget->addTab(securityWidget, "安全管理");
        qDebug() << "SecurityWidget created successfully";
        
        qDebug() << "Creating CommunicationWidget...";
        CommunicationWidget* communicationWidget = new CommunicationWidget(tabWidget);
        tabWidget->addTab(communicationWidget, "通信管理");
        qDebug() << "CommunicationWidget created successfully";
        
        // 尝试创建简化版本的AlarmWidget
        qDebug() << "Creating AlarmWidget...";
        AlarmWidget* alarmWidget = new AlarmWidget(tabWidget);
        tabWidget->addTab(alarmWidget, "报警管理");
        qDebug() << "AlarmWidget created successfully";
        
    } catch (const std::exception& e) {
        qCritical() << "Exception creating functional widgets:" << e.what();
        return;
    } catch (...) {
        qCritical() << "Unknown exception creating functional widgets";
        return;
    }
    
    qDebug() << "Functional widgets created successfully";
}

void UIManager::setupLayoutAndConnections()
{
    // 连接标签页变化信号
    if (tabWidget) {
        connect(tabWidget, &QTabWidget::currentChanged, this, &UIManager::onTabChanged);
    }
    
    qDebug() << "Layout and connections setup completed";
}

void UIManager::loadSettings()
{
    // 这里可以加载UI设置
    qDebug() << "UI settings loaded";
}

void UIManager::saveSettings()
{
    // 这里可以保存UI设置
    qDebug() << "UI settings saved";
}

// 菜单响应槽函数
void UIManager::onFileOpen()
{
    qDebug() << "File open requested";
    emit fileOpenRequested();
}

void UIManager::onFileSave()
{
    qDebug() << "File save requested";
    emit fileSaveRequested();
}

void UIManager::onFileImport()
{
    qDebug() << "File import requested";
    emit fileImportRequested();
}

void UIManager::onFileExport()
{
    qDebug() << "File export requested";
    emit fileExportRequested();
}

void UIManager::onFileExit()
{
    qDebug() << "File exit requested";
    emit exitRequested();
}

void UIManager::onEditUndo()
{
    qDebug() << "Edit undo requested";
    emit undoRequested();
}

void UIManager::onEditRedo()
{
    qDebug() << "Edit redo requested";
    emit redoRequested();
}

void UIManager::onEditCut()
{
    qDebug() << "Edit cut requested";
    emit cutRequested();
}

void UIManager::onEditCopy()
{
    qDebug() << "Edit copy requested";
    emit copyRequested();
}

void UIManager::onEditPaste()
{
    qDebug() << "Edit paste requested";
    emit pasteRequested();
}

void UIManager::onEditPreferences()
{
    qDebug() << "Edit preferences requested";
    emit preferencesRequested();
}

void UIManager::onViewFullScreen()
{
    qDebug() << "View full screen requested";
    toggleFullScreen();
}

void UIManager::onViewResetLayout()
{
    qDebug() << "View reset layout requested";
    resetLayout();
}

void UIManager::onViewShowStatusBar()
{
    qDebug() << "View show status bar requested";
    showStatusBar(true);
}

void UIManager::onViewShowToolBar()
{
    qDebug() << "View show toolbar requested";
    showToolBar(true);
}

void UIManager::onHelpAbout()
{
    qDebug() << "Help about requested";
    emit aboutRequested();
}

void UIManager::onHelpManual()
{
    qDebug() << "Help manual requested";
    emit manualRequested();
}

void UIManager::onHelpSupport()
{
    qDebug() << "Help support requested";
    emit supportRequested();
}

void UIManager::onHelpUpdate()
{
    qDebug() << "Help update requested";
    emit updateRequested();
}

void UIManager::onTabChanged(int index)
{
    qDebug() << "Tab changed to index:" << index;
    emit tabChanged(index);
}

// UI更新方法
void UIManager::updateStatusBar(const QString& message)
{
    if (statusLabel) {
        statusLabel->setText(message);
    }
}

void UIManager::updateConnectionStatus(bool connected)
{
    if (connectionLabel) {
        connectionLabel->setText(connected ? "已连接" : "未连接");
    }
}

void UIManager::updateDeviceStatus(const QString& status)
{
    if (deviceStatusLabel) {
        deviceStatusLabel->setText("设备: " + status);
    }
}

void UIManager::updateUserStatus(const QString& user)
{
    if (userLabel) {
        userLabel->setText("用户: " + user);
    }
}

void UIManager::updateStatistics(qint64 bytesSent, qint64 bytesReceived)
{
    if (statisticsLabel) {
        statisticsLabel->setText(QString("发送: %1 接收: %2").arg(bytesSent).arg(bytesReceived));
    }
}

void UIManager::updateTimeDisplay()
{
    if (timeLabel) {
        timeLabel->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
    }
}

void UIManager::showProgressBar(const QString& text, int maximum)
{
    if (progressBar) {
        progressBar->setVisible(true);
        progressBar->setTextVisible(true);
        progressBar->setFormat(text + " %p%");
        progressBar->setMaximum(maximum);
        progressBar->setValue(0);
    }
}

void UIManager::hideProgressBar()
{
    if (progressBar) {
        progressBar->setVisible(false);
    }
}

void UIManager::updateProgress(int value)
{
    if (progressBar && progressBar->isVisible()) {
        progressBar->setValue(value);
    }
}

void UIManager::toggleFullScreen()
{
    if (!mainWindow) {
        return;
    }
    
    if (isFullScreenMode) {
        mainWindow->showNormal();
        isFullScreenMode = false;
    } else {
        mainWindow->showFullScreen();
        isFullScreenMode = true;
    }
    
    emit fullScreenToggled(isFullScreenMode);
}

void UIManager::resetLayout()
{
    if (!mainWindow) {
        return;
    }
    
    // 重置窗口布局
    mainWindow->restoreState(QByteArray());
    emit layoutReset();
}

void UIManager::showStatusBar(bool show)
{
    if (!mainWindow) {
        return;
    }
    
    mainWindow->statusBar()->setVisible(show);
    emit statusBarToggled(show);
}

void UIManager::showToolBar(bool show)
{
    if (!mainWindow) {
        return;
    }
    
    // 显示/隐藏所有工具栏
    QList<QToolBar*> toolBars = mainWindow->findChildren<QToolBar*>();
    for (QToolBar* toolbar : toolBars) {
        toolbar->setVisible(show);
    }
    
    emit toolBarToggled(show);
} 