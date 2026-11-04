param(
    [ValidateSet("windows-debug", "windows-release", "windows-test-debug", "windows-test-release")]
    [string[]]$Preset = @("windows-debug", "windows-release", "windows-test-debug",
                          "windows-test-release")
)

#load Native Tools x64
Write-Output "`n=== Setting up environment ==="
Set-Location (Join-Path ${env:ProgramFiles(x86)} "\Microsoft Visual Studio\Installer")
$ToolPath = ./vswhere.exe -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $ToolPath) {
    Write-Output "`n`n[ERROR] VS tools not found, cannot create native toolprompt!"
    exit 1
}
& (Join-Path $ToolPath "\Common7\Tools\Launch-VsDevShell.ps1") -Arch amd64 -HostArch amd64

#configure cmake and build
Set-Location (Get-Item $PSScriptRoot).Parent.FullName
foreach ($p in $Preset) {
    Write-Output "`n=== configuring $p preset ==="
    cmake --preset $p --fresh #--fresh so a local run behaves like a remote run
    if ($LASTEXITCODE) {
        Write-Output "`n`n[ERROR] CMake failed to configure the $p preset!"
        exit $LASTEXITCODE
    }
    Write-Output "`n=== building with preset $p ==="
    cmake --build --preset $p --clean-first #--clean-first so a local run behaves like a remote run
    if ($LASTEXITCODE) {
        Write-Output "`n`n[ERROR] CMake failed to build with preset $p!"
        exit $LASTEXITCODE
    }
    if ($p -like "*test*") {
        Write-Output "`n=== running tests with preset $p ==="
        ctest --preset $p
        if ($LASTEXITCODE) {
            Write-Output "`n`n[ERROR] Test suite failed with preset $p!"
            exit $LASTEXITCODE
        }
    }
}
