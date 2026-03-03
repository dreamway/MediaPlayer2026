# WZMediaPlayer 快速编译脚本 (修复版)
# 用于编译 WZMediaPlayer 项目

param(
    [switch]$Clean,
    [switch]$Verbose,
    [string]$Configuration = "Debug"
)

# 设置错误处理
$ErrorActionPreference = "Stop"

# 获取脚本所在目录
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectDir = $ScriptDir
$ProjectFile = Join-Path $ProjectDir "WZMediaPlay.vcxproj"

# VS安装路径
$VSPath = "D:\Program Files\Microsoft Visual Studio\2022\Community"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "WZMediaPlayer 编译脚本" -ForegroundColor Cyan
Write-Host "项目目录: $ProjectDir" -ForegroundColor Cyan
Write-Host "配置: $Configuration" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

# 构建命令
$cmdCommand = "cd /d `"$ProjectDir`" && "
if ($Clean) {
    $cmdCommand += "msbuild `"$ProjectFile`" /p:Configuration=$Configuration /p:Platform=x64 /t:Clean && "
}
$cmdCommand += "msbuild `"$ProjectFile`" /p:Configuration=$Configuration /p:Platform=x64 /t:Build /verbosity:$($Verbose ? "normal" : "minimal")"

try {
    # 执行编译
    Write-Host "正在设置VS环境并编译..." -ForegroundColor Green

    $startTime = Get-Date

    # 使用 cmd.exe 执行 VS 环境设置和 MSBuild
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo.FileName = "cmd.exe"
    $process.StartInfo.Arguments = "/c `"`"$VSPath\VC\Auxiliary\Build\vcvars64.bat`" && $cmdCommand`""
    $process.StartInfo.UseShellExecute = $false
    $process.StartInfo.RedirectStandardOutput = $true
    $process.StartInfo.RedirectStandardError = $true
    $process.Start() | Out-Null

    # 读取输出
    $output = $process.StandardOutput.ReadToEnd()
    $errorOutput = $process.StandardError.ReadToEnd()
    $process.WaitForExit()

    $endTime = Get-Date
    $duration = $endTime - $startTime

    # 显示输出（限制行数避免过多）
    $lines = $output -split "`r`n"
    foreach ($line in $lines) {
        Write-Host $line
    }

    if ($errorOutput) {
        Write-Host $errorOutput -ForegroundColor Red
    }

    Write-Host ("`n编译耗时: {0:mm\:ss\.fff}" -f $duration) -ForegroundColor Yellow

    # 返回退出码
    exit $process.ExitCode

} catch {
    Write-Error "编译过程中发生错误: $_"
    exit 1
}
