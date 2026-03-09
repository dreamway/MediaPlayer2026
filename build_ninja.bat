@echo off
REM 使用 Ninja 构建（需已配置 CMake：cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release）
REM 用法: build_ninja.bat [Release|Debug]

setlocal
set "BUILD_TYPE=Release"
if not "%~1"=="" set "BUILD_TYPE=%~1"
set "BD=%~dp0build"

if not exist "%BD%\build.ninja" (
    echo 未找到 build.ninja，正在配置 CMake...
    cmake -B "%BD%" -G Ninja -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
    if %ERRORLEVEL% NEQ 0 exit /b 1
)

echo ========================================
echo Ninja 构建 [%BUILD_TYPE%]
echo ========================================
ninja -C "%BD%"
set "R=%ERRORLEVEL%"
if %R% EQU 0 echo 输出: %BD%\%BUILD_TYPE%\WZMediaPlayer.exe
exit /b %R%
