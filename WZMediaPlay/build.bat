@echo off
REM WZMediaPlayer 子目录快速编译脚本
REM 优先使用 CMake 生成的 build\WZMediaPlayer.sln；若无则尝试本目录 vcxproj

setlocal
set "ROOT=%~dp0.."
set "SLN=%ROOT%\build\WZMediaPlayer.sln"
set "CONFIG=Release"
if not "%~1"=="" set "CONFIG=%~1"

REM 优先使用仓库根目录下 build 中的 sln（CMake 生成）
if exist "%SLN%" (
    echo 使用 CMake 解决方案: %SLN%
    call "%ROOT%\build.bat" %CONFIG%
    exit /b %ERRORLEVEL%
)

REM 回退：本目录 vcxproj（需已生成 vcxproj 的旧流程）
if exist "%~dp0WZMediaPlay.vcxproj" (
    echo 使用本目录项目: WZMediaPlay.vcxproj
    call "D:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" 2>nul
    if %ERRORLEVEL% NEQ 0 call "D:\Program Files\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" 2>nul
    MSBuild.exe "%~dp0WZMediaPlay.vcxproj" /p:Configuration=%CONFIG% /p:Platform=x64 /t:Build /verbosity:minimal
    exit /b %ERRORLEVEL%
)

echo 错误: 未找到可构建项。
echo 请先在仓库根目录执行: cmake -B build -G "Visual Studio 17 2022" -A x64
exit /b 1
