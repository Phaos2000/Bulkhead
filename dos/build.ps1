param([Parameter(Mandatory=$true)][string]$WatcomRoot,[string]$OutputDirectory="$PSScriptRoot/build")
$ErrorActionPreference='Stop'
$compiler=Join-Path $WatcomRoot 'binnt64/wcl386.exe'
if(!(Test-Path -LiteralPath $compiler)){throw 'Expected official Open Watcom binnt64/wcl386.exe'}
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$OutputDirectory=(Resolve-Path -LiteralPath $OutputDirectory).Path
$env:WATCOM=(Resolve-Path -LiteralPath $WatcomRoot).Path
$env:INCLUDE=Join-Path $env:WATCOM 'h'
$env:PATH=(Join-Path $env:WATCOM 'binnt64')+';'+(Join-Path $env:WATCOM 'binnt')+';'+$env:PATH
Push-Location $OutputDirectory
try{
    foreach($name in @('bh486.c','bh_core.c','bh_core.h','bh_room.c','bh_room.h')){Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $OutputDirectory -Force}
    # Local short filenames avoid historical compiler-driver quoting problems.
    & $compiler '-q' '-bt=dos' '-l=dos4g' '-4r' '-ox' '-oe=1000' '-s' '-d0' '-fe=BH486.EXE' 'bh486.c' 'bh_core.c'
    if($LASTEXITCODE){throw 'DOS build failed'}
    & (Join-Path $env:WATCOM 'binnt64/wdis.exe') '-a' '-l=bh_core.asm' 'bh_core.obj'
    if($LASTEXITCODE){throw 'DOS disassembly failed'}
    & $compiler '-q' '-bt=dos' '-l=dos4g' '-4r' '-ox' '-oe=1000' '-s' '-d0' '-dBH_ROOM_EXPERIMENT' '-fe=BHROOM.EXE' 'bh486.c' 'bh_core.c' 'bh_room.c'
    if($LASTEXITCODE){throw 'Experimental DOS room build failed'}
    & (Join-Path $env:WATCOM 'binnt64/wdis.exe') '-a' '-l=bh_room.asm' 'bh_room.obj'
    if($LASTEXITCODE){throw 'Room disassembly failed'}
    Get-FileHash -LiteralPath BH486.EXE -Algorithm SHA256
}finally{Pop-Location}
