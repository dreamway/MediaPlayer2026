@echo off
REM WZMediaPlayer 构建脚本
REM 使用 CMake 生成的解决方案: build\WZMediaPlayer.sln
REM 用法: build.bat [Debug|Release]  默认 Release

setlocal
set "CONFIG=Release"
if not "%~1"=="" set "CONFIG=%~1"
set "SLN=%~dp0build\WZMediaPlayer.sln"

if not exist "%SLN%" (
    echo 错误: 未找到 %SLN%
    echo 请先运行 CMake 生成: cmake -B build -G "Visual Studio 17 2022" -A x64
    exit /b 1
)

REM 尝试加载 VS 环境（按常见路径顺序）
set "VCVARS="
if exist "D:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set "VCVARS=D:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
)
if exist "D:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
    set "VCVARS=D:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
)
if exist "D:\Program Files\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set "VCVARS=D:\Program Files\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
)
if exist "D:\DevTools\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat" (
    set "VCVARS=D:\DevTools\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"
)

if "%VCVARS%"=="" (
    echo 错误: 未找到 vcvars64.bat，请安装 Visual Studio 2019/2022 或修改本脚本中的路径
    exit /b 1
)

call "%VCVARS%" >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo 错误: 无法加载 VS 环境
    exit /b 1
)

echo ========================================
echo 构建 WZMediaPlayer [%CONFIG%]
echo 解决方案: %SLN%
echo ========================================
MSBuild.exe "%SLN%" /p:Configuration=%CONFIG% /p:Platform=x64 /m /v:minimal
set "BUILD_RESULT=%ERRORLEVEL%"
echo.
if %BUILD_RESULT% EQU 0 (
    echo 构建成功. 输出: build\%CONFIG%\WZMediaPlayer.exe
) else (
    echo 构建失败. 退出码: %BUILD_RESULT%
)
exit /b %BUILD_RESULT%
