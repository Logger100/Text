@echo off
rem Do not edit! This batch file is created by CASIO fx-9860G SDK.

if exist debug\*.obj  del debug\*.obj
if exist TEXT.G1A  del TEXT.G1A

cd debug
if exist FXADDINror.bin  del FXADDINror.bin
"C:\CASIOMAKE\OS\SH\Bin\Hmake.exe" Addin.mak
cd ..
if not exist debug\FXADDINror.bin  goto error

"C:\CASIOMAKE\Tools\MakeAddinHeader363.exe" "C:\Users\Ann Marie\Desktop\CASIO\homebrew projects\Text"
if not exist TEXT.G1A  goto error
echo Build has completed.
goto end

:error
echo Build was not successful.

:end

