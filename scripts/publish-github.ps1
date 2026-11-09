# Creates the public GitHub repository "FICSIT-Roles" on your account and pushes the source.
# Run it in a normal PowerShell window (it opens your browser once to log in to GitHub).
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Gh = 'C:\Program Files\GitHub CLI\gh.exe'
if (!(Test-Path $Gh)) {
  # Portable GitHub CLI (official release zip), kept in temp\gh - nothing installed system-wide.
  $Portable = Join-Path $Root 'temp\gh'
  $Gh = Get-ChildItem $Portable -Recurse -Filter gh.exe -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
  if (!$Gh) {
    Write-Output 'Downloading GitHub CLI (portable)...'
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $Release = Invoke-RestMethod 'https://api.github.com/repos/cli/cli/releases/latest' -Headers @{ 'User-Agent' = 'FICSIT-Roles-publish' }
    $Asset = $Release.assets | Where-Object { $_.name -like '*_windows_amd64.zip' } | Select-Object -First 1
    if (!$Asset) { throw 'GitHub CLI download not found - install it from https://cli.github.com then run this script again.' }
    New-Item -ItemType Directory -Force -Path $Portable | Out-Null
    $Zip = Join-Path $Portable $Asset.name
    Invoke-WebRequest $Asset.browser_download_url -OutFile $Zip -UseBasicParsing
    Expand-Archive $Zip -DestinationPath $Portable -Force
    $Gh = Get-ChildItem $Portable -Recurse -Filter gh.exe | Select-Object -First 1 -ExpandProperty FullName
  }
  if (!$Gh) { throw 'GitHub CLI unavailable.' }
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
