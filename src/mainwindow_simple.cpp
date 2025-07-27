#include "mainwindow_simple.h"
#include <QCloseEvent>
#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <iostream>

SimpleMainWindow::SimpleMainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_isRunning(false)
    , m_dataCounter(0)
{
    std::cout << "SimpleMainWindow constructor started" << std::endl;
    
    setWindowTitle("工业点胶设备上位机控制软件 v2.0.0 - 简化版");
    setMinimumSize(1200, 800);
    
    setupUI();
    setupMenus();
    setupStatusBar();
    
    // 应用现代化样式
    applyModernStyle();
    
    // 启动定时器更新状态
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &SimpleMainWindow::updateStatus);
    m_updateTimer->start(1000);
    
    std::cout << "SimpleMainWindow constructor completed" << std::endl;
}

SimpleMainWindow::~SimpleMainWindow()
{
    std::cout << "SimpleMainWindow destructor called" << std::endl;
}

void SimpleMainWindow::closeEvent(QCloseEvent* event)
{
    std::cout << "SimpleMainWindow closeEvent called" << std::endl;
    
    if (m_isRunning) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("确认退出");
        msgBox.setText("设备正在运行中，确定要退出吗？");
        msgBox.setIcon(QMessageBox::Question);
        
        QPushButton* yesButton = msgBox.addButton("确定", QMessageBox::YesRole);
        QPushButton* noButton = msgBox.addButton("取消", QMessageBox::NoRole);
        msgBox.setDefaultButton(noButton);
        
        msgBox.exec();
        
        if (msgBox.clickedButton() == noButton) {
            event->ignore();
            return;
        }
    }
    
    event->accept();
}

