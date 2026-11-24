param([string]$RetroVerseFolder,[switch]$Offline)
$ErrorActionPreference='Stop'
if(-not $RetroVerseFolder){throw 'Pass the switch/ai1emu folder to the artwork tool.'}
$root=(Resolve-Path -LiteralPath $RetroVerseFolder).Path
if(-not $databaseJson){$databaseJson=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'databases/multi-database.json'))}
$systems=$databaseJson | ConvertFrom-Json
$hackTitles=@{
 'generation3evolved'='Generation 3 Evolved'
 'pokemoncrystaladvanceredux'='Pokémon Crystal Advance Redux'
 'pokemonunbound'='Pokémon Unbound'
}
$gbaSystem=$systems | Where-Object {$_.slug -eq 'gba'}
foreach($title in $hackTitles.Values){$gbaSystem.games+= [pscustomobject]@{title=$title}}
function New-HackCover([string]$title,[string]$destination){
 $bitmap=New-Object Drawing.Bitmap 360,480
 $graphics=[Drawing.Graphics]::FromImage($bitmap)
 $accent=New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(98,196,182))
 $white=New-Object Drawing.SolidBrush ([Drawing.Color]::White)
 $heading=New-Object Drawing.Font 'Segoe UI',25,([Drawing.FontStyle]::Bold)
 $small=New-Object Drawing.Font 'Segoe UI',12
 try{
  $graphics.Clear([Drawing.Color]::FromArgb(24,29,38))
  $graphics.TextRenderingHint=[Drawing.Text.TextRenderingHint]::AntiAliasGridFit
  $graphics.FillRectangle($accent,24,28,312,6)
  $graphics.DrawString('GAME BOY ADVANCE',$small,$accent,24,54)
  $graphics.DrawString($title,$heading,$white,(New-Object Drawing.RectangleF 24,145,312,240))
  $graphics.DrawString('FAN ROM HACK',$small,$accent,24,414)
  $bitmap.Save($destination,[Drawing.Imaging.ImageFormat]::Png)
 }finally{$small.Dispose();$heading.Dispose();$white.Dispose();$accent.Dispose();$graphics.Dispose();$bitmap.Dispose()}
}
function Normalize-Title([string]$s){return (($s.Normalize([Text.NormalizationForm]::FormD) -replace '\p{Mn}','' -replace '\([^)]*\)|\[[^\]]*\]','' -replace '[^a-zA-Z0-9]','').ToLowerInvariant())}
function Platform-Of($rom){
 if($rom.Length -gt 0 -and $rom.Extension -in @('.gb','.nds','.bin')){
  $f=[IO.File]::OpenRead($rom.FullName);try{
   if($rom.Extension -ieq '.gb'){$null=$f.Seek(0x143,[IO.SeekOrigin]::Begin);if(($f.ReadByte() -band 0x80) -ne 0){return 'gbc'}}
   if($rom.Extension -ieq '.nds'){$null=$f.Seek(0x12,[IO.SeekOrigin]::Begin);if($f.ReadByte() -eq 3){return 'dsi'}}
   if($rom.Extension -ieq '.bin'){$null=$f.Seek(0x100,[IO.SeekOrigin]::Begin);$magic=New-Object byte[] 4;$n=$f.Read($magic,0,4);if($n -eq 4 -and [Text.Encoding]::ASCII.GetString($magic) -eq 'SEGA'){return 'genesis'}}
  }finally{$f.Dispose()}
 }
 switch($rom.Extension.ToLowerInvariant()){
 '.gba'{return 'gba'} '.gb'{return 'gb'} '.gbc'{return 'gbc'} '.nes'{return 'nes'} '.sfc'{return 'snes'} '.smc'{return 'snes'}
 '.z64'{return 'n64'} '.v64'{return 'n64'} '.n64'{return 'n64'} '.md'{return 'genesis'} '.gen'{return 'genesis'} '.smd'{return 'genesis'} '.sms'{return 'sms'} '.gg'{return 'gg'} '.dsi'{return 'dsi'}
 '.nds'{if($rom.Directory.Name -eq 'dsi'){return 'dsi'};return 'nds'}
 '.bin'{if($rom.Directory.Name -in @('genesis','megadrive')){return 'genesis'}}
 };return $null
}
function Select-ArtworkTitle($system,[string]$title,[string]$romName){
 $safe=$title -replace '[&*/:`<>?\\|]','_'
 if($system.boxarts -contains $safe){return $safe}
 $key=Normalize-Title $safe;$choices=@($system.boxarts | Where-Object {(Normalize-Title $_) -eq $key})
 if(-not $choices.Count){return $null}
 $region='USA';if($romName -match '\((Europe|Japan)[^)]*\)'){$region=$Matches[1]}
 $preferred=@($choices | Where-Object {$_ -like "*($region*" -and $_ -notmatch '\((Beta|Proto|Rev )'})
 if($preferred.Count){return $preferred[0]};return $choices[0]
}
function Test-CachedArtwork([string]$path){
 if(-not(Test-Path -LiteralPath $path)){return $false};$item=Get-Item -LiteralPath $path;if($item.Length -lt 16 -or $item.Length -gt 8MB){return $false}
 $image=$null;try{$image=[Drawing.Image]::FromFile($path);return $image.Width -gt 0 -and $image.Height -gt 0 -and $image.Width -le 2048 -and $image.Height -le 2048}catch{return $false}finally{if($image){$image.Dispose()}}
}
function Receive-Boxart([string]$repository,[string]$title,[string]$destination){
 $visited=@{}
 for($hop=0;$hop -lt 5;$hop++){
  if($visited.ContainsKey($title)){throw 'Artwork alias cycle'};$visited[$title]=$true
  $url='https://raw.githubusercontent.com/libretro-thumbnails/'+$repository+'/master/Named_Boxarts/'+[Uri]::EscapeDataString($title+'.png')
  Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $destination -TimeoutSec 10
  $file=Get-Item -LiteralPath $destination;if($file.Length -gt 8MB){throw 'Artwork exceeds size limit'}
  $stream=[IO.File]::OpenRead($destination);try{$head=New-Object byte[] 8;$n=$stream.Read($head,0,8)}finally{$stream.Dispose()}
  if(($n -eq 8 -and [BitConverter]::ToString($head) -eq '89-50-4E-47-0D-0A-1A-0A') -or ($n -ge 3 -and $head[0] -eq 255 -and $head[1] -eq 216 -and $head[2] -eq 255)){return $url}
  # GitHub raw serves a symbolic link as its short target filename, not pixels.
  # Follow only another PNG basename in this same trusted repository directory.
  if($file.Length -gt 1024){throw 'Artwork response is not a supported image'}
  $target=[Text.Encoding]::UTF8.GetString([IO.File]::ReadAllBytes($destination)).Trim()
  if($target -notmatch '^[^/\\\r\n<>:"|?*]+\.png$' -or $target -in @('.png','..png')){throw 'Invalid artwork alias response'}
  $title=$target.Substring(0,$target.Length-4)
 }
 throw 'Artwork alias depth exceeded'
}
[Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
Add-Type -AssemblyName System.Drawing
$indexes=@{};$report=@()
foreach($rom in Get-ChildItem -LiteralPath (Join-Path $root 'roms') -File -Recurse){
 $slug=Platform-Of $rom;if(-not $slug){continue}
 if(-not $indexes.ContainsKey($slug)){
  $system=$systems | Where-Object {$_.slug -eq $slug};$names=@{};$hashes=@{};$normalized=@{}
  foreach($g in $system.games){$names[$g.title]=$g;if($g.sha1){$hashes[$g.sha1]=$g};$key=Normalize-Title $g.title;$normalized[$key]=@($normalized[$key])+$g}
  $indexes[$slug]=@{system=$system;names=$names;hashes=$hashes;normalized=$normalized}
 }
 $index=$indexes[$slug];$match=$null;$method='filename'
 # DBI mode supplies zero-byte filename markers: never read or transfer ROMs.
 if($rom.Length -gt 0){$sha=[Security.Cryptography.SHA1]::Create();$stream=[IO.File]::OpenRead($rom.FullName);try{$digest=[BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','');$match=$index.hashes[$digest];if($match){$method='SHA-1'}}finally{$stream.Dispose();$sha.Dispose()}}
 if(-not $match){$match=$index.names[$rom.BaseName]}
 if(-not $match){$choices=@($index.normalized[(Normalize-Title $rom.BaseName)] | Where-Object {$_});if($choices.Count){$region='USA';if($rom.BaseName -match '\((Europe|Japan)[^)]*\)'){$region=$Matches[1]};$preferred=@($choices | Where-Object {$_.title -like "*($region*" -and $_.title -notlike '*(Rev *'});if($preferred.Count){$match=$preferred[0]}else{$match=$choices[0]};$method='normalized filename (not checksum verified)'}}
 $artSystem=$index.system
 if(-not $match -and $slug -in @('gb','gbc')){
  $other=$systems | Where-Object {$_.slug -eq $(if($slug -eq 'gb'){'gbc'}else{'gb'})};$key=Normalize-Title $rom.BaseName
  $alternatives=@($other.games | Where-Object {(Normalize-Title $_.title) -eq $key})
  if($alternatives.Count){$region='USA';if($rom.BaseName -match '\((Europe|Japan)[^)]*\)'){$region=$Matches[1]};$preferred=@($alternatives | Where-Object {$_.title -like "*($region*" -and $_.title -notlike '*(Rev *'});$match=if($preferred.Count){$preferred[0]}else{$alternatives[0]};$artSystem=$other;$method='GB/GBC filename fallback (not checksum verified)'}
 }
 if(-not $match){$actual=Select-ArtworkTitle $index.system $rom.BaseName $rom.BaseName;if($actual){$match=[pscustomobject]@{title=$actual};$method='artwork catalogue filename (not checksum verified)'}}
 if(-not $match){Write-Host "$($rom.Name): Unknown game; using placeholder";continue}
 $key=$match.title -replace '[&*/:`<>?\\|]','_'
 $artTitle=Select-ArtworkTitle $artSystem $match.title $rom.BaseName
 if(-not $artTitle -and $slug -in @('gb','gbc')){$other=$systems | Where-Object {$_.slug -eq $(if($slug -eq 'gb'){'gbc'}else{'gb'})};$alternative=Select-ArtworkTitle $other $match.title $rom.BaseName;if($alternative){$artSystem=$other;$artTitle=$alternative}}

 foreach($dir in @('covers','metadata','artwork-cache')){New-Item -ItemType Directory -Force (Join-Path $root "$dir/$slug") | Out-Null}
 $cache=Join-Path $root "artwork-cache/$slug/$key.png";$custom=$false
 foreach($ext in @('.png','.jpg','.jpeg')){if((Test-Path -LiteralPath (Join-Path $root "covers/$slug/$($rom.BaseName)$ext")) -or ($slug -eq 'gba' -and (Test-Path -LiteralPath (Join-Path $root "covers/$($rom.BaseName)$ext")))){$custom=$true}}
 $legacy=Join-Path $root "artwork-cache/$key.png";$result='Missing (offline)'
 if($custom){$result='Custom cover preserved'}elseif((Test-CachedArtwork $cache) -or ($slug -eq 'gba' -and (Test-CachedArtwork $legacy))){$result='Cached'}elseif(-not $Offline -and $artTitle){
  $url='https://raw.githubusercontent.com/libretro-thumbnails/'+$artSystem.repository+'/master/Named_Boxarts/'+[Uri]::EscapeDataString($artTitle+'.png')
  $temporary=$cache+'.download';$optimized=$cache+'.tmp.png'
  try{
   $url=Receive-Boxart $artSystem.repository $artTitle $temporary
   if((Get-Item -LiteralPath $temporary).Length -gt 8MB){throw 'Artwork exceeds size limit'}
   $image=[Drawing.Image]::FromFile($temporary);try{$scale=[Math]::Min(1,512.0/[Math]::Max($image.Width,$image.Height));$bmp=New-Object Drawing.Bitmap ([int][Math]::Max(1,$image.Width*$scale)),([int][Math]::Max(1,$image.Height*$scale));$gfx=[Drawing.Graphics]::FromImage($bmp);try{$gfx.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic;$gfx.DrawImage($image,0,0,$bmp.Width,$bmp.Height);$bmp.Save($optimized,[Drawing.Imaging.ImageFormat]::Png)}finally{$gfx.Dispose();$bmp.Dispose()}}finally{$image.Dispose()}
   [IO.File]::Copy($optimized,$cache,$true);[IO.File]::WriteAllText($cache+'.source.txt',$url+"`nPersonal cache; redistribution permission has not been established.`n");$result='Downloaded and cached'
  }catch{$result='Unavailable: '+$_.Exception.Message}finally{foreach($temp in @($temporary,$optimized)){if(Test-Path -LiteralPath $temp){Remove-Item -LiteralPath $temp -Force}}}
 }
 if(-not $custom -and -not(Test-CachedArtwork $cache) -and $slug -eq 'gba' -and $hackTitles.ContainsKey((Normalize-Title $match.title))){
  New-HackCover $match.title $cache
  $result='Generated title cover (fan ROM hack)'
 }
 if(-not $Offline -and -not $artTitle -and $result -eq 'Missing (offline)'){$result='Cover not available in artwork catalogue'}
 # DBI cannot inspect a .gb cartridge header without transferring the ROM.
 # Cache the small image in both GB/GBC locations so the real header on Switch
 # can select the correct platform. Never replace an existing image.
 if($rom.Extension -ieq '.gb' -and (Test-CachedArtwork $cache)){$otherSlug=if($slug -eq 'gb'){'gbc'}else{'gb'};$mirror=Join-Path $root "artwork-cache/$otherSlug/$key.png";if(-not(Test-CachedArtwork $mirror)){New-Item -ItemType Directory -Force (Split-Path $mirror -Parent) | Out-Null;Copy-Item -LiteralPath $cache -Destination $mirror -Force}}
 $meta=Join-Path $root "metadata/$slug/$($rom.BaseName).txt";if(-not(Test-Path -LiteralPath $meta)){[IO.File]::WriteAllText($meta,"title=$($match.title)`nidentification=$method`n",(New-Object Text.UTF8Encoding($false)))}
 if($rom.Extension -ieq '.gb'){$otherSlug=if($slug -eq 'gb'){'gbc'}else{'gb'};$otherMeta=Join-Path $root "metadata/$otherSlug/$($rom.BaseName).txt";if(-not(Test-Path -LiteralPath $otherMeta)){New-Item -ItemType Directory -Force (Split-Path $otherMeta -Parent) | Out-Null;Copy-Item -LiteralPath $meta -Destination $otherMeta}}
 $relative=$rom.FullName.Substring((Join-Path $root 'roms').Length+1)
 Write-Host "$relative`: $result";$report+=[pscustomobject]@{filename=$rom.Name;relativePath=$relative;platform=$slug;title=$match.title;result=$result}
}
$report | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'artwork-report.json') -Encoding UTF8
Write-Host 'Done. Artwork is cached for offline gameplay.'
