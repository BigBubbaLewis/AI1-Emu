@echo off
setlocal
set "AI1_TARGET=%~1"
if not defined AI1_TARGET set "AI1_TARGET=%~dp0..\switch\ai1emu"
call "%~dp0AI1Emu-Prepare-Artwork.cmd" "%AI1_TARGET%"
