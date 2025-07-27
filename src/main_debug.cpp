#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include <iostream>

// 逐步测试各个模块
#include "logger/logmanager.h"
#include "config/configmanager.h"
#include "communication/serialcommunication.h"
//#include "data/databasemanager.h"
//#include "mainwindow.h"

// 创建一个简单的测试窗口
class SimpleTestWindow : public QMainWindow
{
    Q_OBJECT

public:
    SimpleTestWindow(QWidget* parent = nullptr) : QMainWindow(parent)
    {
        setWindowTitle("调试测试窗口 - 通信组件测试");
        setMinimumSize(600, 500);
        
        // 创建中央窗口
        QWidget* centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);
        
        // 创建布局
        QVBoxLayout* layout = new QVBoxLayout(centralWidget);
        
        // 添加标签
        QLabel* label1 = new QLabel("✓ Qt应用程序初始化成功", this);
        QLabel* label2 = new QLabel("✓ LogManager初始化成功", this);
        QLabel* label3 = new QLabel("✓ ConfigManager初始化成功", this);
        QLabel* label4 = new QLabel("✓ SerialCommunication初始化成功", this);
        QLabel* label5 = new QLabel("✓ 测试窗口创建成功", this);
        
        layout->addWidget(label1);
        layout->addWidget(label2);
        layout->addWidget(label3);
        layout->addWidget(label4);
        layout->addWidget(label5);
        
        // 添加样式
        setStyleSheet("QLabel { font-size: 14px; margin: 10px; }");
    }
};

int main(int argc, char *argv[])
{
    std::cout << "Starting debug main with communication support..." << std::endl;
    
    QApplication app(argc, argv);
    
    // 设置应用程序信息
    app.setApplicationName("IndustrialHostPC");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("Industrial Solutions");
    app.setOrganizationDomain("industrial-solutions.com");
    
    std::cout << "Application info set" << std::endl;
    
    // 设置应用程序目录
    QDir::setCurrent(QApplication::applicationDirPath());
    
    std::cout << "Directory set" << std::endl;
    
    // 创建必要的目录
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    QDir().mkpath(dataDir + "/logs");
    QDir().mkpath(dataDir + "/config");
    QDir().mkpath(dataDir + "/data");
    
    std::cout << "Directories created" << std::endl;
    
    // 测试点1：基本Qt初始化
    std::cout << "Test point 1: Basic Qt initialization complete" << std::endl;
    
    // 测试点2：初始化日志管理器
    std::cout << "Test point 2: Initializing LogManager..." << std::endl;
    try {
        LogManager::getInstance();
        std::cout << "LogManager initialized successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "LogManager initialization failed: " << e.what() << std::endl;
        return -1;
    } catch (...) {
        std::cout << "LogManager initialization failed with unknown exception" << std::endl;
        return -1;
    }
    
    // 测试点3：初始化配置管理器
    std::cout << "Test point 3: Initializing ConfigManager..." << std::endl;
    try {
        ConfigManager::getInstance();
        std::cout << "ConfigManager initialized successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "ConfigManager initialization failed: " << e.what() << std::endl;
        return -1;
    } catch (...) {
        std::cout << "ConfigManager initialization failed with unknown exception" << std::endl;
        return -1;
    }
    
    // 测试点4：创建串口通信对象
    std::cout << "Test point 4: Creating SerialCommunication..." << std::endl;
    try {
        SerialCommunication* serialComm = new SerialCommunication();
        std::cout << "SerialCommunication created successfully" << std::endl;
        delete serialComm;
        std::cout << "SerialCommunication cleaned up successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "SerialCommunication creation failed: " << e.what() << std::endl;
        return -1;
    } catch (...) {
        std::cout << "SerialCommunication creation failed with unknown exception" << std::endl;
        return -1;
    }
    
    // 测试点5：创建测试窗口
    std::cout << "Test point 5: Creating SimpleTestWindow..." << std::endl;
    try {
        SimpleTestWindow window;
        std::cout << "SimpleTestWindow created successfully" << std::endl;
        window.show();
        std::cout << "SimpleTestWindow shown successfully" << std::endl;
        
        return app.exec();
    } catch (const std::exception& e) {
        std::cout << "SimpleTestWindow creation failed: " << e.what() << std::endl;
        return -1;
    } catch (...) {
        std::cout << "SimpleTestWindow creation failed with unknown exception" << std::endl;
        return -1;
    }
}

#include "main_debug.moc" 