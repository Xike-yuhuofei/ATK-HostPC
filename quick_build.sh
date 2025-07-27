#!/bin/bash

# 快速编译脚本
echo "=========================================="
echo "工业点胶设备上位机 - 快速编译"
echo "=========================================="

# 设置构建类型
BUILD_TYPE=${1:-Debug}
echo "构建类型: $BUILD_TYPE"

# 进入项目目录
cd "$(dirname "$0")"

# 如果存在旧的构建目录，询问是否清理
if [ -d "build" ]; then
    echo "发现已存在的构建目录，正在增量编译..."
    cd build
else
    echo "创建新的构建目录..."
    mkdir -p build
    cd build
    
    # 配置项目
    echo "配置项目..."
    cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15 ..
fi

# 编译项目
echo "编译项目..."
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# 检查编译结果
if [ $? -eq 0 ]; then
    echo "✓ 编译成功！"
    if [ -f "GlueDispensePC" ]; then
        echo "可执行文件: build/GlueDispensePC"
        echo "运行命令: cd build && ./GlueDispensePC"
    fi
else
    echo "✗ 编译失败"
    exit 1
fi

echo "=========================================="
