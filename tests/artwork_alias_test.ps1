$ErrorActionPreference='Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
$tokens=$null;$errors=$null;$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $pwd 'tools/Prepare-Multi-Artwork.ps1'),[ref]$tokens,[ref]$errors)
if($errors){throw ($errors | Out-String)}
$definition=$ast.Find({param($node)$node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Receive-Boxart'},$true);Invoke-Expression $definition.Extent.Text
$script:requests=@();$script:mode='alias'
function Invoke-WebRequest {param($UseBasicParsing,$Uri,$OutFile,$TimeoutSec)
 $script:requests+=,$Uri
 if($script:mode -eq 'escape'){[IO.File]::WriteAllText($OutFile,'../../private.png');return}
 if($script:mode -eq 'cycle'){[IO.File]::WriteAllText($OutFile,'Alias.png');return}
 if($Uri -match 'Alias.png$'){[IO.File]::WriteAllText($OutFile,'Actual.png')}else{[IO.File]::WriteAllBytes($OutFile,[byte[]]@(137,80,78,71,13,10,26,10))}
}
# A switch-style parameter is needed for the real helper call.
Remove-Item Function:Invoke-WebRequest
function Invoke-WebRequest {param([switch]$UseBasicParsing,$Uri,$OutFile,$TimeoutSec)
 $script:requests+=,$Uri
 if($script:mode -eq 'escape'){[IO.File]::WriteAllText($OutFile,'../../private.png');return}
 if($script:mode -eq 'cycle'){[IO.File]::WriteAllText($OutFile,'Alias.png');return}
 if($Uri -match 'Alias.png$'){[IO.File]::WriteAllText($OutFile,'Actual.png')}else{[IO.File]::WriteAllBytes($OutFile,[byte[]]@(137,80,78,71,13,10,26,10))}
}
$out=Join-Path $pwd 'build/artwork-alias-test.download'
$url=Receive-Boxart 'Nintendo_-_Game_Boy_Color' 'Alias' $out
if($requests.Count -ne 2 -or $url -notmatch '/Actual.png$'){throw 'Alias did not resolve to the actual image'}
foreach($mode in @('escape','cycle')){$script:mode=$mode;$rejected=$false;try{$null=Receive-Boxart 'Nintendo_-_Game_Boy_Color' 'Alias' $out}catch{$rejected=$true};if(-not $rejected){throw "Unsafe alias accepted: $mode"}}
Write-Output 'PASS: symlink artwork follows a trusted image basename; traversal and cycles rejected'
