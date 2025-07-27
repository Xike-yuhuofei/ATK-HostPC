#include <QApplication>
#include <QMainWindow>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QProgressBar>
#include <QStatusBar>
#include <QMenuBar>
#include <QToolBar>
#include <QTextEdit>
#include <QSplitter>
#include <QTimer>
#include <QDateTime>
#include <QIcon>
#include <QStyleFactory>
#include <QMessageBox>

class SimpleMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    SimpleMainWindow(QWidget *parent = nullptr) : QMainWindow(parent)
    {
        setWindowTitle("工业点胶设备上位机控制软件 v2.0.0 - 演示版");
        setMinimumSize(1200, 800);
        
        setupUI();
        setupMenus();
        setupStatusBar();
        
        // 应用现代化样式
        applyModernStyle();
        
        // 启动定时器更新状态
        QTimer *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &SimpleMainWindow::updateStatus);
        timer->start(1000);
    }

private slots:
    void updateStatus()
    {
        // 更新状态栏时间
        statusBar()->showMessage(QString("系统时间: %1").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")));
        
        // 模拟数据更新
        static int counter = 0;
        counter++;
        
        // 更新进度条
        m_progressBar->setValue((counter % 100));
        
        // 更新表格数据
        if (counter % 5 == 0) {
            addRandomData();
        }
    }
    
    void onStartClicked()
    {
        QMessageBox::information(this, "设备控制", "设备启动命令已发送！");
        m_statusLabel->setText("状态: 运行中");
        m_statusLabel->setStyleSheet("color: green; font-weight: bold;");
    }
    
    void onStopClicked()
    {
        QMessageBox::information(this, "设备控制", "设备停止命令已发送！");
        m_statusLabel->setText("状态: 已停止");
        m_statusLabel->setStyleSheet("color: red; font-weight: bold;");
    }
    
    void onEmergencyClicked()
    {
        QMessageBox::warning(this, "紧急停止", "紧急停止已触发！");
        m_statusLabel->setText("状态: 紧急停止");
        m_statusLabel->setStyleSheet("color: red; font-weight: bold; background-color: yellow;");
    }

private:
    void setupUI()
    {
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
    }
    
    QWidget* createControlPanel()
    {
        QWidget *panel = new QWidget;
        QVBoxLayout *layout = new QVBoxLayout(panel);
        
        // 设备控制组
        QGroupBox *deviceGroup = new QGroupBox("设备控制");
        QVBoxLayout *deviceLayout = new QVBoxLayout(deviceGroup);
        
        QPushButton *startBtn = new QPushButton("启动设备");
        startBtn->setIcon(QIcon(":/icons/start.png"));
        startBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; padding: 8px; border-radius: 4px; }");
        connect(startBtn, &QPushButton::clicked, this, &SimpleMainWindow::onStartClicked);
        
        QPushButton *stopBtn = new QPushButton("停止设备");
        stopBtn->setIcon(QIcon(":/icons/stop.png"));
        stopBtn->setStyleSheet("QPushButton { background-color: #f44336; color: white; padding: 8px; border-radius: 4px; }");
        connect(stopBtn, &QPushButton::clicked, this, &SimpleMainWindow::onStopClicked);
        
        QPushButton *emergencyBtn = new QPushButton("紧急停止");
        emergencyBtn->setIcon(QIcon(":/icons/emergency.png"));
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
    
    QTabWidget* createMainTabs()
    {
        QTabWidget *tabWidget = new QTabWidget;
        
        // 数据监控页面
        tabWidget->addTab(createMonitorTab(), "数据监控");
        
        // 报警系统页面
        tabWidget->addTab(createAlarmTab(), "报警系统");
        
        // 数据记录页面
        tabWidget->addTab(createRecordTab(), "数据记录");
        
        // 图表分析页面
        tabWidget->addTab(createChartTab(), "图表分析");
        
        return tabWidget;
    }
    
    QWidget* createMonitorTab()
    {
        QWidget *widget = new QWidget;
        QVBoxLayout *layout = new QVBoxLayout(widget);
        
        // 实时数据表格
        m_dataTable = new QTableWidget(0, 6);
        QStringList headers = {"时间", "X坐标", "Y坐标", "Z坐标", "胶量", "压力"};
        m_dataTable->setHorizontalHeaderLabels(headers);
        m_dataTable->horizontalHeader()->setStretchLastSection(true);
        
        layout->addWidget(new QLabel("实时数据监控"));
        layout->addWidget(m_dataTable);
        
        return widget;
    }
    
    QWidget* createAlarmTab()
    {
        QWidget *widget = new QWidget;
        QVBoxLayout *layout = new QVBoxLayout(widget);
        
        // 报警表格
        QTableWidget *alarmTable = new QTableWidget(0, 5);
        QStringList headers = {"时间", "报警类型", "报警级别", "报警信息", "状态"};
        alarmTable->setHorizontalHeaderLabels(headers);
        alarmTable->horizontalHeader()->setStretchLastSection(true);
        
        // 添加一些示例报警数据
        alarmTable->insertRow(0);
        alarmTable->setItem(0, 0, new QTableWidgetItem(QDateTime::currentDateTime().toString()));
        alarmTable->setItem(0, 1, new QTableWidgetItem("系统报警"));
        alarmTable->setItem(0, 2, new QTableWidgetItem("警告"));
        alarmTable->setItem(0, 3, new QTableWidgetItem("系统启动完成"));
        alarmTable->setItem(0, 4, new QTableWidgetItem("已确认"));
        
        layout->addWidget(new QLabel("报警记录"));
        layout->addWidget(alarmTable);
        
        return widget;
    }
    
    QWidget* createRecordTab()
    {
        QWidget *widget = new QWidget;
        QVBoxLayout *layout = new QVBoxLayout(widget);
        
        // 生产记录表格
        QTableWidget *recordTable = new QTableWidget(0, 6);
        QStringList headers = {"批次号", "产品类型", "开始时间", "结束时间", "总数量", "合格率"};
        recordTable->setHorizontalHeaderLabels(headers);
        recordTable->horizontalHeader()->setStretchLastSection(true);
        
        layout->addWidget(new QLabel("生产记录"));
        layout->addWidget(recordTable);
        
        return widget;
    }
    
    QWidget* createChartTab()
    {
        QWidget *widget = new QWidget;
        QVBoxLayout *layout = new QVBoxLayout(widget);
        
        // 简单的图表占位符
        QLabel *chartLabel = new QLabel("图表分析区域\n(集成Qt Charts实现实时数据可视化)");
        chartLabel->setAlignment(Qt::AlignCenter);
        chartLabel->setStyleSheet("border: 2px dashed #ccc; padding: 50px; font-size: 16px; color: #666;");
        chartLabel->setMinimumHeight(400);
        
        layout->addWidget(chartLabel);
        
        return widget;
    }
    
    void setupMenus()
    {
        // 文件菜单
        QMenu *fileMenu = menuBar()->addMenu("文件(&F)");
        fileMenu->addAction("新建项目", this, [this]() { QMessageBox::information(this, "提示", "新建项目功能"); });
        fileMenu->addAction("打开项目", this, [this]() { QMessageBox::information(this, "提示", "打开项目功能"); });
        fileMenu->addSeparator();
        fileMenu->addAction("退出", this, &QWidget::close);
        
        // 设备菜单
        QMenu *deviceMenu = menuBar()->addMenu("设备(&D)");
        deviceMenu->addAction("连接设备", this, [this]() { QMessageBox::information(this, "提示", "连接设备功能"); });
        deviceMenu->addAction("断开连接", this, [this]() { QMessageBox::information(this, "提示", "断开连接功能"); });
        
        // 工具菜单
        QMenu *toolsMenu = menuBar()->addMenu("工具(&T)");
        toolsMenu->addAction("参数配置", this, [this]() { QMessageBox::information(this, "提示", "参数配置功能"); });
        toolsMenu->addAction("系统设置", this, [this]() { QMessageBox::information(this, "提示", "系统设置功能"); });
        
        // 帮助菜单
        QMenu *helpMenu = menuBar()->addMenu("帮助(&H)");
        helpMenu->addAction("关于", this, [this]() { 
            QMessageBox::about(this, "关于", 
                "工业点胶设备上位机控制软件 v2.0.0\n\n"
                "这是一个现代化的工业自动化控制系统，\n"
                "具有完整的设备控制、数据监控、报警管理、\n"
                "数据记录和分析功能。\n\n"
                "技术栈：Qt6 + C++ + SQLite + Qt Charts");
        });
    }
    
    void setupStatusBar()
    {
        statusBar()->showMessage("系统就绪");
    }
    
    void applyModernStyle()
    {
        // 设置现代化的样式表
        QString styleSheet = R"(
            QMainWindow {
                background-color: #f5f5f5;
            }
            
            QTabWidget::pane {
                border: 1px solid #c0c0c0;
                background-color: white;
            }
            
            QTabWidget::tab-bar {
                left: 5px;
            }
            
            QTabBar::tab {
                background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                           stop: 0 #e1e1e1, stop: 0.4 #dddddd,
                                           stop: 0.5 #d8d8d8, stop: 1.0 #d3d3d3);
                border: 2px solid #c4c4c3;
                border-bottom-color: #c2c7cb;
                border-top-left-radius: 4px;
                border-top-right-radius: 4px;
                min-width: 8ex;
                padding: 8px;
                margin-right: 2px;
            }
            
            QTabBar::tab:selected {
                background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                           stop: 0 #fafafa, stop: 0.4 #f4f4f4,
                                           stop: 0.5 #e7e7e7, stop: 1.0 #fafafa);
                border-color: #9b9b9b;
                border-bottom-color: #c2c7cb;
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
            
            QTableWidget {
                gridline-color: #d0d0d0;
                background-color: white;
                alternate-background-color: #f8f8f8;
            }
            
            QTableWidget::item:selected {
                background-color: #3daee9;
                color: white;
            }
            
            QHeaderView::section {
                background-color: #e1e1e1;
                padding: 8px;
                border: 1px solid #d0d0d0;
                font-weight: bold;
            }
        )";
        
        setStyleSheet(styleSheet);
    }
    
    void addRandomData()
    {
        // 添加随机数据到监控表格
        int row = m_dataTable->rowCount();
        m_dataTable->insertRow(row);
        
        m_dataTable->setItem(row, 0, new QTableWidgetItem(QDateTime::currentDateTime().toString("hh:mm:ss")));
        m_dataTable->setItem(row, 1, new QTableWidgetItem(QString::number(10.0 + (rand() % 100) / 10.0, 'f', 1)));
        m_dataTable->setItem(row, 2, new QTableWidgetItem(QString::number(20.0 + (rand() % 100) / 10.0, 'f', 1)));
        m_dataTable->setItem(row, 3, new QTableWidgetItem(QString::number(5.0 + (rand() % 50) / 10.0, 'f', 1)));
        m_dataTable->setItem(row, 4, new QTableWidgetItem(QString::number(1.0 + (rand() % 20) / 10.0, 'f', 2)));
        m_dataTable->setItem(row, 5, new QTableWidgetItem(QString::number(2.0 + (rand() % 30) / 10.0, 'f', 1)));
        
        // 保持最新50行数据
        if (m_dataTable->rowCount() > 50) {
            m_dataTable->removeRow(0);
        }
        
        // 滚动到最新数据
        m_dataTable->scrollToBottom();
    }

private:
    QLabel *m_statusLabel;
    QProgressBar *m_progressBar;
    QTableWidget *m_dataTable;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // 设置应用程序信息
    app.setApplicationName("工业点胶设备上位机");
    app.setApplicationVersion("2.0.0");
    app.setOrganizationName("工业自动化");
    
    // 创建并显示主窗口
    SimpleMainWindow window;
    window.show();
    
    return app.exec();
}

#include "main_simple.moc" 