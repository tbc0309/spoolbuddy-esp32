param(
    [string]$Board="esp32-p4-wifi6-touch-lcd-4.3/rev3",
    [string]$BuildDirectory="build",
    [Nullable[bool]]$BootSleep=$null,
    [uint32]$BuildRevision=0,
    [switch]$LocalPatches
)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot
$taskBoardPath=Join-Path (Join-Path $taskRoot 'boards') $Board
if(!(Test-Path (Join-Path $taskBoardPath 'board.cmake'))){throw "Unsupported board: $Board"}
$taskPatches=if($LocalPatches){'ON'}else{'OFF'}
$taskFeatureArgs=@()
if($PSBoundParameters.ContainsKey('BootSleep')) {
    $taskBootSleep=if($BootSleep){'ON'}else{'OFF'}
    $taskFeatureArgs=@('-D',"SB_ENABLE_BOOT_SLEEP=$taskBootSleep")
}
if($BuildRevision -gt 0) {$taskFeatureArgs+=@('-D',"SB_FIRMWARE_BUILD=$BuildRevision")}
$taskDefaults=(Join-Path $taskRoot 'sdkconfig.defaults')+';'+(Join-Path $taskBoardPath 'sdkconfig.defaults')
$taskConfig=Join-Path (Join-Path $taskRoot $BuildDirectory) 'sdkconfig'
& idf.py @taskFeatureArgs -B $BuildDirectory -D "SB_BOARD=$Board" -D "SDKCONFIG=$taskConfig" -D "SDKCONFIG_DEFAULTS=$taskDefaults" `
    -D SB_UI_PREVIEW=OFF -D "SB_ENABLE_MANUAL_WEIGHT=$taskPatches" -D "SB_ENABLE_ACTION_CONFIRMATION=$taskPatches" build
if($LASTEXITCODE){throw 'Firmware build failed'}
