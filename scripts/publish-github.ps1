# Creates the public GitHub repository "FICSIT-Roles" on your account and pushes the source.
# Run it in a normal PowerShell window (it opens your browser once to log in to GitHub).
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Gh = 'C:\Program Files\GitHub CLI\gh.exe'

if (!(Test-Path $Gh)) {
  Write-Output 'Installing GitHub CLI...'
  winget install --id GitHub.cli -e --accept-source-agreements --accept-package-agreements
  if (!(Test-Path $Gh)) { throw 'GitHub CLI install failed - install it from https://cli.github.com then run this script again.' }
}

& $Gh auth status 2>$null
if ($LASTEXITCODE -ne 0) {
  Write-Output 'Log in to GitHub (a browser page opens, paste the code shown here):'
  & $Gh auth login --hostname github.com --git-protocol https --web
  if ($LASTEXITCODE -ne 0) { throw 'GitHub login failed.' }
}
& $Gh auth setup-git | Out-Null

Set-Location $Root
$HasOrigin = (git remote) -contains 'origin'
if (!$HasOrigin) {
  & $Gh repo create FICSIT-Roles --public --source $Root --remote origin --push `
    --description 'FICSIT Roles - industrial roleplay divisions for Satisfactory co-op (SML mod)'
  if ($LASTEXITCODE -ne 0) { throw 'Repository creation failed (does FICSIT-Roles already exist on your account?).' }
} else {
  git push -u origin main
}
$Url = & $Gh repo view --json url -q .url
Write-Output "PUBLISHED: $Url"