void SimpleMainWindow::updateStatus()
{
    // 更新状态栏时间
    statusBar()->showMessage(QString("系统时间: %1").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")));
    
    // 模拟数据更新
    m_dataCounter++;
    
    // 更新进度条
    m_progressBar->setValue((m_dataCounter % 100));
    
    // 更新表格数据
    if (m_dataCounter % 5 == 0) {
        addRandomData();
    }
}

void SimpleMainWindow::onStartClicked()
{
    QMessageBox::information(this, "设备控制", "设备启动命令已发送！");
    m_statusLabel->setText("状态: 运行中");
    m_statusLabel->setStyleSheet("color: green; font-weight: bold;");
    m_isRunning = true;
}

void SimpleMainWindow::onStopClicked()
{
    QMessageBox::information(this, "设备控制", "设备停止命令已发送！");
    m_statusLabel->setText("状态: 已停止");
    m_statusLabel->setStyleSheet("color: red; font-weight: bold;");
    m_isRunning = false;
}

void SimpleMainWindow::onEmergencyClicked()
{
    QMessageBox::warning(this, "紧急停止", "紧急停止已触发！");
    m_statusLabel->setText("状态: 紧急停止");
    m_statusLabel->setStyleSheet("color: red; font-weight: bold; background-color: yellow;");
    m_isRunning = false;
}

void SimpleMainWindow::onAboutClicked()
{
    QMessageBox::about(this, "关于", "工业点胶设备上位机控制软件 v2.0.0\n简化版 - 用于调试测试");
}

void SimpleMainWindow::onSettingsClicked()
{
    QMessageBox::information(this, "设置", "设置功能待实现");
}

void SimpleMainWindow::setupUI()
{
    std::cout << "Setting up UI..." << std::endl;
    
    QWidget *centralWidget = new QWidget;
    setCentralWidget(centralWidget);
    
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
    
    // 创建左侧控制面板
    QWidget *controlPanel = createControlPanel();
    controlPanel->setMaximumWidth(300);
    
    // 创建右侧主要内容区域
    QTabWidget *tabWidget = createMainTabs();
    
    // 使用分割器
    QSplitter *splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(controlPanel);
    splitter->addWidget(tabWidget);
    splitter->setSizes({250, 950});
    
    mainLayout->addWidget(splitter);
    
    std::cout << "UI setup completed" << std::endl;
}

QWidget* SimpleMainWindow::createControlPanel()
{
    QWidget *panel = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(panel);
    
    // 设备控制组
    QGroupBox *deviceGroup = new QGroupBox("设备控制");
    QVBoxLayout *deviceLayout = new QVBoxLayout(deviceGroup);
    
    QPushButton *startBtn = new QPushButton("启动设备");
    startBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; padding: 8px; border-radius: 4px; }");
    connect(startBtn, &QPushButton::clicked, this, &SimpleMainWindow::onStartClicked);
    
    QPushButton *stopBtn = new QPushButton("停止设备");
    stopBtn->setStyleSheet("QPushButton { background-color: #f44336; color: white; padding: 8px; border-radius: 4px; }");
    connect(stopBtn, &QPushButton::clicked, this, &SimpleMainWindow::onStopClicked);
    
    QPushButton *emergencyBtn = new QPushButton("紧急停止");
    emergencyBtn->setStyleSheet("QPushButton { background-color: #FF9800; color: white; padding: 8px; border-radius: 4px; font-weight: bold; }");
    connect(emergencyBtn, &QPushButton::clicked, this, &SimpleMainWindow::onEmergencyClicked);
    
    deviceLayout->addWidget(startBtn);
    deviceLayout->addWidget(stopBtn);
    deviceLayout->addWidget(emergencyBtn);
    
    // 状态显示组
    QGroupBox *statusGroup = new QGroupBox("设备状态");
    QVBoxLayout *statusLayout = new QVBoxLayout(statusGroup);
    
    m_statusLabel = new QLabel("状态: 待机");
    m_statusLabel->setStyleSheet("color: blue; font-weight: bold;");
    
    m_progressBar = new QProgressBar;
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addWidget(new QLabel("进度:"));
    statusLayout->addWidget(m_progressBar);
    
    // 参数设置组
    QGroupBox *paramGroup = new QGroupBox("参数设置");
    QVBoxLayout *paramLayout = new QVBoxLayout(paramGroup);
    
    paramLayout->addWidget(new QLabel("胶量 (μL):"));
    QDoubleSpinBox *volumeSpinBox = new QDoubleSpinBox;
    volumeSpinBox->setRange(0.1, 100.0);
    volumeSpinBox->setValue(1.0);
    volumeSpinBox->setSuffix(" μL");
    paramLayout->addWidget(volumeSpinBox);
    
    paramLayout->addWidget(new QLabel("压力 (Bar):"));
    QDoubleSpinBox *pressureSpinBox = new QDoubleSpinBox;
    pressureSpinBox->setRange(0.1, 10.0);
    pressureSpinBox->setValue(2.0);
    pressureSpinBox->setSuffix(" Bar");
    paramLayout->addWidget(pressureSpinBox);
    
    paramLayout->addWidget(new QLabel("温度 (°C):"));
    QDoubleSpinBox *tempSpinBox = new QDoubleSpinBox;
    tempSpinBox->setRange(15.0, 60.0);
    tempSpinBox->setValue(25.0);
    tempSpinBox->setSuffix(" °C");
    paramLayout->addWidget(tempSpinBox);
    
    layout->addWidget(deviceGroup);
    layout->addWidget(statusGroup);
    layout->addWidget(paramGroup);
    layout->addStretch();
    
    return panel;
}

QTabWidget* SimpleMainWindow::createMainTabs()
{
    QTabWidget *tabWidget = new QTabWidget;
    
    // 数据监控页面
    tabWidget->addTab(createMonitorTab(), "数据监控");
    
    // 报警系统页面
    tabWidget->addTab(createAlarmTab(), "报警系统");
    
    // 数据记录页面
    tabWidget->addTab(createDataTab(), "数据记录");
    
    // 设置页面
    tabWidget->addTab(createSettingsTab(), "系统设置");
    
    return tabWidget;
}

QWidget* SimpleMainWindow::createMonitorTab()
{
    QWidget *widget = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(widget);
    
    // 创建数据表格
    m_dataTable = new QTableWidget;
    m_dataTable->setColumnCount(6);
    m_dataTable->setHorizontalHeaderLabels({"时间", "温度", "压力", "胶量", "位置X", "位置Y"});
    m_dataTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    
    layout->addWidget(new QLabel("实时数据监控"));
    layout->addWidget(m_dataTable);
    
    return widget;
}

