@echo off
REM WZMediaPlayer 快速编译脚本
REM 用于验证新增代码是否有语法错误

echo ========================================
echo WZMediaPlayer 快速编译脚本
echo ========================================

REM 设置VS环境
call "D:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if %ERRORLEVEL% NEQ 0 (
    echo 错误：无法设置VS环境
    exit /b 1
)

REM 检查项目文件是否存在
if not exist "WZMediaPlay.vcxproj" (
    echo 错误：找不到项目文件 WZMediaPlay.vcxproj
    exit /b 1
)

echo 正在编译项目...
echo.

REM 记录开始时间
for /f "tokens=1-4 delims=:.," %%a in ("%time%") do (
    set start_hour=%%a
    set start_minute=%%b
    set start_second=%%c
    set start_centisecond=%%d
)

REM 编译项目
MSBuild.exe WZMediaPlay.vcxproj /p:Configuration=Debug /p:Platform=x64 /t:Build /verbosity:minimal
set build_result=%ERRORLEVEL%

REM 计算耗时
for /f "tokens=1-4 delims=:.," %%a in ("%time%") do (
    set end_hour=%%a
    set end_minute=%%b
    set end_second=%%c
    set end_centisecond=%%d
)

REM 显示结果
echo.
if %build_result% EQU 0 (
    echo ========================================
    echo 编译成功!
    echo ========================================
) else (
    echo ========================================
    echo 编译失败!
    echo ========================================
)

echo.
echo 退出代码: %build_result%
exit /b %build_result%