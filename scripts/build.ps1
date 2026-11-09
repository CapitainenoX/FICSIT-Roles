# Compiles FICSITRoles (and SML import libs) for the Steam Shipping game target.
param([int]$MaxParallelActions = 6, [string[]]$Modules = @('FICSITRoles'))
$ErrorActionPreference = 'Stop'
$Root    = Split-Path -Parent $PSScriptRoot
$Project = Join-Path $Root 'SMLProject\FactoryGame.uproject'
$Build   = 'F:\Prerequis-Modding-Satisfactory\UnrealEngine-CSS\Engine\Build\BatchFiles\Build.bat'
$LogDir  = Join-Path $Root 'diagnostics'
New-Item -ItemType Directory -Force -Path $LogDir, (Join-Path $Root 'temp\Unreal') | Out-Null
& (Join-Path $PSScriptRoot 'sync.ps1')
$env:TEMP = Join-Path $Root 'temp\Unreal'; $env:TMP = $env:TEMP
$ModuleArgs = @($Modules | ForEach-Object { "-Module=$_" })
& $Build FactoryGameSteam Win64 Shipping "-Project=$Project" @ModuleArgs '-NoHotReload' '-WaitMutex' "-MaxParallelActions=$MaxParallelActions" "-log=$(Join-Path $LogDir 'ubt.log')"
if ($LASTEXITCODE -ne 0) { throw "BUILD_FAILED ($LASTEXITCODE) - see diagnostics\ubt.log" }
Write-Output 'BUILD_SUCCEEDED'
