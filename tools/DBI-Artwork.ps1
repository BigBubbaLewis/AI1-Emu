# The build script appends the self-contained preparation engine to this wrapper.
$ErrorActionPreference='Stop'
function Child($folder,[string]$name){foreach($item in $folder.Items()){if($item.Name -ieq $name){return $item}}return $null}
function Walk-Files($folder,[string]$relative=''){foreach($item in $folder.Items()){if($item.IsFolder){Walk-Files $item.GetFolder ($relative+$item.Name+'/')}else{[pscustomobject]@{Name=$item.Name;Relative=$relative+$item.Name}}}}
function Has-SupportedGames($appFolder){if(-not $appFolder){return $false};$roms=Child $appFolder 'roms';if($roms){foreach($game in Walk-Files $roms.GetFolder){if($game.Name -match '\.(gba|gb|gbc|nes|sfc|smc|md|gen|smd|sms|gg|nds|dsi|z64|v64|n64|bin)$'){return $true}}};return $false}
function Choose-AppFolder($switchFolder){$current=Child $switchFolder 'ai1emu';$legacy=Child $switchFolder 'retroverse';if($current -and (Has-SupportedGames $current.GetFolder)){return $current};if($legacy -and (Has-SupportedGames $legacy.GetFolder)){return $legacy};if($current){return $current};return $legacy}
function Remote-Parent($folder,[string]$relative){$parts=$relative -split '/';for($i=0;$i -lt $parts.Count-1;$i++){$folder=Ensure-Folder $folder $parts[$i]};return $folder}
function Ensure-Folder($folder,[string]$name){$item=Child $folder $name;if(-not $item){$folder.NewFolder($name);for($i=0;$i -lt 30;$i++){Start-Sleep -Milliseconds 200;$item=Child $folder $name;if($item){break}}};if(-not $item){throw "Could not create $name on the SD card"};return $item.GetFolder}
function Copy-ToSwitch($folder,[string]$file){
 $name=[IO.Path]::GetFileName($file);if(Child $folder $name){return}
 $seen=0;$folder.CopyHere($file,1044);$deadline=(Get-Date).AddSeconds(15)
 do{Start-Sleep -Milliseconds 250;$item=Child $folder $name;if($item){$seen++;if([long]$item.Size -eq (Get-Item -LiteralPath $file).Length){return};if([long]$item.Size -le 0 -and $seen -ge 8){Write-Host 'DBI reports no size; file presence confirmed only.';return}}}while((Get-Date) -lt $deadline)
 throw "Could not confirm SD transfer: $name"
}
Write-Host 'FAST COVERS: reading filenames only; no ROM transfers.' -ForegroundColor Cyan
Write-Host 'Keep DBI > Run MTP responder open on your Switch.'
Write-Host 'Looking for your Switch and AI1 Emu automatically...'
$shell=New-Object -ComObject Shell.Application
$app=$null
for($attempt=0;$attempt -lt 10 -and -not $app;$attempt++){
 foreach($device in $shell.Namespace(17).Items()){
  if($device.Name -notmatch 'Nintendo Switch'){continue}
  $root=$shell.Namespace($device.Path)
  foreach($storage in $root.Items()){
   if(-not $storage.IsFolder -or $storage.Name -notmatch 'SD'){continue}
   $switch=Child $storage.GetFolder 'switch'
   if($switch){$candidate=Choose-AppFolder $switch.GetFolder;if($candidate){$app=$candidate.GetFolder;break}}
  }
 }
 if(-not $app){Start-Sleep -Seconds 2}
}
if(-not $app){throw 'Open DBI > Run MTP responder, keep the USB cable connected, then double-click this file again. Windows can see the Switch but its SD storage is not available.'}
Write-Host ('Using data folder: '+$app.Self.Name) -ForegroundColor Cyan
$romFolder=Child $app 'roms';if(-not $romFolder){throw 'AI1 Emu has no roms folder on this SD card.'}
$cache=Join-Path $env:LOCALAPPDATA 'AI1 Emu/DBI-Artwork'
New-Item -ItemType Directory -Force $cache | Out-Null
# Each run gets a private workspace, so removed/replaced ROMs are never mixed.
$work=Join-Path $cache ([Guid]::NewGuid().ToString('N'))
foreach($dir in @('roms','covers','metadata','artwork-cache')){New-Item -ItemType Directory -Force (Join-Path $work $dir) | Out-Null}
$null=New-Item -ItemType File (Join-Path $work 'AI1Emu.nro')
$count=0;$seenPaths=@{}
foreach($rom in Walk-Files $romFolder.GetFolder){
 if($rom.Name -notmatch '\.(gba|gb|gbc|nes|sfc|smc|md|gen|smd|sms|gg|nds|dsi|z64|v64|n64|bin)$'){continue}
 $pathKey=$rom.Relative.ToLowerInvariant();if($seenPaths.ContainsKey($pathKey)){continue};$seenPaths[$pathKey]=$true
 Write-Host "Matching $($rom.Relative)..."
 $marker=Join-Path $work ('roms/'+$rom.Relative);New-Item -ItemType Directory -Force (Split-Path $marker -Parent) | Out-Null
 [IO.File]::WriteAllBytes($marker,[byte[]]@());$count++
}
if(-not $count){throw 'No supported games were found in switch/ai1emu/roms.'}
# Bring existing artwork/metadata into the workspace to preserve user choices.
foreach($dir in @('covers','metadata','artwork-cache')){
 $existing=Child $app $dir
 if($existing){foreach($file in Walk-Files $existing.GetFolder){if($file.Name -match '\.(png|jpg|jpeg|txt)$'){$marker=Join-Path $work ($dir+'/'+$file.Relative);New-Item -ItemType Directory -Force (Split-Path $marker -Parent) | Out-Null;[IO.File]::WriteAllBytes($marker,[byte[]]@())}}}
}
$downloads=Join-Path $cache 'downloaded-covers';New-Item -ItemType Directory -Force $downloads | Out-Null
foreach($saved in Get-ChildItem -LiteralPath $downloads -File -Recurse){$relative=$saved.FullName.Substring($downloads.Length+1);$dest=Join-Path $work ('artwork-cache/'+$relative);if(-not(Test-Path -LiteralPath $dest) -or (Get-Item -LiteralPath $dest).Length -eq 0){New-Item -ItemType Directory -Force (Split-Path $dest -Parent) | Out-Null;Copy-Item -LiteralPath $saved.FullName -Destination $dest -Force}}
$engine=[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($embeddedEngine))
& ([scriptblock]::Create($engine)) -RetroVerseFolder $work
foreach($saved in Get-ChildItem -LiteralPath (Join-Path $work 'artwork-cache') -File -Recurse){if($saved.Length -gt 0){$relative=$saved.FullName.Substring((Join-Path $work 'artwork-cache').Length+1);$dest=Join-Path $downloads $relative;New-Item -ItemType Directory -Force (Split-Path $dest -Parent) | Out-Null;Copy-Item -LiteralPath $saved.FullName -Destination $dest -Force}}
Write-Host 'Copying small covers back to your Switch...'
foreach($dir in @('artwork-cache','metadata')){
 $target=Ensure-Folder $app $dir
 foreach($file in Get-ChildItem -LiteralPath (Join-Path $work $dir) -File -Recurse){if($file.Length -gt 0){$relative=$file.FullName.Substring((Join-Path $work $dir).Length+1).Replace('\','/');$parent=Remote-Parent $target $relative;Copy-ToSwitch $parent $file.FullName}}
}
# Clear local filename markers; no ROM contents were copied.
foreach($file in Get-ChildItem -LiteralPath (Join-Path $work 'roms') -File -Recurse){Remove-Item -LiteralPath $file.FullName -Force}
Write-Host 'Done! Restart AI1 Emu to see your covers.' -ForegroundColor Green





