# PowerShell 脚本：下载项目依赖库

Write-Host "开始下载项目依赖库..." -ForegroundColor Green

# 创建第三方库目录
$thirdPartyDir = "third_party"
if (-not (Test-Path $thirdPartyDir)) {
    New-Item -ItemType Directory -Path $thirdPartyDir | Out-Null
    Write-Host "创建目录: $thirdPartyDir" -ForegroundColor Yellow
}

# 检查 Git 是否安装
$gitInstalled = Get-Command git -ErrorAction SilentlyContinue
if (-not $gitInstalled) {
    Write-Host "错误: 未找到 Git。请先安装 Git。" -ForegroundColor Red
    exit 1
}

# 下载 ImGui
$imguiDir = Join-Path $thirdPartyDir "imgui"
if (Test-Path $imguiDir) {
    Write-Host "ImGui 已存在，跳过下载" -ForegroundColor Yellow
} else {
    Write-Host "正在下载 ImGui..." -ForegroundColor Cyan
    git clone https://github.com/ocornut/imgui.git $imguiDir
    if ($LASTEXITCODE -eq 0) {
        Write-Host "ImGui 下载成功！" -ForegroundColor Green
    } else {
        Write-Host "ImGui 下载失败！" -ForegroundColor Red
        exit 1
    }
}

# 下载 GLFW
$glfwDir = Join-Path $thirdPartyDir "glfw"
if (Test-Path $glfwDir) {
    Write-Host "GLFW 已存在，跳过下载" -ForegroundColor Yellow
} else {
    Write-Host "正在下载 GLFW..." -ForegroundColor Cyan
    git clone https://github.com/glfw/glfw.git $glfwDir
    if ($LASTEXITCODE -eq 0) {
        Write-Host "GLFW 下载成功！" -ForegroundColor Green
    } else {
        Write-Host "GLFW 下载失败！" -ForegroundColor Red
        exit 1
    }
}

Write-Host "`n所有依赖库下载完成！" -ForegroundColor Green
Write-Host "接下来可以运行 CMake 构建项目：" -ForegroundColor Yellow
Write-Host "  mkdir build" -ForegroundColor White
Write-Host "  cd build" -ForegroundColor White
Write-Host "  cmake .." -ForegroundColor White
Write-Host "  cmake --build . --config Release" -ForegroundColor White

