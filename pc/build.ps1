param([string]$PythonPath,[int]$Jobs=4,[switch]$TestsOnly)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs=(& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
if(!$vs){throw 'Instale as ferramentas C++ x86 do Visual Studio.'}
$vcvars=Join-Path $vs 'VC/Auxiliary/Build/vcvars32.bat'
$cmakeRoot=Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake'
if(!$PythonPath){$PythonPath=Join-Path $repo '.venv/Scripts/python.exe'}
if(!(Test-Path $PythonPath)){throw 'Crie a .venv e instale pc/requirements.txt antes de compilar.'}
$PythonPath=(Resolve-Path $PythonPath).Path
$tests=Join-Path $repo 'build/native-tests';New-Item -ItemType Directory -Path $tests -Force | Out-Null
$script=Join-Path $repo 'build/pc-build.cmd'
$jobsCount=[Math]::Min(16,[Math]::Max(1,$Jobs))
$body=@"
@echo off
setlocal
set "VSLANG=1033"
call "$vcvars" >nul || exit /b 1
set "PATH=$([IO.Path]::GetDirectoryName($PythonPath));$cmakeRoot\CMake\bin;$cmakeRoot\Ninja;%PATH%"
pushd "$tests"
cl /nologo /EHsc /MT /std:c++17 /I"$repo\port" "$repo\port\tests\smoke_pc_single_screen.cpp" "$repo\port\hal\menu_bigmap.cpp" "$repo\port\hal\pc_map_ui.cpp" /Fesmoke_pc_single_screen.exe || exit /b 1
smoke_pc_single_screen.exe || exit /b 1
cl /nologo /EHsc /MT /std:c++17 "$repo\port\tests\smoke_pc_boot_order.cpp" "$repo\port\hal\os_lockid.cpp" /Fesmoke_pc_boot_order.exe || exit /b 1
smoke_pc_boot_order.exe || exit /b 1
popd
"@
if(!$TestsOnly){
    if(!(Test-Path (Join-Path $repo 'extracted/arm9_dec.bin'))){throw 'Prepare sua ROM europeia com pc/prepare-rom.py antes da build nativa.'}
    $body+=@"

cmake -S "$repo\port" -B "$repo\build\native" -G Ninja -DCMAKE_BUILD_TYPE=Release -DPORT_ROM_CLEAN=ON || exit /b 1
cmake --build "$repo\build\native" --target walk_window --parallel $jobsCount || exit /b 1
"@
}
$body+="`nexit /b 0`n"
[IO.File]::WriteAllText($script,$body,[Text.Encoding]::Default)
& cmd /c $script
if($LASTEXITCODE -ne 0){throw "Falha na build ou nos testes (exit $LASTEXITCODE)."}
