#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QTranslator>
#include <QLocale>
#include <QDebug>
#include <QMessageBox>
#include <exception>
#include <stdexcept>

#include "mainwindow.h"
#include "logger/logmanager.h"
#include "config/configmanager.h"
#include "core/errorhandler.h"
#include "core/performanceconfigmanager.h"

/**
 * @brief 全局异常处理函数
 * 
 * 捕获未处理的异常并记录到错误处理系统
 */
void handleGlobalException()
{
    try {
        std::rethrow_exception(std::current_exception());
    } catch (const std::exception& e) {
        QString errorMsg = QString("Unhandled exception: %1").arg(e.what());
        qCritical() << errorMsg;
        
        ErrorHandler* errorHandler = ErrorHandler::getInstance();
        errorHandler->reportFatal(errorMsg, "Global");
        
        QMessageBox::critical(nullptr, "严重错误", 
            QString("应用程序遇到未处理的异常：%1\n\n程序将退出。").arg(e.what()));
    } catch (...) {
        QString errorMsg = "Unknown unhandled exception";
        qCritical() << errorMsg;
        
        ErrorHandler* errorHandler = ErrorHandler::getInstance();
        errorHandler->reportFatal(errorMsg, "Global");
        
        QMessageBox::critical(nullptr, "严重错误", 
            "应用程序遇到未知异常，程序将退出。");
    }
}

int main(int argc, char *argv[])
{
    // 设置全局异常处理
    std::set_terminate(handleGlobalException);
    
    int exitCode = -1;
    
    try {
        QApplication app(argc, argv);
        
        // 设置应用程序信息
        app.setApplicationName("IndustrialHostPC");
        app.setApplicationVersion("1.0.0");
        app.setOrganizationName("Industrial Solutions");
        app.setOrganizationDomain("industrial-solutions.com");
        
        // 设置应用程序目录
        QDir::setCurrent(QApplication::applicationDirPath());
        
        // 创建必要的目录
        QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (!QDir().mkpath(dataDir)) {
            throw std::runtime_error("Failed to create application data directory");
        }
        if (!QDir().mkpath(dataDir + "/logs")) {
            throw std::runtime_error("Failed to create logs directory");
        }
        if (!QDir().mkpath(dataDir + "/config")) {
            throw std::runtime_error("Failed to create config directory");
        }
        if (!QDir().mkpath(dataDir + "/data")) {
            throw std::runtime_error("Failed to create data directory");
        }
        
        // 初始化核心组件
        try {
            // 初始化日志管理器
            LogManager* logManager = LogManager::getInstance();
            if (!logManager) {
                throw std::runtime_error("Failed to initialize LogManager");
            }
            qDebug() << "LogManager initialized successfully";
            
            // 初始化错误处理器
            ErrorHandler* errorHandler = ErrorHandler::getInstance();
            if (!errorHandler) {
                throw std::runtime_error("Failed to initialize ErrorHandler");
            }
            qDebug() << "ErrorHandler initialized successfully";
            
            // 初始化配置管理器
            ConfigManager* configManager = ConfigManager::getInstance();
            if (!configManager) {
                throw std::runtime_error("Failed to initialize ConfigManager");
            }
            qDebug() << "ConfigManager initialized successfully";
            
            // 初始化性能配置管理器
            PerformanceConfigManager* perfManager = new PerformanceConfigManager();
            QString perfConfigPath = QApplication::applicationDirPath() + "/config/performance_config.json";
            if (perfManager->loadConfiguration(perfConfigPath)) {
                perfManager->startMonitoring();
                qDebug() << "PerformanceConfigManager initialized and monitoring started";
            } else {
                qWarning() << "Failed to load performance configuration, using defaults";
            }
            
        } catch (const std::exception& e) {
            QString errorMsg = QString("Core component initialization failed: %1").arg(e.what());
            qCritical() << errorMsg;
            QMessageBox::critical(nullptr, "初始化错误", 
                QString("核心组件初始化失败：%1\n\n程序将退出。").arg(e.what()));
            return -1;
        }
        
        qDebug() << "Application started successfully";
        
        // 创建主窗口
        MainWindow* window = nullptr;
        try {
            window = new MainWindow();
            if (!window) {
                throw std::runtime_error("Failed to create MainWindow");
            }
            
            // 初始化应用程序
            window->initializeApplication();
            window->show();
            
            qDebug() << "Main window created and shown successfully";
            
        } catch (const std::exception& e) {
            QString errorMsg = QString("MainWindow creation failed: %1").arg(e.what());
            qCritical() << errorMsg;
            
            ErrorHandler::getInstance()->reportFatal(errorMsg, "MainWindow");
            
            QMessageBox::critical(nullptr, "窗口创建错误", 
                QString("主窗口创建失败：%1\n\n程序将退出。").arg(e.what()));
            
            if (window) {
                delete window;
            }
            return -2;
        }
        
        // 运行应用程序事件循环
        exitCode = app.exec();
        
        // 清理资源
        if (window) {
            delete window;
        }
        
        qDebug() << "Application exited with code:" << exitCode;
        
    } catch (const std::exception& e) {
        QString errorMsg = QString("Application startup failed: %1").arg(e.what());
        qCritical() << errorMsg;
        
        // 尝试初始化错误处理器来记录错误
        try {
            ErrorHandler::getInstance()->reportFatal(errorMsg, "Startup");
        } catch (...) {
            // 如果错误处理器也失败了，只能输出到控制台
            qCritical() << "ErrorHandler also failed during startup error handling";
        }
        
        QMessageBox::critical(nullptr, "启动错误", 
            QString("应用程序启动失败：%1\n\n程序将退出。").arg(e.what()));
        
        exitCode = -3;
        
    } catch (...) {
        qCritical() << "Unknown exception during application startup";
        
        QMessageBox::critical(nullptr, "启动错误", 
            "应用程序启动时遇到未知错误，程序将退出。");
        
        exitCode = -4;
    }
    
    return exitCode;
}