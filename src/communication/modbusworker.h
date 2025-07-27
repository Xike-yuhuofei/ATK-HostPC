#pragma once

#include <QObject>

class ModbusWorker : public QObject
{
    Q_OBJECT

public:
    explicit ModbusWorker(QObject* parent = nullptr);
    ~ModbusWorker();

private:
    // 占位符实现
}; 