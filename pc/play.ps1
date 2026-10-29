$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$exe=Join-Path $repo 'build/native/walk_window.exe'
if(!(Test-Path $exe)){throw 'Compile o jogo com pc/build.ps1 primeiro.'}
$settings=Join-Path (Split-Path $exe) 'settings.json'
if(!(Test-Path $settings)){Copy-Item (Join-Path $PSScriptRoot 'default-settings.json') $settings}
$env:SM64DS_ASSET_ROOT=$repo
$env:SM64DS_ONE_SCREEN_MENUS='1'
$env:SM64DS_SUB_PANEL='0'
$env:SM64DS_SAVE_PATH=Join-Path $repo 'build/user/save/sm64ds.sav'
New-Item -ItemType Directory -Path (Split-Path $env:SM64DS_SAVE_PATH) -Force | Out-Null
Push-Location (Split-Path $exe)
try{& $exe}finally{Pop-Location}
