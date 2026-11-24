$ErrorActionPreference='Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
$root=Join-Path $PWD ('build/artwork-fixture-'+[Guid]::NewGuid().ToString('N'))
$games=@{n64='Super Mario 64 (USA).z64';gba='Pokemon - Emerald Version (USA, Europe).gba';gb='Tetris (World).gb';gbc='Pokemon - Crystal Version (USA, Europe).gbc';nes='Super Mario Bros. (World).nes';snes='Super Mario World (USA).sfc';genesis='Sonic The Hedgehog (USA, Europe).md';sms='Alex Kidd in Miracle World (USA, Europe).sms';gg='Sonic The Hedgehog (USA, Europe).gg';nds='Mario Kart DS (USA).nds'}
foreach($slug in $games.Keys){$dir=Join-Path $root "roms/$slug";New-Item -ItemType Directory -Force $dir | Out-Null;[IO.File]::WriteAllBytes((Join-Path $dir $games[$slug]),[byte[]]@())}
$custom=Join-Path $root 'covers/gba/Pokemon - Emerald Version (USA, Europe).png';New-Item -ItemType Directory -Force (Split-Path $custom -Parent) | Out-Null;[IO.File]::WriteAllText($custom,'custom sentinel')
& ./tools/Prepare-Multi-Artwork.ps1 -RetroVerseFolder $root -Offline
$report=Get-Content (Join-Path $root 'artwork-report.json') -Raw | ConvertFrom-Json
if($report.Count -ne $games.Count){throw "Expected $($games.Count) matches; got $($report.Count)"}
if([IO.File]::ReadAllText($custom) -ne 'custom sentinel'){throw 'Custom art was overwritten'}
foreach($entry in $report){$slug=$entry.platform;$stem=[IO.Path]::GetFileNameWithoutExtension($entry.filename);if(-not(Test-Path -LiteralPath (Join-Path $root "metadata/$slug/$stem.txt"))){throw "Missing metadata: $slug/$stem"}}
Write-Output 'PASS: multi-console artwork matching, offline handling, subfolders and custom art preservation'
$checksumRoot=Join-Path $PWD ('build/artwork-checksum-'+[Guid]::NewGuid().ToString('N'));New-Item -ItemType Directory -Force "$checksumRoot/roms" | Out-Null
$bytes=[Text.Encoding]::ASCII.GetBytes('AI1 checksum test fixture, not a game');[IO.File]::WriteAllBytes("$checksumRoot/roms/renamed.nes",$bytes)
$sha=[Security.Cryptography.SHA1]::Create();try{$digest=[BitConverter]::ToString($sha.ComputeHash($bytes)).Replace('-','')}finally{$sha.Dispose()}
$databaseJson=ConvertTo-Json -Depth 6 -InputObject @(@{slug='nes';repository='unused-offline';games=@(@{title='AI1 Test Fixture';sha1=$digest})})
& ./tools/Prepare-Multi-Artwork.ps1 -RetroVerseFolder $checksumRoot -Offline
$identified=Get-Content "$checksumRoot/metadata/nes/renamed.txt" -Raw
if($identified -notmatch 'title=AI1 Test Fixture' -or $identified -notmatch 'identification=SHA-1'){throw 'Checksum matching failed'}
Write-Output 'PASS: checksum identifies a renamed synthetic fixture without network or commercial ROMs'
