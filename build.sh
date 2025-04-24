#!/bin/bash

# 默认值
BUILD_DIR="./product"
INTERNUCLEAR_MODULE="./internuclear.ko"
KERNEL_SOURCE_AARCH64="/home/bigdog/share/RK3568/rk356x_linux_release_v1.3.1_20221120/kernel"
KERNEL_SOURCE_LOONGARCH="/home/bigdog/share/Loongar/uisrc-lab-loongarch/sources/kernel"

# 检查传递的参数
if [ "$1" == "aarch64" ]; then
    export ARCH=arm64
    export CROSS_COMPILE=/home/bigdog/share/tools/gcc-arm-11.2-2022.02-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-
    KERNEL_SOURCE=$KERNEL_SOURCE_AARCH64
    CONFIG_OPTION="CONFIG_USE_AARCH64=y"
elif [ "$1" == "loongarch" ]; then
    export ARCH=loongarch
    export CROSS_COMPILE=/opt/loongson-gnu-toolchain-8.3.2k1000la-x86_64-loongarch64-linux-gnu-rc1.1a/bin/loongarch64-linux-gnu-
    KERNEL_SOURCE=$KERNEL_SOURCE_LOONGARCH
    CONFIG_OPTION="CONFIG_USE_LONGARCH=y"
else
    echo "Usage: $0 [aarch64|loongarch]"
    exit 1
fi

echo "Selected architecture: $ARCH"
echo "Using kernel source: $KERNEL_SOURCE"

# 清理之前的构建
echo "Cleaning previous build..."
make clean KERNEL_SOURCE=$KERNEL_SOURCE || { echo "Failed to clean build. Exiting."; exit 1; }
rm -rf "$BUILD_DIR" || { echo "Failed to remove $BUILD_DIR. Exiting."; exit 1; }

# 编译内核模块
echo "Building kernel modules..."
make $CONFIG_OPTION KERNEL_SOURCE=$KERNEL_SOURCE || { echo "Build failed. Exiting."; exit 1; }

# 创建目标目录
echo "Creating build directory: $BUILD_DIR"
mkdir -p "$BUILD_DIR" || { echo "Failed to create directory $BUILD_DIR. Exiting."; exit 1; }

# 复制生成的模块文件
echo "Copying kernel modules to $BUILD_DIR..."
cp "$INTERNUCLEAR_MODULE" "$BUILD_DIR" || { echo "Failed to copy $INTERNUCLEAR_MODULE. Exiting."; exit 1; }

echo "Build and copy completed successfully. Modules are in $BUILD_DIR."