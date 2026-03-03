@echo off
chcp 65001 >nul
echo ====================================
echo WZMediaPlayer 自动化测试
echo ====================================
echo.

REM 检查Python是否安装
python --version >nul 2>&1
if errorlevel 1 (
    echo [错误] 未找到Python，请先安装Python 3.7+
    pause
    exit /b 1
)

REM 检查pywinauto是否安装
python -c "import pywinauto" >nul 2>&1
if errorlevel 1 (
    echo [信息] pywinauto未安装，正在安装...
    pip install pywinauto
    if errorlevel 1 (
        echo [错误] pywinauto安装失败
        pause
        exit /b 1
    )
)

REM 运行测试
echo [信息] 开始运行测试...
echo.
python main.py

REM 检查退出码
if errorlevel 1 (
    echo.
    echo [结果] 测试失败
) else (
    echo.
    echo [结果] 测试通过
)

echo.
pause
