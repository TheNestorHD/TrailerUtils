[CmdletBinding()]
param([switch]$NoInstall)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Invoke-Native {
    param([string]$Exe, [string[]]$Arguments)
    & $Exe @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Exe failed (exit $LASTEXITCODE)" }
}

$root = $PSScriptRoot
$tools = Join-Path $env:LOCALAPPDATA 'TrailerUtils-build'
if ($env:RUNNER_TEMP) { $tools = Join-Path $env:RUNNER_TEMP 'tu-build' }
New-Item -ItemType Directory -Force $tools | Out-Null
$vswhere = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer\vswhere.exe'
function Find-MSBuild {
    if (-not (Test-Path $vswhere)) { return }
    & $vswhere -latest -version '[17.0,18.0)' -products '*' -requires Microsoft.Component.MSBuild Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'MSBuild\Current\Bin\amd64\MSBuild.exe' | Select-Object -First 1
}

$msbuild = Find-MSBuild
if (-not $msbuild) {
    if ($NoInstall) { throw 'VS 2022 C++ Build Tools not found.' }
    Write-Host 'Installing VS 2022 Build Tools (C++, Windows SDK)...'
    $installer = Join-Path $tools 'vs_BuildTools.exe'
    Invoke-WebRequest -UseBasicParsing 'https://aka.ms/vs/17/release/vs_BuildTools.exe' -OutFile $installer
    $proc = Start-Process -FilePath $installer -Verb RunAs -ArgumentList '--wait --passive --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended' -Wait -PassThru
    if ($proc.ExitCode -eq 3010 -or $proc.ExitCode -eq 1641) { throw 'Installation succeeded but requires a reboot. Restart Windows and run this script again.' }
    if ($proc.ExitCode -ne 0) { throw "VS installer failed: $($proc.ExitCode)" }
    $msbuild = Find-MSBuild
    if (-not $msbuild) { throw 'MSBuild still not found after installation.' }
}

$git = Get-Command git.exe -ErrorAction SilentlyContinue
if (-not $git) {
    if ($NoInstall) { throw 'Git not found.' }
    $winget = Get-Command winget.exe -ErrorAction SilentlyContinue
    if (-not $winget) { throw 'Install Git for Windows from https://git-scm.com/download/win and run again.' }
    Invoke-Native $winget.Source @('install','--id','Git.Git','--exact','--accept-package-agreements','--accept-source-agreements')
    $git = Get-Command git.exe -ErrorAction SilentlyContinue
    if (-not $git) {
        $gitPath = Join-Path $env:ProgramFiles 'Git\cmd\git.exe'
        if (-not (Test-Path $gitPath)) { throw 'Git installed. Open a new PowerShell window and run again.' }
    } else { $gitPath = $git.Source }
} else { $gitPath = $git.Source }

# Pin the dependency registry so the local build and CI use the same versions.
$vcpkg = Join-Path $tools 'vcpkg-2025.10.17'
$revision = '74e6536215718009aae747d86d84b78376bf9e09'
if (-not (Test-Path (Join-Path $vcpkg '.git'))) {
    Invoke-Native $gitPath @('clone','--depth','1','--branch','2025.10.17','https://github.com/microsoft/vcpkg.git',$vcpkg)
}
$actual = (& $gitPath -C $vcpkg rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $actual -ne $revision) { throw "Unexpected vcpkg revision: $actual" }
$env:VCPKG_ROOT = $vcpkg
$env:VCPKG_DISABLE_METRICS = '1'
$vcpkgExe = Join-Path $vcpkg 'vcpkg.exe'
if (-not (Test-Path $vcpkgExe)) {
    Invoke-Native (Join-Path $vcpkg 'bootstrap-vcpkg.bat') @('-disableMetrics')
}
Push-Location $vcpkg
try {
    Invoke-Native $vcpkgExe @('install','glew:x64-windows-static','vulkan-loader:x64-windows','--classic')
} finally { Pop-Location }

$glewRoot = Join-Path $vcpkg 'installed\x64-windows-static'
$vkRoot = Join-Path $vcpkg 'installed\x64-windows'
$glew = @('glew32s.lib','glew32.lib') | ForEach-Object { Join-Path $glewRoot "lib\$_" } | Where-Object { Test-Path $_ } | Select-Object -First 1
$vk = Join-Path $vkRoot 'lib\vulkan-1.lib'
if (-not $glew -or -not (Test-Path $vk)) { throw 'Dependency libraries not found.' }

$out = Join-Path $root 'artifacts'
New-Item -ItemType Directory -Force $out | Out-Null
$dll = Join-Path $out 'ETS2-TrailerUtil.dll'
if (Test-Path $dll) { Remove-Item $dll -Force }
$log = Join-Path $out 'build.log'
$binlog = Join-Path $out 'build.binlog'
$args = @(
    (Join-Path $root 'ETS2-TrailerUtil.sln'),
    '/t:Rebuild','/m','/p:Configuration=Release','/p:Platform=x64',
    '/p:VcpkgEnabled=false',
    "/p:OutDir=$out\",
    "/p:TrailerVulkanInclude=$vkRoot\include",
    "/p:TrailerVulkanLib=$vk",
    "/p:TrailerGlewInclude=$glewRoot\include",
    "/p:TrailerGlewLib=$glew",
    '/fl', "/flp:LogFile=$log;Verbosity=normal", "/bl:$binlog"
)
Write-Host 'Building Release x64...'
Invoke-Native $msbuild $args
if (-not (Test-Path $dll)) { throw "Expected output missing: $dll" }
Write-Host "Build complete: $dll" -ForegroundColor Green
Get-FileHash $dll -Algorithm SHA256
