param([Parameter(Mandatory=$true)][string]$InputPath,[Parameter(Mandatory=$true)][string]$OutputPath)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$image=[Drawing.Image]::FromFile($InputPath)
try {
 if($image.Width -gt 4096 -or $image.Height -gt 4096){throw 'Image too large'}
 $scale=[Math]::Min(1,512.0/[Math]::Max($image.Width,$image.Height))
 $bitmap=[Drawing.Bitmap]::new([Math]::Max(1,[int]($image.Width*$scale)),[Math]::Max(1,[int]($image.Height*$scale)))
 try {$g=[Drawing.Graphics]::FromImage($bitmap);try {$g.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic;$g.Clear([Drawing.Color]::FromArgb(36,39,46));$g.DrawImage($image,0,0,$bitmap.Width,$bitmap.Height)} finally {$g.Dispose()};$bitmap.Save($OutputPath,[Drawing.Imaging.ImageFormat]::Png)}finally{$bitmap.Dispose()}
}finally{$image.Dispose()}
