SET PATH=%~dp0;
SET unpkDir=unpackedVBF

call:unpack_vbf
EXIT /B %ERRORLEVEL%

:unpack_vbf
	echo Repacking VBF... please wait
	echo.
	for %%a in ("*14C026*.vbf") do (
		imgunpkr.exe -p -v "%%a" -c "%unpkDir%/_config.json"
	ren patched.vbf %%a.patched
	)
	echo.
	echo Done
EXIT /B 0