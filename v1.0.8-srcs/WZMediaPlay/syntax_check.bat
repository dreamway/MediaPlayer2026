@echo off
REM WZMediaPlayer 语法检查脚本
REM 只检查新增的core模块语法，不依赖Qt环境

echo ========================================
echo WZMediaPlayer 语法检查
echo 只检查新增的core模块
echo ========================================

REM 设置VS环境
call "D:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if %ERRORLEVEL% NEQ 0 (
    echo 错误：无法设置VS环境
    exit /b 1
)

echo 正在检查语法...

REM 检查新增的core文件
set "CORE_FILES=core\Packet.cpp core\Frame.cpp core\StreamInfo.cpp core\PacketBuffer.cpp core\Demuxer.cpp core\DemuxerThr.cpp core\AVThread.cpp core\Decoder.cpp core\VideoThr.cpp core\AudioThr.cpp core\VideoWriter.cpp core\PlayClass.cpp core\StereoRenderer.cpp core\FFmpegViewAdapter.cpp"

set syntax_errors=0

for %%f in (%CORE_FILES%) do (
    if exist "%%f" (
        echo 检查文件: %%f
        REM 使用cl.exe进行语法检查（不生成目标文件）
        REM 添加Qt包含路径（如果存在）
        if exist "D:\DevTools\Qt\6.6.3\msvc2019_64\include" (
            cl.exe /Zs /I. /Icore /ID:\DevTools\Qt\6.6.3\msvc2019_64\include /ID:\DevTools\Qt\6.6.3\msvc2019_64\include\QtCore /std:c++17 /EHsc /MD /DUNICODE /D_UNICODE "%%f" 2>nul
        ) else (
            cl.exe /Zs /I. /Icore /std:c++17 /EHsc /MD /DUNICODE /D_UNICODE "%%f" 2>nul
        )
        if !ERRORLEVEL! NEQ 0 (
            echo   [错误] %%f 语法检查失败
            set /a syntax_errors+=1
        ) else (
            echo   [通过] %%f 语法正确
        )
    ) else (
        echo   [错误] 文件不存在: %%f
        set /a syntax_errors+=1
    )
)

echo.
echo ========================================
if %syntax_errors% EQU 0 (
    echo 语法检查通过!
    echo 所有新增文件语法正确
) else (
    echo 语法检查失败!
    echo 发现 %syntax_errors% 个语法错误
)
echo ========================================

exit /b %syntax_errors%