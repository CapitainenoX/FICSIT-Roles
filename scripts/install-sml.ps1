# Installs the official SML 3.12.0 (dependency of FICSIT Roles) with ficsit-cli into the selected Steam install.
# Uses a dedicated profile "FICSITRoles" so existing profiles are left untouched.
$ErrorActionPreference = 'Stop'
$Cli = 'F:\Prerequis-Modding-Satisfactory\ficsit.exe'
$Game = 'c:\program files (x86)\steam\steamapps\common\Satisfactory'
if (Get-Process -Name 'FactoryGameSteam-Win64-Shipping' -ErrorAction SilentlyContinue) { throw 'Close Satisfactory first.' }
$ProfilesFile = Join-Path $env:APPDATA 'ficsit\profiles.json'
$Profiles = Get-Content $ProfilesFile -Raw | ConvertFrom-Json
if (-not $Profiles.profiles.PSObject.Properties['FICSITRoles']) {
  & $Cli profile new FICSITRoles | Out-Null
  $Profiles = Get-Content $ProfilesFile -Raw | ConvertFrom-Json
}
$Mods = [pscustomobject]@{ SML = [pscustomobject]@{ version = '3.12.0'; enabled = $true } }
$Profiles.profiles.FICSITRoles.mods = $Mods
# ficsit-cli rejects a UTF-8 BOM (PowerShell 5 adds one with -Encoding utf8): write raw UTF-8.
[IO.File]::WriteAllText($ProfilesFile, ($Profiles | ConvertTo-Json -Depth 8), (New-Object Text.UTF8Encoding $false))
& $Cli installation set-profile $Game FICSITRoles
& $Cli apply
if (!(Test-Path 'C:\Program Files (x86)\Steam\steamapps\common\Satisfactory\FactoryGame\Mods\SML\SML.uplugin')) { throw 'SML install failed (see ficsit-cli output).' }
Write-Output 'SML 3.12.0 installed.'
