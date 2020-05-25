echo off
cls
SET PATH="%~dp0";"%PATH%"
SET vbfFile=%1
SET unpkDir=unpackedVbf
SET unpkBin=unpackedBin

if "%vbfFile%"=="" (
	echo No VBF specified
	EXIT /B 0
)

call:pack_images "%vbfFile%"
EXIT /B %ERRORLEVEL% 

:pack_images
	echo Repacking EIFs into binary... please wait.
	echo.
	for %%a in ("%unpkDir%/%unpkBin%/%vbfFile%_section_1400000_*.json") do (
		for %%d in ("%unpkDir%/%vbfFile%_section_1400000_*.bin") do (
			ren "%unpkDir%\\%%d" "%%d.original"
			imgparcer.exe -p "%unpkDir%/%unpkBin%/%%a" -o "%unpkDir%/%%d.repacked" >nul 2>&1
			ren "%unpkDir%\\%%d.repacked" "%%d"
		)
	)
	call:pack_vbf
EXIT /B 0

:pack_vbf
	echo Repacking VBF... please wait.
	echo.
	vbfeditor.exe -p %unpkDir%/%vbfFile%_config.json -o %vbfFile%.patched
	for %%b in ("%unpkDir%/%vbfFile%_section_1400000_*.bin") do (
		ren "%unpkDir%\\%%b" "%%b.repacked"
		ren "%unpkDir%\\%%b.original" "%%b"
	)
	echo Done
	echo.
EXIT /B 0