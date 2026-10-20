param([int]$Jobs=4,[switch]$Integration,[switch]$LocalPatches,[string]$BuildDirectory="build")
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot
$taskZig=Join-Path $taskRoot '.tools/sim-venv/Lib/site-packages/ziglang/zig.exe'
if(!(Test-Path $taskZig)){
    uv venv (Join-Path $taskRoot '.tools/sim-venv') --python 3.12
    uv pip install --python (Join-Path $taskRoot '.tools/sim-venv/Scripts/python.exe') 'ziglang==0.16.0'
}
$taskCmake=(Get-ChildItem "$env:USERPROFILE/.espressif/tools/cmake/*/bin/cmake.exe" | Select-Object -Last 1).FullName
$taskNinja=(Get-ChildItem "$env:USERPROFILE/.espressif/tools/ninja/*/ninja.exe" | Select-Object -Last 1).FullName
if(!$taskCmake -or !$taskNinja){throw 'Install ESP-IDF tools first (CMake and Ninja).'}
$taskWrapper=Join-Path $taskRoot '.tools/zigcc.cmd'
Set-Content -LiteralPath $taskWrapper -Encoding ascii -Value "@echo off`r`n`"$taskZig`" cc -target x86_64-windows-gnu %*"
$taskPreview=if($Integration){'OFF'}else{'ON'}
$taskPatches=if($LocalPatches){'ON'}else{'OFF'}
& $taskCmake -S $PSScriptRoot -B (Join-Path $PSScriptRoot $BuildDirectory) -G Ninja "-DCMAKE_MAKE_PROGRAM=$taskNinja" "-DCMAKE_C_COMPILER=$taskWrapper" -DCMAKE_BUILD_TYPE=Release "-DSB_UI_PREVIEW=$taskPreview" "-DSB_ENABLE_MANUAL_WEIGHT=$taskPatches" "-DSB_ENABLE_SIMULATOR_CONTROLS=$taskPatches" "-DSB_ENABLE_ACTION_CONFIRMATION=$taskPatches"
if($LASTEXITCODE){throw 'Simulator configuration failed'}
& $taskCmake --build (Join-Path $PSScriptRoot $BuildDirectory) --parallel $Jobs
if($LASTEXITCODE){throw 'Simulator build failed'}
