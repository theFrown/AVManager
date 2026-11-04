#load Native Tools x64
Write-Output "=== Setting up environment ===`n"
Set-Location (Join-Path ${env:ProgramFiles(x86)} "\Microsoft Visual Studio\Installer")
$ToolPath = ./vswhere.exe -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $ToolPath) {
    Write-Output "[ERROR] VS tools not found, cannot create native toolprompt!"
    exit 1
}
. (Join-Path $ToolPath "\Common7\Tools\Launch-VsDevShell.ps1") -Arch amd64 -HostArch amd64

#configure cmake and build
Set-Location (Get-Item $PSScriptRoot).Parent
foreach ($preset in "windows-debug", "windows-release", "windows-test-debug", "windows-test-release") {
    Write-Output "=== configuring $preset preset ===`n"
    cmake --preset $preset --fresh #--fresh so a local run behaves like a remote run
    if ($LASTEXITCODE) {
        Write-Output "`n`n[ERROR] CMake failed to configure the $preset preset!"
        exit $LASTEXITCODE
    }
    Write-Output "=== building with preset $preset ===`n"
    cmake --build --preset $preset --clean-first #--clean-first so a local run behaves like a remote run
    if ($LASTEXITCODE) {
        Write-Output "`n`n[ERROR] CMake failed to build with preset $preset!"
        exit $LASTEXITCODE
    }
}

#runtests
foreach ($preset in "windows-test-debug", "windows-test-release") {
    Write-Output "=== running tests with preset $preset ===`n"
    ctest --preset $preset
    if ($LASTEXITCODE) {
        Write-Output "`n`n[ERROR] Test suite failed with preset $preset!"
        exit $LASTEXITCODE
    }
}
