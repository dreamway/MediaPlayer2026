#!/bin/bash

# WZMediaPlayer Mac 打包脚本
# 此脚本用于在 macOS 上打包 WZMediaPlayer 及其依赖库

set -e  # 遇到错误立即退出

# 颜色输出
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# 获取脚本所在目录的父目录（项目根目录）
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_DIR}/build"
DIST_DIR="${PROJECT_DIR}/dist"
APP_NAME="WZMediaPlayer"

echo -e "${GREEN}======================================${NC}"
echo -e "${GREEN}  WZMediaPlayer Mac 打包脚本${NC}"
echo -e "${GREEN}======================================${NC}"
echo ""

# 清理旧的打包目录
echo -e "${YELLOW}[1/6] 清理旧的打包目录...${NC}"
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR"
echo "清理完成: $DIST_DIR"

# 编译程序
echo ""
echo -e "${YELLOW}[2/6] 编译程序...${NC}"
cd "$BUILD_DIR"

# 检查可执行文件或 .app bundle 是否存在
if [ ! -f "$APP_NAME" ] && [ ! -d "${APP_NAME}.app" ]; then
    echo -e "${RED}错误: 可执行文件不存在，请先运行 cmake .. && make WZMediaPlayer${NC}"
    exit 1
fi
echo "编译完成"

# 复制可执行文件到打包目录
echo ""
echo -e "${YELLOW}[3/6] 复制 .app bundle...${NC}"

# 检查是 .app bundle 还是单独的可执行文件
if [ -f "$BUILD_DIR/${APP_NAME}.app/Contents/MacOS/$APP_NAME" ]; then
    # 复制整个 .app bundle
    cp -R "$BUILD_DIR/${APP_NAME}.app" "$DIST_DIR/"
    echo ".app bundle 已复制"
else
    # 复制单独的可执行文件
    cp "$BUILD_DIR/$APP_NAME" "$DIST_DIR/"
    chmod +x "$DIST_DIR/$APP_NAME"
    echo "可执行文件已复制"
fi

# 复制资源文件
echo ""
echo -e "${YELLOW}[4/6] 复制资源文件...${NC}"

# 复制配置文件
mkdir -p "$DIST_DIR/config"
cp "$PROJECT_DIR/WZMediaPlay/config/"* "$DIST_DIR/config/"
echo "配置文件已复制"

# 复制 logo 资源
mkdir -p "$DIST_DIR/Resources/logo"
cp -r "$PROJECT_DIR/WZMediaPlay/Resources/logo/"* "$DIST_DIR/Resources/logo/"
echo "Logo 资源已复制"

# 创建截图目录
mkdir -p "$DIST_DIR/Snapshots"
echo "截图目录已创建"

# 使用 macdeployqt 打包 Qt 框架
echo ""
echo -e "${YELLOW}[5/6] 使用 macdeployqt 打包 Qt 框架...${NC}"
cd "$DIST_DIR"

# 检查是 .app bundle 还是单独的可执行文件
if [ -f "${APP_NAME}.app/Contents/MacOS/${APP_NAME}" ]; then
    /opt/homebrew/bin/macdeployqt "${APP_NAME}.app" -verbose=2 -always-overwrite
else
    /opt/homebrew/bin/macdeployqt "$APP_NAME" -verbose=2 -always-overwrite
fi

echo "Qt 框架打包完成"

# 验证依赖库
echo ""
echo -e "${YELLOW}[6/6] 验证依赖库...${NC}"

# 确定要检查的可执行文件路径
if [ -f "${APP_NAME}.app/Contents/MacOS/${APP_NAME}" ]; then
    EXECUTABLE="${APP_NAME}.app/Contents/MacOS/${APP_NAME}"
    RUN_CMD="open ${APP_NAME}.app"
else
    EXECUTABLE="$APP_NAME"
    RUN_CMD="./$APP_NAME"
fi

echo "主要依赖库:"
otool -L "$DIST_DIR/$EXECUTABLE" | grep -E "opt/homebrew|libffmpeg|libopenal|libfmt|libspdlog|libGLEW" || echo "所有依赖库已正确链接"

echo ""
echo -e "${GREEN}======================================${NC}"
echo -e "${GREEN}  打包完成！${NC}"
echo -e "${GREEN}======================================${NC}"
echo ""
echo "打包目录: $DIST_DIR"
echo ""
echo "运行程序:"
echo "  cd $DIST_DIR"
echo "  $RUN_CMD"
echo ""
echo "目录结构:"
ls -lh "$DIST_DIR/"
echo ""
echo -e "${GREEN}提示: 可以将整个 dist 目录压缩打包分发给其他 Mac 用户${NC}"
