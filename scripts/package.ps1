# Builds the distributable archives in dist\:
#  - FICSITRoles-<ver>.zip          : ficsit.app / SML layout (Windows\ target folder)
#  - FICSITRoles-<ver>-manual.zip   : drop-in folder for <Game>\FactoryGame\Mods\
# Only our own files are packaged: no game binary, no extracted game asset.
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Src = Join-Path $Root 'Mod\FICSITRoles'
$Bin = Join-Path $Root 'SMLProject\Mods\FICSITRoles\Binaries\Win64'
$Version = (Get-Content (Join-Path $Src 'FICSITRoles.uplugin') -Raw | ConvertFrom-Json).SemVersion
$Stage = Join-Path $Root 'temp\package'
$Dist = Join-Path $Root 'dist'
if (Test-Path $Stage) { Remove-Item $Stage -Recurse -Force }
New-Item -ItemType Directory -Force -Path $Dist | Out-Null

function Stage-Plugin([string]$Target) {
  New-Item -ItemType Directory -Force -Path (Join-Path $Target 'Binaries\Win64') | Out-Null
  Copy-Item (Join-Path $Src 'FICSITRoles.uplugin') $Target
  Copy-Item (Join-Path $Src 'Resources') $Target -Recurse
  $Paks = Join-Path $Root 'build\cooked\Content'
  if (Test-Path $Paks) { Copy-Item $Paks $Target -Recurse }
  $Dll = 'FactoryGameSteam-FICSITRoles-Win64-Shipping.dll'
  if (!(Test-Path (Join-Path $Bin $Dll))) { throw "Missing $Bin\$Dll - build first." }
  Copy-Item (Join-Path $Bin $Dll) (Join-Path $Target 'Binaries\Win64')
  [ordered]@{ BuildId = 'SML'; Modules = [ordered]@{ FICSITRoles = $Dll } } | ConvertTo-Json -Depth 4 |
    Set-Content -Encoding ascii (Join-Path $Target 'Binaries\Win64\FactoryGameSteam-Win64-Shipping.modules')
  Copy-Item (Join-Path $Root 'README.md') $Target -ErrorAction SilentlyContinue
}

Stage-Plugin (Join-Path $Stage 'smr\Windows')
Stage-Plugin (Join-Path $Stage 'manual\FICSITRoles')
$Smr = Join-Path $Dist "FICSITRoles-$Version.zip"
$Manual = Join-Path $Dist "FICSITRoles-$Version-manual.zip"
Remove-Item $Smr, $Manual -ErrorAction SilentlyContinue
Compress-Archive -Path (Join-Path $Stage 'smr\*') -DestinationPath $Smr
Compress-Archive -Path (Join-Path $Stage 'manual\*') -DestinationPath $Manual
Get-Item $Smr, $Manual | ForEach-Object { '{0}  {1:N0} KB' -f $_.Name, ($_.Length / 1KB) }
