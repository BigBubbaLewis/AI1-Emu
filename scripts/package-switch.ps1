$ErrorActionPreference='Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
Add-Type -AssemblyName System.IO.Compression.FileSystem
$stage=Join-Path $PWD 'build/package-v02'
if(Test-Path $stage){throw 'Staging directory already exists; use a fresh checked path before packaging.'}
$app=Join-Path $stage 'switch/retroverse'
$pc=Join-Path $stage 'PC-tools'
foreach($dir in @($app,$pc,"$app/roms","$app/artwork","$app/covers","$app/artwork-cache","$app/screenshots","$app/metadata","$app/saves","$app/licenses")){New-Item -ItemType Directory -Force $dir | Out-Null}
Copy-Item RetroVerse.nro "$app/RetroVerse.nro"
foreach($file in @('Prepare-Artwork.ps1','optimize-art.ps1','gba-database.json')){Copy-Item "tools/$file" "$pc/$file"}
@'
@echo off
setlocal
set "RETROVERSE_ART_ROOT=%~1"
if not defined RETROVERSE_ART_ROOT set "RETROVERSE_ART_ROOT=%~dp0..\switch\retroverse"
echo Preparing artwork in "%RETROVERSE_ART_ROOT%"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Prepare-Artwork.ps1" -RetroVerseFolder "%RETROVERSE_ART_ROOT%"
pause
'@ | Set-Content "$pc/Prepare-Artwork.cmd" -Encoding ascii
@'
Place your own legally obtained, uncompressed .gba files here.
Do not place ZIP files here. No commercial games or BIOS files are supplied.
Then optionally run PC-tools/Prepare-Artwork.cmd on Windows for automatic covers.
'@ | Set-Content "$app/roms/README.txt"
@'
Artwork paths used by RetroVerse v0.2:
../covers/<ROM stem>.png or .jpg/.jpeg : optional custom override
../artwork-cache/<canonical release title>.png : automatic PC-prepared covers
../screenshots/<ROM stem>.png or .jpg/.jpeg : optional gameplay screenshot
This artwork/ folder is documentation only; the application does not scan it.
Run PC-tools/Prepare-Artwork.cmd to prepare covers automatically.
No commercial artwork is bundled: redistribution permission was not established.
'@ | Set-Content "$app/artwork/README.txt"
@'
RetroVerse Arcade Edition v0.2 - SD-card installation

Extract this ZIP directly into the ROOT of your Switch SD card.
The result must be /switch/retroverse/RetroVerse.nro (no extra package folder).
Add your own legally obtained .gba files to /switch/retroverse/roms/.
Launch RetroVerse through Homebrew Menu, preferably in application mode.
Atmosphere and Homebrew Menu must already be installed; they are not included.
No Nintendo system files or HOME menu files are modified.

OPTIONAL AUTOMATIC COVERS (Windows PC)
No additional software installation is needed. After adding ROMs, double-click
PC-tools/Prepare-Artwork.cmd. It defaults to the adjacent switch/retroverse
folder, whether extracted onto your SD card or first onto your PC.
Internet is needed only for downloading missing covers on the PC.
It uses Windows PowerShell and System.Drawing to resize images; no npm install.
Existing custom covers and previously downloaded images are preserved.
If preparing on the PC, copy the entire switch/retroverse folder to your SD.
To target another location: PC-tools\Prepare-Artwork.cmd E:\switch\retroverse
Offline check: PC-tools\Prepare-Artwork.cmd -Offline
The app itself stays offline. PC-tools may remain on the PC after preparation.

CONTROLS
D-pad/left stick: select. A: play. X: cover/cartridge. Y: reverse title sort.
B: back/exit. Plus or Minus: settings. L/R: tabs.
In game: Plus=Start, Minus=Select; hold Plus+Minus to return to the library.
ZR: save one state. ZL: load it. Battery saves are automatic.
Back up existing saves before testing; do not turn off during writes.

ASSETS
The font, font license, 3,503-record identification database and attribution
are embedded in RetroVerse.nro. No external runtime font/config is needed.
Optional overrides and generated files use covers/, artwork-cache/,
screenshots/, metadata/ and saves/ under /switch/retroverse/.
artwork/README.txt describes the actual paths; artwork/ itself is not scanned.
No ROMs, BIOS, commercial covers, screenshots, compiler or emulator sources
are included. mGBA provides a built-in BIOS replacement.

LIMITATIONS
Game matching uses SHA-1 on the PC, with filename/header heuristics as fallback.
Unknown games use a placeholder; matching and artwork availability vary.
No blanket redistribution right was established for Libretro's commercial
game imagery. Personal artwork downloads are separate from this package.
Gameplay, UI performance, audio, saves, states and Switch/Switch Lite behavior
have NOT been tested on physical hardware. A successful build is not proof.
Normal clocks are intended; overclocking is not required by the application.
'@ | Set-Content "$stage/INSTALL.txt" -Encoding ascii
Copy-Item "$stage/INSTALL.txt" "$app/README.txt"
Copy-Item romfs/*LICENSE* "$app/licenses/"
Copy-Item romfs/DATABASE-ATTRIBUTION.txt "$app/licenses/"
Copy-Item vendor/mgba/LICENSE "$app/licenses/mGBA-MPL-2.0.txt"
Copy-Item licenses/libnx-LICENSE.txt,licenses/SDL2-LICENSE.txt "$app/licenses/"
$stb=Get-Content source/third_party/stb_image.h -Raw
$notice=$stb.Substring($stb.LastIndexOf('This software is available under 2 licenses'))
$notice | Set-Content "$app/licenses/stb-LICENSE.txt"
Copy-Item licenses/libretro-database-LICENSE.txt "$pc/"
Copy-Item romfs/DATABASE-ATTRIBUTION.txt "$pc/"
@'
PC artwork tools use built-in Windows PowerShell; no Node.js is required.
Prepare-Artwork.cmd defaults to ../switch/retroverse relative to this folder.
Add ROMs first, then run it. An explicit path argument overrides the default.
The database is CC-BY-SA-4.0; attribution and full license accompany it.
Downloaded cover images are personal cached assets, not part of this release.
No development tools or npm packages are included or installed by this tool.
'@ | Set-Content "$pc/README.txt"
$hash=(Get-FileHash RetroVerse.nro -Algorithm SHA256).Hash
"RetroVerse.nro SHA256: $hash`nBuilt and packaged November 3, 2026. Not hardware-tested." | Set-Content "$stage/BUILD-INFO.txt"
$zipPath=Join-Path $PWD 'RetroVerse-v0.2-Switch-Package.zip'
if(Test-Path $zipPath){throw 'Package ZIP already exists; refusing to overwrite.'}
$zip=[IO.Compression.ZipFile]::Open($zipPath,[IO.Compression.ZipArchiveMode]::Create)
try {
 foreach($item in Get-ChildItem $stage -Recurse){
  $entry=$item.FullName.Substring($stage.Length+1).Replace('\','/')
  if($item.PSIsContainer){$null=$zip.CreateEntry($entry+'/')}
  else{$null=[IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,$item.FullName,$entry,[IO.Compression.CompressionLevel]::Optimal)}
 }
} finally {$zip.Dispose()}
Write-Output $zipPath

