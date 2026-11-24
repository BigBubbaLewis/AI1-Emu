param([string]$RetroVerseFolder,[switch]$Offline)
$ErrorActionPreference='Stop'
if(-not $RetroVerseFolder){$RetroVerseFolder=Join-Path $PSScriptRoot '../switch/retroverse'}
if(-not(Test-Path (Join-Path $RetroVerseFolder 'RetroVerse.nro'))){
 Add-Type -AssemblyName System.Windows.Forms
 $picker=New-Object Windows.Forms.FolderBrowserDialog
 $picker.Description='Select the retroverse folder inside switch on your SD card'
 if($picker.ShowDialog() -ne 'OK'){exit 0}
 $RetroVerseFolder=$picker.SelectedPath
}
$root=(Resolve-Path -LiteralPath $RetroVerseFolder).Path
if(-not(Test-Path -LiteralPath (Join-Path $root 'roms'))){throw 'This folder has no roms folder. Select switch/retroverse.'}
$db=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'gba-database.json') -Raw | ConvertFrom-Json
function Normalize-Title([string]$s){
 $s=$s.Normalize([Text.NormalizationForm]::FormD) -replace '\p{Mn}',''
 return ($s -replace '\([^)]*\)|\[[^\]]*\]','' -replace '[^a-zA-Z0-9]','').ToLowerInvariant()
}
$hashes=@{};$names=@{};$serials=@{};$normalized=@{}
foreach($entry in $db){$hashes[$entry.sha1]=$entry;$names[$entry.title]=$entry;if($entry.serial){$serials[$entry.serial]=@($serials[$entry.serial])+$entry};$key=Normalize-Title $entry.title;$normalized[$key]=@($normalized[$key])+$entry}
foreach($dir in @('covers','metadata','artwork-cache')){New-Item -ItemType Directory -Force (Join-Path $root $dir) | Out-Null}
[Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
Add-Type -AssemblyName System.Drawing
$report=@()
foreach($rom in Get-ChildItem -LiteralPath (Join-Path $root 'roms') -File | Where-Object {$_.Extension -ieq '.gba'}){
 $sha=[Security.Cryptography.SHA1]::Create();$inputStream=[IO.File]::OpenRead($rom.FullName);try{$digest=[BitConverter]::ToString($sha.ComputeHash($inputStream)).Replace('-','')}finally{$inputStream.Dispose();$sha.Dispose()};$match=$hashes[$digest];$method='SHA-1'
 if(-not $match){$match=$names[$rom.BaseName];$method='filename'}
 if(-not $match -and $rom.Length -ge 192){
  $stream=[IO.File]::OpenRead($rom.FullName)
  try{$header=New-Object byte[] 192;$null=$stream.Read($header,0,192)}finally{$stream.Dispose()}
  $id=[Text.Encoding]::ASCII.GetString($header,172,4);$options=@($serials[$id] | Where-Object {$_})
  if($options.Count){$revision=$header[188];$candidate=@($options | Where-Object {if($revision){$_.title -like "*(Rev $revision)*"}else{$_.title -notlike '*(Rev *'}});if($candidate.Count){$match=$candidate[0]}else{$match=$options[0]};$method='header serial (not checksum verified)'}
 }
 if(-not $match){
  $options=@($normalized[(Normalize-Title $rom.BaseName)] | Where-Object {$_})
  if($options.Count){$preferred=@($options | Where-Object {$_.title -like '*(USA*' -and $_.title -notlike '*(Rev *'});if($rom.BaseName -match '\((Europe|Japan)[^)]*\)'){$region=$Matches[1];$preferred=@($options | Where-Object {$_.title -like "*($region*"})};if($preferred.Count){$match=$preferred[0]}else{$match=$options[0]};$method='normalized filename (not checksum verified)'}
 }
 if(-not $match){Write-Host "$($rom.Name): Unknown game; using placeholder";$report+= [pscustomobject]@{filename=$rom.Name;result='Unrecognized'};continue}
 $key=$match.title -replace '[&*/:`<>?\\|]','_'
 $cache=Join-Path $root ('artwork-cache/'+($key -replace '^Metroid - Zero Mission \(USA, Australia\)$','Metroid - Zero Mission (USA)')+'.png')
 $custom=$false;foreach($ext in @('.png','.jpg','.jpeg')){if(Test-Path -LiteralPath (Join-Path $root ('covers/'+$rom.BaseName+$ext))){$custom=$true}}
 $result='Missing'
 if($custom){$result='Custom cover preserved'}elseif(Test-Path -LiteralPath $cache){$result='Cached'}elseif(-not $Offline){
  $url='https://raw.githubusercontent.com/libretro-thumbnails/Nintendo_-_Game_Boy_Advance/master/Named_Boxarts/'+[Uri]::EscapeDataString(($key -replace '^Metroid - Zero Mission \(USA, Australia\)$','Metroid - Zero Mission (USA)')+'.png')
  $temporary=$cache+'.download';$optimized=$cache+'.tmp.png'
  try{
   Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $temporary -TimeoutSec 20
   if((Get-Item -LiteralPath $temporary).Length -gt 8MB){throw 'Image download too large'}
   & (Join-Path $PSScriptRoot 'optimize-art.ps1') -InputPath $temporary -OutputPath $optimized
   [IO.File]::Move($optimized,$cache)
   [IO.File]::WriteAllText($cache+'.source.txt',$url+"`nPersonal cache; not cleared for redistribution.`n")
   $result='Downloaded and cached'
  }catch{$result='Unavailable: '+$_.Exception.Message}finally{foreach($temp in @($temporary,$optimized)){if(Test-Path -LiteralPath $temp){Remove-Item -LiteralPath $temp -Force}}}
 }
 $meta=Join-Path $root ('metadata/'+$rom.BaseName+'.txt')
 if(-not(Test-Path -LiteralPath $meta)){[IO.File]::WriteAllText($meta,"title=$($match.title)`nidentification=$method`n",(New-Object Text.UTF8Encoding($false)))}
 Write-Host "$($rom.Name): $result"
 $report+=[pscustomobject]@{filename=$rom.Name;title=$match.title;method=$method;result=$result}
}
ConvertTo-Json -InputObject @($report) -Depth 4 | Set-Content -LiteralPath (Join-Path $root 'artwork-report.json') -Encoding UTF8
Write-Host "Done. Safely eject your SD card when finished."



