# Full release pipeline: sync -> editor module -> regenerate MAM assets -> Alpakit cook+build -> extract pak -> dist zips.
# Never touches the game folder (deploy.ps1 does that).
$ErrorActionPreference = 'Stop'
$Root    = Split-Path -Parent $PSScriptRoot
$Project = Join-Path $Root 'SMLProject\FactoryGame.uproject'
$Engine  = 'F:\Prerequis-Modding-Satisfactory\UnrealEngine-CSS\Engine'
$Diag    = Join-Path $Root 'diagnostics'
$env:TEMP = Join-Path $Root 'temp\Unreal'; $env:TMP = $env:TEMP; $env:PYTHONIOENCODING = 'utf-8'
New-Item -ItemType Directory -Force -Path $env:TEMP, $Diag | Out-Null

function Step($Name, [scriptblock]$Body) { Write-Output "== $Name"; & $Body; if ($LASTEXITCODE -ne 0) { throw "$Name FAILED ($LASTEXITCODE)" } }

Step 'validate config' { python (Join-Path $Root 'tools\validate_config.py') | Select-Object -Last 2 }
& (Join-Path $PSScriptRoot 'sync.ps1')
Step 'editor module' { & "$Engine\Build\BatchFiles\Build.bat" FactoryEditor Win64 Development "-Project=$Project" -Module=FICSITRoles -WaitMutex -MaxParallelActions=8 "-log=$Diag\ubt-editor.log" *> "$Diag\editor-build.txt" }

# Old generated assets are moved aside (removed nodes must not stay in the pak).
$Mam = Join-Path $Root 'SMLProject\Mods\FICSITRoles\Content\MAM'
if (Test-Path $Mam) {
  $Bk = Join-Path $Root ('backups\mam-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
  New-Item -ItemType Directory -Force -Path $Bk | Out-Null
  Move-Item $Mam $Bk
}
New-Item -ItemType Directory -Force -Path $Mam | Out-Null
# Exit code ignored: unrelated SDK placeholder Blueprints report compile errors; success = every asset saved.
Write-Output '== generate MAM assets'; & "$Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $Project -run=pythonscript "-script=$Root\tools\gen_mam.py" -unattended -nosplash -nullrhi -stdout *> "$Diag\gen-mam.txt"
$Saved = Select-String -Path "$Diag\gen-mam.txt" -Pattern 'MAM assets saved: (\d+) / (\d+)' | Select-Object -Last 1
if (!$Saved -or $Saved.Matches[0].Groups[1].Value -ne $Saved.Matches[0].Groups[2].Value) { throw 'MAM generation incomplete - see diagnostics\gen-mam.txt' }
Write-Output $Saved.Line.Trim()
$SrcContent = Join-Path $Root 'Mod\FICSITRoles\Content'
if (Test-Path $SrcContent) { Remove-Item $SrcContent -Recurse -Force }
Copy-Item (Join-Path $Root 'SMLProject\Mods\FICSITRoles\Content') $SrcContent -Recurse

Step 'alpakit' { & "$Engine\Build\BatchFiles\RunUAT.bat" "-ScriptsForProject=$Project" PackagePlugin "-project=$Project" -clientconfig=Shipping -serverconfig=Shipping -utf8output -DLCName=FICSITRoles -build -platform=Win64 -Target=FactoryGameSteam -merge -nocompileeditor *> "$Diag\alpakit.txt" }
Step 'extract pak' { python (Join-Path $Root 'tools\extract_pak.py') }
& (Join-Path $PSScriptRoot 'package.ps1')
Write-Output 'FULL BUILD OK'
