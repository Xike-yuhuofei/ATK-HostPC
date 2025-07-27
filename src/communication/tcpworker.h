#pragma once

#include <QObject>

class TcpWorker : public QObject
{
    Q_OBJECT

public:
    explicit TcpWorker(QObject* parent = nullptr);
    ~TcpWorker();

private:
    // 占位符实现
}; 