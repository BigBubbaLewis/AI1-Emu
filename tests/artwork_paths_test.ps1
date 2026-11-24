$ErrorActionPreference='Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
$tokens=$null;$errors=$null;$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PWD 'tools/DBI-Artwork.ps1'),[ref]$tokens,[ref]$errors)
if($errors){throw ($errors | Out-String)}
foreach($name in @('Child','Walk-Files','Has-SupportedGames','Choose-AppFolder')){$definition=$ast.Find({param($node)$node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name},$true);Invoke-Expression $definition.Extent.Text}
function File([string]$name){[pscustomobject]@{Name=$name;IsFolder=$false}}
function Folder([string]$name,[object[]]$children){$folder=[pscustomobject]@{Children=$children};$folder | Add-Member ScriptMethod Items {return $this.Children};return [pscustomobject]@{Name=$name;IsFolder=$true;GetFolder=$folder}}
foreach($ext in @('gba','gb','gbc','nes','sfc','md','sms','gg','nds','dsi','z64','v64','n64')){
 $nested=Folder 'nested' @((File "test.$ext"))
 $roms=Folder 'roms' @($nested)
 $current=Folder 'ai1emu' @($roms)
 $legacy=Folder 'retroverse' @((Folder 'roms' @((File 'old.gba'))))
 $switch=Folder 'switch' @($current,$legacy)
 if((Choose-AppFolder $switch.GetFolder).Name -ne 'ai1emu'){throw "Incorrect folder for $ext"}
}
$current=Folder 'ai1emu' @((Folder 'roms' @((File 'README.txt'))))
$switch=Folder 'switch' @($current,$legacy)
if((Choose-AppFolder $switch.GetFolder).Name -ne 'retroverse'){throw 'Legacy fallback failed'}
$legacy=Folder 'retroverse' @((Folder 'roms' @()))
$switch=Folder 'switch' @($current,$legacy)
if((Choose-AppFolder $switch.GetFolder).Name -ne 'ai1emu'){throw 'Empty folder preference failed'}
Write-Output 'PASS: DBI/app folder agreement for every console, nested games, legacy fallback and empty folders'
