#!/bin/bash

# 工业点胶设备上位机控制软件构建脚本
# 版本: 2.0.0
# 作者: Industrial Solutions

echo "=========================================="
echo "工业点胶设备上位机控制软件构建脚本"
echo "版本: 2.0.0"
echo "=========================================="

# 检查操作系统
OS=$(uname -s)
echo "检测到操作系统: $OS"

# 设置构建类型
BUILD_TYPE=${1:-Release}
echo "构建类型: $BUILD_TYPE"

# 创建构建目录
BUILD_DIR="build"
if [ -d "$BUILD_DIR" ]; then
    echo "清理旧的构建目录..."
    rm -rf "$BUILD_DIR"
fi

echo "创建构建目录: $BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# 检查Qt环境
echo "检查Qt环境..."
if ! command -v qmake &> /dev/null; then
    echo "错误: 未找到Qt环境，请确保Qt6已正确安装并添加到PATH"
    exit 1
fi

QT_VERSION=$(qmake -query QT_VERSION)
echo "Qt版本: $QT_VERSION"

# 检查CMake
echo "检查CMake..."
if ! command -v cmake &> /dev/null; then
    echo "错误: 未找到CMake，请先安装CMake"
    exit 1
fi

CMAKE_VERSION=$(cmake --version | head -n1 | cut -d' ' -f3)
echo "CMake版本: $CMAKE_VERSION"

# 配置项目
echo "配置项目..."
cmake_args="-DCMAKE_BUILD_TYPE=$BUILD_TYPE"

# 根据操作系统添加特定配置
case $OS in
    "Darwin")
        echo "配置macOS构建..."
        cmake_args="$cmake_args -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15"
        ;;
    "Linux")
        echo "配置Linux构建..."
        cmake_args="$cmake_args -DCMAKE_INSTALL_PREFIX=/usr/local"
        ;;
    "MINGW"*|"MSYS"*|"CYGWIN"*)
        echo "配置Windows构建..."
        cmake_args="$cmake_args -G \"MinGW Makefiles\""
        ;;
esac

echo "执行CMake配置: cmake $cmake_args .."
if ! cmake $cmake_args ..; then
    echo "错误: CMake配置失败"
    exit 1
fi

# 构建项目
echo "开始构建项目..."
CPU_COUNT=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
echo "使用 $CPU_COUNT 个CPU核心进行并行构建"

if ! cmake --build . --config $BUILD_TYPE -j $CPU_COUNT; then
    echo "错误: 项目构建失败"
    exit 1
fi

echo "构建完成！"

# 显示构建结果
echo "=========================================="
echo "构建结果:"
if [ "$OS" = "Darwin" ]; then
    EXECUTABLE="GlueDispensePC.app"
elif [ "$OS" = "Linux" ]; then
    EXECUTABLE="GlueDispensePC"
else
    EXECUTABLE="GlueDispensePC.exe"
fi

if [ -f "$EXECUTABLE" ] || [ -d "$EXECUTABLE" ]; then
    echo "✓ 可执行文件已生成: $EXECUTABLE"
    
    # 显示文件信息
    if [ -f "$EXECUTABLE" ]; then
        FILE_SIZE=$(ls -lh "$EXECUTABLE" | awk '{print $5}')
        echo "  文件大小: $FILE_SIZE"
    fi
    
    echo "  构建类型: $BUILD_TYPE"
    echo "  构建时间: $(date)"
else
    echo "✗ 未找到可执行文件"
    exit 1
fi

echo "=========================================="

# 可选：运行测试
if [ "$2" = "test" ]; then
    echo "运行测试..."
    if ! ctest --output-on-failure; then
        echo "警告: 测试失败"
    else
        echo "✓ 所有测试通过"
    fi
fi

# 可选：创建安装包
if [ "$2" = "package" ] || [ "$3" = "package" ]; then
    echo "创建安装包..."
    if ! cpack; then
        echo "警告: 安装包创建失败"
    else
        echo "✓ 安装包创建成功"
        ls -la *.deb *.rpm *.dmg *.exe 2>/dev/null || echo "  安装包文件已生成"
    fi
fi

echo "构建脚本执行完成！"
echo "使用方法:"
echo "  ./build.sh [Debug|Release] [test] [package]"
echo "  示例: ./build.sh Release test package" 