QWidget* SimpleMainWindow::createAlarmTab()
{
    QWidget *widget = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(widget);
    
    m_logText = new QTextEdit;
    m_logText->setReadOnly(true);
    m_logText->append("系统日志:");
    m_logText->append("2025-07-06 19:55:00 - 系统启动");
    m_logText->append("2025-07-06 19:55:01 - 设备连接正常");
    m_logText->append("2025-07-06 19:55:02 - 传感器初始化完成");
    
    layout->addWidget(new QLabel("系统日志和报警信息"));
    layout->addWidget(m_logText);
    
    return widget;
}

QWidget* SimpleMainWindow::createDataTab()
{
    QWidget *widget = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(widget);
    
    layout->addWidget(new QLabel("数据记录功能"));
    layout->addWidget(new QLabel("(简化版 - 功能待实现)"));
    
    return widget;
}

QWidget* SimpleMainWindow::createSettingsTab()
{
    QWidget *widget = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(widget);
    
    layout->addWidget(new QLabel("系统设置"));
    layout->addWidget(new QLabel("(简化版 - 功能待实现)"));
    
    return widget;
}

void SimpleMainWindow::setupMenus()
{
    QMenuBar *menuBar = this->menuBar();
    
    // 文件菜单
    QMenu *fileMenu = menuBar->addMenu("文件");
    QAction *exitAction = fileMenu->addAction("退出");
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    
    // 设置菜单
    QMenu *settingsMenu = menuBar->addMenu("设置");
    QAction *settingsAction = settingsMenu->addAction("系统设置");
    connect(settingsAction, &QAction::triggered, this, &SimpleMainWindow::onSettingsClicked);
    
    // 帮助菜单
    QMenu *helpMenu = menuBar->addMenu("帮助");
    QAction *aboutAction = helpMenu->addAction("关于");
    connect(aboutAction, &QAction::triggered, this, &SimpleMainWindow::onAboutClicked);
}

void SimpleMainWindow::setupStatusBar()
{
    statusBar()->showMessage("就绪");
}

void SimpleMainWindow::applyModernStyle()
{
    // 应用现代化样式
    qApp->setStyle(QStyleFactory::create("Fusion"));
    
    // 设置样式表
    QString styleSheet = R"(
        QMainWindow {
            background-color: #f0f0f0;
        }
        QGroupBox {
            font-weight: bold;
            border: 2px solid #cccccc;
            border-radius: 5px;
            margin-top: 1ex;
            padding-top: 10px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px 0 5px;
        }
        QTabWidget::pane {
            border: 1px solid #cccccc;
            background-color: white;
        }
        QTabBar::tab {
            background-color: #e1e1e1;
            padding: 8px 16px;
            margin-right: 2px;
        }
        QTabBar::tab:selected {
            background-color: white;
        }
    )";
    
    setStyleSheet(styleSheet);
}

void SimpleMainWindow::addRandomData()
{
    if (!m_dataTable) return;
    
    int row = m_dataTable->rowCount();
    m_dataTable->insertRow(row);
    
    QDateTime now = QDateTime::currentDateTime();
    m_dataTable->setItem(row, 0, new QTableWidgetItem(now.toString("hh:mm:ss")));
    m_dataTable->setItem(row, 1, new QTableWidgetItem(QString::number(20 + (rand() % 10))));
    m_dataTable->setItem(row, 2, new QTableWidgetItem(QString::number(1.5 + (rand() % 50) / 10.0)));
    m_dataTable->setItem(row, 3, new QTableWidgetItem(QString::number(0.5 + (rand() % 20) / 10.0)));
    m_dataTable->setItem(row, 4, new QTableWidgetItem(QString::number(100 + (rand() % 200))));
    m_dataTable->setItem(row, 5, new QTableWidgetItem(QString::number(50 + (rand() % 150))));
    
    // 保持最多100行数据
    if (m_dataTable->rowCount() > 100) {
        m_dataTable->removeRow(0);
    }
} 