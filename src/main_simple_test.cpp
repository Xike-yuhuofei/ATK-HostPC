#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include <iostream>

#include "mainwindow_simple.h"
#include "logger/logmanager.h"
#include "config/configmanager.h"

int main(int argc, char *argv[])
{
    std::cout << "Starting simplified MainWindow test..." << std::endl;
    
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
    
    // 测试点4：创建简化版主窗口
    std::cout << "Test point 4: Creating SimpleMainWindow..." << std::endl;
    try {
        SimpleMainWindow window;
        std::cout << "SimpleMainWindow created successfully" << std::endl;
        window.show();
        std::cout << "SimpleMainWindow shown successfully" << std::endl;
        
        return app.exec();
    } catch (const std::exception& e) {
        std::cout << "SimpleMainWindow creation failed: " << e.what() << std::endl;
        return -1;
    } catch (...) {
        std::cout << "SimpleMainWindow creation failed with unknown exception" << std::endl;
        return -1;
    }
} 