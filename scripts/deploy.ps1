# Installs the compiled FICSIT Roles into the Steam game (Mods\FICSITRoles).
# Safe by design: refuses while the game runs, backs up the previous install, never touches saves.
param(
  [string]$Game = 'C:\Program Files (x86)\Steam\steamapps\common\Satisfactory'
)
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Built = Join-Path $Root 'SMLProject\Mods\FICSITRoles'
$Src = Join-Path $Root 'Mod\FICSITRoles'
$Dll = 'FactoryGameSteam-FICSITRoles-Win64-Shipping.dll'
$BinSrc = Join-Path $Built 'Binaries\Win64'

if (Get-Process -Name 'FactoryGameSteam-Win64-Shipping', 'FactoryGame*' -ErrorAction SilentlyContinue) {
  throw 'Satisfactory is running. Save, quit the game, then deploy again.'
}
if (!(Test-Path (Join-Path $BinSrc $Dll))) { throw "Missing build output $BinSrc\$Dll - run scripts\build.ps1 first." }
if (!(Test-Path (Join-Path $Game 'FactoryGame\Mods\SML\SML.uplugin'))) {
  throw 'SML is not installed in the game. Run scripts\install-sml.ps1 first (official SML 3.12.0 via ficsit-cli).'
}

$Dest = Join-Path $Game 'FactoryGame\Mods\FICSITRoles'
if (Test-Path $Dest) {
  $Backup = Join-Path $Root ('backups\' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
  New-Item -ItemType Directory -Force -Path $Backup | Out-Null
  Copy-Item -LiteralPath $Dest -Destination $Backup -Recurse -Force
  Remove-Item -LiteralPath $Dest -Recurse -Force
  Write-Output "Previous install backed up to $Backup"
}
New-Item -ItemType Directory -Force -Path (Join-Path $Dest 'Binaries\Win64') | Out-Null
Copy-Item -LiteralPath (Join-Path $Src 'FICSITRoles.uplugin') -Destination $Dest -Force
Copy-Item -LiteralPath (Join-Path $Src 'Resources') -Destination $Dest -Recurse -Force
# Cooked MAM trees (Alpakit pak). Without it the technologies fall back to the F7 terminal.
$Paks = Join-Path $Root 'build\cooked\Content'
if (Test-Path $Paks) { Copy-Item -LiteralPath $Paks -Destination $Dest -Recurse -Force } else { Write-Warning 'No cooked pak: run Alpakit (MAM trees will use the F7 terminal).' }
Copy-Item -LiteralPath (Join-Path $BinSrc $Dll) -Destination (Join-Path $Dest 'Binaries\Win64') -Force
$Pdb = Join-Path $BinSrc ($Dll -replace '\.dll$', '.pdb')
if (Test-Path $Pdb) { Copy-Item -LiteralPath $Pdb -Destination (Join-Path $Dest 'Binaries\Win64') -Force }
# Same rule as Alpakit: BuildId "SML" lets any game build load the module.
$Manifest = [ordered]@{ BuildId = 'SML'; Modules = [ordered]@{ FICSITRoles = $Dll } }
$Manifest | ConvertTo-Json -Depth 4 | Set-Content -Encoding ascii (Join-Path $Dest 'Binaries\Win64\FactoryGameSteam-Win64-Shipping.modules')

$Hash = (Get-FileHash (Join-Path $Dest "Binaries\Win64\$Dll")).Hash.Substring(0, 12)
Write-Output "DEPLOYED FICSITRoles 1.0.0 to $Dest (dll sha256 $Hash)"
