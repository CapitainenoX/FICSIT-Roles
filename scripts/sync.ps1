# Mirrors the mod source (Mod/FICSITRoles) into the SML project. Content-hash based so UBT sees real changes only.
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Src  = Join-Path $Root 'Mod\FICSITRoles'
$Dst  = Join-Path $Root 'SMLProject\Mods\FICSITRoles'
if (!(Test-Path $Src)) { return }
New-Item -ItemType Directory -Force -Path $Dst | Out-Null
Get-ChildItem -LiteralPath $Src -Recurse -File | ForEach-Object {
  $rel = $_.FullName.Substring($Src.Length).TrimStart('\'); $t = Join-Path $Dst $rel
  if (!(Test-Path $t) -or (Get-FileHash $_.FullName).Hash -ne (Get-FileHash $t).Hash) {
    New-Item -ItemType Directory -Force -Path (Split-Path $t) | Out-Null
    Copy-Item -LiteralPath $_.FullName -Destination $t -Force
    (Get-Item $t).LastWriteTimeUtc = [DateTime]::UtcNow
  }
}
# Remove files deleted from the source tree (source/resources only, never build output).
Get-ChildItem -LiteralPath $Dst -Recurse -File | Where-Object { $_.FullName -notmatch '\\(Binaries|Intermediate|Content)\\' } | ForEach-Object {
  $rel = $_.FullName.Substring($Dst.Length).TrimStart('\')
  if (!(Test-Path (Join-Path $Src $rel))) { Remove-Item -LiteralPath $_.FullName -Force }
}
