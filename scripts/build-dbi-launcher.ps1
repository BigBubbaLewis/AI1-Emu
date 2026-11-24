$ErrorActionPreference='Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
$cmd=Get-Content tools/Prepare-Artwork-Standalone.cmd -Raw
$toolVersion=[regex]::Match((Get-Content source/app_version.h -Raw),'AI1EMU_VERSION "([0-9.]+)"').Groups[1].Value
$cmd=[regex]::Replace($cmd,'(?m)^setlocal\r?$',"setlocal`r`necho AI1 Emu artwork tools v$toolVersion")
$engine=Get-Content tools/Prepare-Multi-Artwork.ps1 -Raw
$database=Get-Content tools/databases/multi-database.json -Raw
$dbMemory=New-Object IO.MemoryStream;$dbZip=New-Object IO.Compression.GZipStream($dbMemory,[IO.Compression.CompressionMode]::Compress,$true);$dbBytes=[Text.Encoding]::UTF8.GetBytes($database);$dbZip.Write($dbBytes,0,$dbBytes.Length);$dbZip.Dispose();$dbEncoded=[Convert]::ToBase64String($dbMemory.ToArray());$dbMemory.Dispose()
$engine=$engine.Replace('$systems=$databaseJson | ConvertFrom-Json',('$dbStream=New-Object IO.MemoryStream(,[Convert]::FromBase64String('''+$dbEncoded+'''));$dbGzip=New-Object IO.Compression.GZipStream($dbStream,[IO.Compression.CompressionMode]::Decompress);$dbReader=New-Object IO.StreamReader($dbGzip);$databaseJson=$dbReader.ReadToEnd();$dbReader.Dispose();$systems=$databaseJson | ConvertFrom-Json'))
# The database is embedded: consumers need neither Node.js nor adjacent scripts.
$engine=$engine.Replace('if(-not $databaseJson){$databaseJson=[IO.File]::ReadAllText((Join-Path $PSScriptRoot ''databases/multi-database.json''))}','')
$directHeader=($cmd -split ':RETROVERSE_PAYLOAD\r?\n',2)[0]
$directMemory=New-Object IO.MemoryStream;$directZip=New-Object IO.Compression.GZipStream($directMemory,[IO.Compression.CompressionMode]::Compress,$true);$directBytes=[Text.Encoding]::UTF8.GetBytes($engine);$directZip.Write($directBytes,0,$directBytes.Length);$directZip.Dispose()
($directHeader+":RETROVERSE_PAYLOAD`r`n"+[Convert]::ToBase64String($directMemory.ToArray())) | Set-Content tools/AI1Emu-Prepare-Artwork.cmd -Encoding ascii
$directMemory.Dispose()
$wrapper='$embeddedEngine='''+[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($engine))+"'`r`n"+(Get-Content tools/DBI-Artwork.ps1 -Raw)
$tokens=$null;$errors=$null;[Management.Automation.Language.Parser]::ParseInput($wrapper,[ref]$tokens,[ref]$errors) | Out-Null;if($errors){throw ($errors | Out-String)}
$memory=New-Object IO.MemoryStream;$gzip=New-Object IO.Compression.GZipStream($memory,[IO.Compression.CompressionMode]::Compress,$true);$bytes=[Text.Encoding]::UTF8.GetBytes($wrapper);$gzip.Write($bytes,0,$bytes.Length);$gzip.Dispose();$encoded=[Convert]::ToBase64String($memory.ToArray());$memory.Dispose()
$header=($cmd -split ':RETROVERSE_PAYLOAD\r?\n',2)[0]
$header=$header.Replace('& ([scriptblock]::Create($code)) -RetroVerseFolder $env:RETROVERSE_ART_TARGET -Offline:($env:RETROVERSE_ART_OFFLINE -eq ''1'')','& ([scriptblock]::Create($code))')
($header+":RETROVERSE_PAYLOAD`r`n"+$encoded) | Set-Content tools/AI1Emu-Automatic-Covers-DBI.cmd -Encoding ascii
Write-Output 'Fast single-file DBI launcher built; syntax verified.'



