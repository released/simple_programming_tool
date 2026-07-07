param(
    [ValidateSet("Debug", "Release")][string]$Configuration = "Release",
    [ValidateSet("Win32", "x64")][string]$Platform = "x64",
    [bool]$CloseRunningTool = $true
)

$ErrorActionPreference = "Stop"

if ($CloseRunningTool) {
    $proc = Get-Process -Name "SimpleProgrammingTool" -ErrorAction SilentlyContinue
    if ($proc) {
        Write-Host "Closing running SimpleProgrammingTool.exe before build..."
        $proc | Stop-Process -Force
        Start-Sleep -Milliseconds 300
    }
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Error "vswhere not found. Please install Visual Studio 2022 with C++ and MFC components."
    exit 1
}

$msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
if (-not $msbuild) {
    Write-Error "MSBuild not found."
    exit 1
}

Push-Location "$PSScriptRoot\.."
& $msbuild "SimpleProgrammingTool.sln" /p:Configuration=$Configuration /p:Platform=$Platform /m
$code = $LASTEXITCODE
Pop-Location

if ($code -ne 0) {
    exit $code
}

$exe = Join-Path "$PSScriptRoot\..\build" "SimpleProgrammingTool.exe"
if (Test-Path $exe) {
    Write-Host "Built: $exe"
} else {
    Write-Warning "Build succeeded but output exe not found at $exe"
}
