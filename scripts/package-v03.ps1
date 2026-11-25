$ErrorActionPreference='Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -AssemblyName System.IO.Compression
$project=$PWD.Path
$releaseVersion=[regex]::Match((Get-Content source/app_version.h -Raw),'AI1EMU_VERSION "([0-9.]+)"').Groups[1].Value
if(-not $releaseVersion){throw 'Missing app version'}
$stage=Join-Path $project ('build/package-v'+$releaseVersion+'-'+[Guid]::NewGuid().ToString('N'))
$app=Join-Path $stage 'switch/ai1emu';$pc=Join-Path $stage 'PC-tools';$notices=Join-Path $stage 'Source-notices'
$slugs=@('gba','gb','gbc','nes','snes','genesis','sms','gg','nds','dsi','n64')
foreach($dir in @($app,$pc,$notices,"$app/cores","$app/licenses","$app/roms","$app/artwork","$app/covers","$app/artwork-cache","$app/metadata","$app/screenshots","$app/saves","$app/system")){New-Item -ItemType Directory -Force $dir | Out-Null}
foreach($slug in $slugs){foreach($parent in @('roms','system','saves','covers','metadata','artwork-cache','screenshots')){New-Item -ItemType Directory -Force "$app/$parent/$slug" | Out-Null}}
$binaries=@{'AI1Emu.nro'='AI1Emu.nro';'build/switch/cores/mupen64plus-next.nro'='cores/mupen64plus-next.nro';'build/switch/cores/melonds.nro'='cores/melonds.nro';'build/switch/cores/fceumm.nro'='cores/fceumm.nro';'build/switch/cores/snes9x.nro'='cores/snes9x.nro';'build/switch/cores/genesis-plus-gx.nro'='cores/genesis-plus-gx.nro'}
$hashes=@()
foreach($source in $binaries.Keys){
 if(-not(Test-Path -LiteralPath $source)){throw "Missing binary: $source"}
 $f=[IO.File]::OpenRead((Join-Path $project $source));try{$f.Position=16;$magic=New-Object byte[] 4;$null=$f.Read($magic,0,4);if([Text.Encoding]::ASCII.GetString($magic) -ne 'NRO0'){throw "Invalid NRO: $source"}}finally{$f.Dispose()}
 Copy-Item -LiteralPath $source -Destination (Join-Path $app $binaries[$source])
 $hashes+=((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()+'  switch/ai1emu/'+$binaries[$source])
}
$mainInputs=@('main.cpp','legacy_migration.cpp','legacy_migration.h','library_ui.cpp','library_ui.h','ui_kit.cpp','ui_kit.h','app_data.cpp','app_data.h','game_session.cpp','game_session.h','keyboard.h','game_database.cpp','game_database.h','platform_library.cpp','platform_library.h','online_updater.cpp','online_updater.h','app_version.h','runtime_paths.h') | ForEach-Object {Get-Item (Join-Path 'source' $_)}
$newest=($mainInputs | Sort-Object LastWriteTime -Descending | Select-Object -First 1).LastWriteTime
if((Get-Item AI1Emu.nro).LastWriteTime -lt $newest){throw 'Main NRO predates a source change. Rebuild first.'}
$coreNewest=(@('core_runner.cpp','core_status.h','dual_screen.h','game_session.cpp','game_session.h','ui_kit.cpp','app_data.cpp') | ForEach-Object {Get-Item (Join-Path 'source' $_)} | Sort-Object LastWriteTime -Descending | Select-Object -First 1).LastWriteTime
foreach($core in Get-ChildItem build/switch/cores -Filter '*.nro' | Where-Object {$_.Name -notin @('mupen64plus-next.nro','melonds.nro')}){if($core.LastWriteTime -lt $coreNewest){throw "Core predates a frontend source change: $($core.Name)"}}
$n64Newest=(@('n64_runner.cpp','n64_video.cpp','n64_video.h','platform_library.cpp','platform_library.h','game_session.cpp','ui_kit.cpp','app_data.cpp') | ForEach-Object {Get-Item (Join-Path 'source' $_)} | Sort-Object LastWriteTime -Descending | Select-Object -First 1).LastWriteTime
if((Get-Item build/switch/cores/mupen64plus-next.nro).LastWriteTime -lt $n64Newest){throw 'N64 runner needs rebuilding'}
Copy-Item vendor/mupen64plus-next/LICENSE "$app/licenses/Mupen64Plus-Next-GPL-2.0.txt"
Copy-Item docs/V0.3.3-N64.md "$stage/N64-README.md"
Copy-Item docs/V0.3.4-FOLDERS.md "$stage/AUTOMATIC-FOLDERS.md"
Copy-Item docs/V0.3.5-FIXES.md "$stage/V0.3.5-FIXES.md"
Copy-Item docs/V0.4.0-UI.md "$stage/V0.4.0-UI.md"
Copy-Item docs/V0.4.0-INTEGRATION.md "$stage/V0.4.0-INTEGRATION.md"
Copy-Item docs/V0.5.0.md "$stage/V0.5.0-WHATS-NEW.md"
$dsNewest=(@('ds_runner.cpp','ds_audio.h','core_status.h','dual_screen.h','platform_library.cpp','platform_library.h','game_session.cpp','ui_kit.cpp','app_data.cpp') | ForEach-Object {Get-Item (Join-Path 'source' $_)} | Sort-Object LastWriteTime -Descending | Select-Object -First 1).LastWriteTime
if((Get-Item build/switch/cores/melonds.nro).LastWriteTime -lt $dsNewest){throw 'DS runner needs rebuilding'}
[IO.File]::WriteAllText((Join-Path $app 'cores/melonds-audio.version'),"1`n")
[IO.File]::WriteAllText((Join-Path $app 'cores/cores.version'),"$releaseVersion`n")
$hashes | Set-Content "$stage/SHA256SUMS.txt" -Encoding ascii
Copy-Item tools/AI1Emu-Automatic-Covers-DBI.cmd,tools/AI1Emu-Prepare-Artwork.cmd $pc
@'
@echo off
setlocal
set "AI1_TARGET=%~1"
if not defined AI1_TARGET set "AI1_TARGET=%~dp0..\switch\ai1emu"
call "%~dp0AI1Emu-Prepare-Artwork.cmd" "%AI1_TARGET%"
'@ | Set-Content "$pc/Prepare-Artwork.cmd" -Encoding ascii
@'
SD reader: after adding ROMs, double-click Prepare-Artwork.cmd.
It defaults to the adjacent switch/ai1emu folder. Optional explicit path:
Prepare-Artwork.cmd E:\switch\ai1emu
DBI USB: open DBI > Run MTP responder, then double-click
AI1Emu-Automatic-Covers-DBI.cmd. It reads filenames, never transfers ROMs.
Both tools are self-contained. Windows PowerShell 5.1 is sufficient; no Node.js.
Only missing artwork is downloaded, using a 10-second request timeout.
Keep downloaded commercial artwork as a personal cache; do not redistribute it.
For legacy switch/retroverse data with an SD reader, pass that folder explicitly.
'@ | Set-Content "$pc/README.txt"
Copy-Item docs/V0.3-SUPPORT.md "$stage/INSTALL.md"
Copy-Item docs/V0.3.1-FIXES.md "$stage/V0.3.1-FIXES.md"
Copy-Item docs/V0.3.2-MERGE.md "$stage/V0.3.2-MERGE.md"
Copy-Item COLLABORATING.md "$notices/COLLABORATING.md"
Copy-Item docs/V0.3-SUPPORT.md "$app/README.md"
@'
Place your own legally obtained, uncompressed games here or in console subfolders.
GBA .gba; GB .gb; GBC .gbc; NES .nes; SNES .sfc/.smc;
Genesis .md/.gen/.smd (.bin requires SEGA header or genesis folder);
Nintendo 64 .z64/.v64/.n64 (cartridges only); Master System .sms; Game Gear .gg; DS .nds; DSi .dsi or nds in dsi folder.
No commercial games, proprietary firmware or commercial artwork are included.
Do not place ZIP archives here. See ../README.md for firmware and limitations.
'@ | Set-Content "$app/roms/README.txt"
@'
Optional artwork is prepared on Windows using PC-tools/Prepare-Artwork.cmd
or the single-file DBI tool. No commercial covers are distributed in this ZIP.
Actual paths: ../covers/<console>/<ROM stem>.png|jpg|jpeg for custom art;
../artwork-cache/<console>/<release title>.png for automatic cached covers;
../metadata/<console>/<ROM stem>.txt and ../screenshots/<console>/<ROM stem>.*.
Legacy flat GBA paths remain supported. This artwork/ folder is documentation.
'@ | Set-Content "$app/artwork/README.txt"
@'
No proprietary firmware is included.
DS optional own dumps: nds/bios7.bin, nds/bios9.bin, nds/firmware.bin.
DSi requires own dsi/dsi_bios7.bin, dsi/dsi_bios9.bin,
dsi/dsi_firmware.bin, dsi/dsi_nand.bin. DSi states are unsupported.
GBA/GB/GBC optional own BIOS: gba/gba_bios.bin, gb/gb_bios.bin, gbc/gbc_bios.bin.
See ../README.md for details. The embedded DS FreeBIOS is an independently
written, redistributable replacement; its notice is in ../licenses/.
'@ | Set-Content "$app/system/README.txt"
Copy-Item licenses/* $app/licenses
Copy-Item romfs/DATABASE-ATTRIBUTION.txt "$app/licenses/"
Copy-Item vendor/mgba/LICENSE "$app/licenses/mGBA-MPL-2.0.txt"
Copy-Item vendor/fceumm/Copying "$app/licenses/FCEUmm-COPYING.txt"
Copy-Item vendor/snes9x/LICENSE "$app/licenses/Snes9x-LICENSE.txt"
Copy-Item vendor/genesis-plus-gx/LICENSE.txt "$app/licenses/Genesis-Plus-GX-LICENSE.txt"
Copy-Item vendor/melonds-libretro/LICENSE "$app/licenses/melonDS-LICENSE.txt"
Copy-Item vendor/melonds-libretro/freebios/drastic_bios_readme.txt "$app/licenses/DS-FreeBIOS.txt"
foreach($header in @('stb_image.h','stb_truetype.h')){$text=Get-Content "source/third_party/$header" -Raw;$at=$text.LastIndexOf('This software is available under 2 licenses');if($at -ge 0){$text.Substring($at) | Set-Content "$app/licenses/$header-LICENSE.txt"}}
$sourceArchive="build/AI1-Emu-v$releaseVersion-Corresponding-Source.tar.gz"
if(-not(Test-Path $sourceArchive)){throw 'Create the corresponding-source archive before packaging.'}
$sourceHash=(Get-FileHash $sourceArchive).Hash
Copy-Item $sourceArchive $notices
if((Get-FileHash (Join-Path $notices "AI1-Emu-v$releaseVersion-Corresponding-Source.tar.gz")).Hash -ne $sourceHash){throw 'Source archive changed during packaging. Wait for its build to finish.'}
@'
These files are source/notices, not runtime dependencies. They may stay on your PC.
The source archive contains the six used emulator trees, AI1 integration source,
build scripts, metadata and dependency source/patch archives, excluding build tools,
compiled libraries, commercial ROMs, proprietary firmware and keys.
The package is noncommercial; see individual licenses. Do not remove source and
license notices when redistributing modified Genesis Plus GX or GPL companions.
Build instructions: docs/V0.3-SUPPORT.md and scripts/build-multi-switch.sh.
Source and release downloads: https://ai1emu-updates.freegamez.workers.dev/
The older stable GBA update channel remains separate from multi-core updates.
'@ | Set-Content "$notices/README.txt"
$zip=Join-Path $project "AI1-Emu-v$releaseVersion-Switch-Package.zip"
if(Test-Path $zip){throw 'Package version already exists; increase the version or explicitly archive a failed local package.'}
$output=[IO.Compression.ZipFile]::Open($zip,[IO.Compression.ZipArchiveMode]::Create)
try{
 # .NET Framework on Windows otherwise writes backslashes into ZIP names.
 foreach($dir in Get-ChildItem -LiteralPath $stage -Directory -Recurse){$relative=$dir.FullName.Substring($stage.Length+1).Replace('\','/');$null=$output.CreateEntry($relative+'/')}
 foreach($file in Get-ChildItem -LiteralPath $stage -File -Recurse){$relative=$file.FullName.Substring($stage.Length+1).Replace('\','/');$null=[IO.Compression.ZipFileExtensions]::CreateEntryFromFile($output,$file.FullName,$relative,[IO.Compression.CompressionLevel]::Optimal)}
}finally{$output.Dispose()}
$archive=[IO.Compression.ZipFile]::OpenRead($zip)
try{
 foreach($source in $binaries.Keys){$name='switch/ai1emu/'+$binaries[$source];$entry=$archive.GetEntry($name);if(-not $entry){throw "Missing ZIP entry: $name"};$stream=$entry.Open();$sha=[Security.Cryptography.SHA256]::Create();try{$digest=[BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','');if($digest -ne (Get-FileHash $source).Hash){throw "ZIP hash mismatch: $name"}}finally{$sha.Dispose();$stream.Dispose()}}
 $entry=$archive.GetEntry("Source-notices/AI1-Emu-v$releaseVersion-Corresponding-Source.tar.gz");if(-not $entry){throw 'Missing source companion'};$stream=$entry.Open();$sha=[Security.Cryptography.SHA256]::Create();try{if([BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','') -ne $sourceHash){throw 'ZIP source companion hash mismatch'}}finally{$sha.Dispose();$stream.Dispose()}
 foreach($entry in $archive.Entries){if($entry.FullName -match '(^|/)(prod\.keys|title\.keys|devkitPro|build)(/|$)|\.gba$|\.nds$'){throw "Unexpected runtime ZIP entry: $($entry.FullName)"}}
 $archive.Entries | ForEach-Object {$_.FullName} | Set-Content build/v03-package-contents.txt
}finally{$archive.Dispose()}
Write-Output "PASS: package layout and all six NRO hashes verified: $zip"
Write-Output "Staging directory: $stage"


