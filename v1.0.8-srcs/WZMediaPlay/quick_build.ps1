# WZMediaPlayer 快速编译脚本
# 用于验证新增代码是否有语法错误

param(
    [switch]$Verbose,
    [string]$Configuration = "Debug"
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "WZMediaPlayer 快速编译" -ForegroundColor Cyan
Write-Host "配置: $Configuration" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

# 获取脚本所在目录
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectDir = $ScriptDir
$ProjectFile = Join-Path $ProjectDir "WZMediaPlay.vcxproj"

# VS安装路径
$VSPath = "D:\Program Files\Microsoft Visual Studio\2022\Community"
$VCVarsPath = Join-Path $VSPath "VC\Auxiliary\Build\vcvars64.bat"

# 检查项目文件
if (-not (Test-Path $ProjectFile)) {
    Write-Error "项目文件不存在: $ProjectFile"
    exit 1
}

# 检查VS环境
if (-not (Test-Path $VCVarsPath)) {
    Write-Error "未找到VS环境脚本: $VCVarsPath"
    exit 1
}

Write-Host "正在设置VS环境并编译..." -ForegroundColor Green

$startTime = Get-Date

try {
    # 构建MSBuild参数
    $msbuildArgs = @(
        "`"$ProjectFile`"",
        "/p:Configuration=$Configuration",
        "/p:Platform=x64",
        "/t:Build"
    )

    if ($Verbose) {
        $msbuildArgs += "/verbosity:normal"
    } else {
        $msbuildArgs += "/verbosity:minimal"
    }

    # 设置VS环境并运行MSBuild
    $argumentList = "/c `"call \`"$VCVarsPath\`"` && cd /d \`"$ProjectDir\`"` && MSBuild.exe $($msbuildArgs -join ' ')`""

    $process = Start-Process -FilePath "cmd.exe" -ArgumentList $argumentList -NoNewWindow -Wait -PassThru

    $endTime = Get-Date
    $duration = $endTime - $startTime

    Write-Host "`n编译耗时: $($duration.ToString('mm\:ss\.fff'))" -ForegroundColor Yellow

    if ($process.ExitCode -eq 0) {
        Write-Host "========================================" -ForegroundColor Green
        Write-Host "编译成功!" -ForegroundColor Green
        Write-Host "========================================" -ForegroundColor Green
    } else {
        Write-Host "========================================" -ForegroundColor Red
        Write-Host "编译失败!" -ForegroundColor Red
        Write-Host "========================================" -ForegroundColor Red
    }

    exit $process.ExitCode

} catch {
    Write-Error "编译过程中发生错误: $_"
    exit 1
}