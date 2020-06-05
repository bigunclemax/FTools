echo off
cls
SET PATH=%~dp0;
SET unpkDir=unpackedVBF

rmdir /Q /S %unpkDir%
mkdir %unpkDir%

call:unpack_vbf
EXIT /B %ERRORLEVEL%

:unpack_vbf
	echo Unpacking VBF... please wait
	echo.
	for %%a in ("*14C026*.vbf") do (
		imgunpkr.exe -u -v %%a -o %unpkDir%/
	)
	echo.
	echo Done
EXIT /B 0