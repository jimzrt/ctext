$ErrorActionPreference = 'Stop'
$destination = 'C:\BuildTools\git'
$git = Join-Path $destination 'cmd\git.exe'
if (Test-Path $git) { exit 0 }
$archive = 'C:\BuildTools\mingit.zip'
$release = Invoke-RestMethod -UseBasicParsing 'https://api.github.com/repos/git-for-windows/git/releases/latest'
$asset = $release.assets | Where-Object { $_.name -match '^MinGit-.*-64-bit\.zip$' } | Select-Object -First 1
if (-not $asset) { throw 'Could not find the MinGit 64-bit release asset.' }
Invoke-WebRequest -UseBasicParsing $asset.browser_download_url -OutFile $archive
if (Test-Path $destination) { Remove-Item -Recurse -Force $destination }
Expand-Archive -LiteralPath $archive -DestinationPath $destination
Remove-Item -Force $archive
if (-not (Test-Path $git)) { throw 'MinGit installation did not produce cmd\git.exe.' }
