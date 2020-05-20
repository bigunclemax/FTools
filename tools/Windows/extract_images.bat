echo off
cls
SET PATH=%~dp0;%PATH%
SET vbfFile=%1
SET unpkDir=unpackedVbf
SET unpkBin=unpackedBin
SET eifsFolder=eifs
SET bmpFolder=bmp

:: Cleaning Folder
rmdir /Q /S %unpkDir%

mkdir %unpkDir%


:unpack_vbf
	echo Unpacking VBF... please wait
	echo. 
	vbfeditor -u %vbfFile% -o %unpkDir%/
	call:unpack_images
EXIT /B 0

:unpack_images
	cd %unpkDir%
	mkdir %unpkBin%
	cd ..
	echo Extracing zipped EIFs from binary.
	echo. 
	for %%a in ("%unpkDir%/%vbfFile%_section_1400000_*.bin") do (
		imgparcer -u %unpkDir%/%%a -o %unpkDir%/%unpkBin%
	)
	call:extract_eifs
EXIT /B 0

:extract_eifs
	cd %unpkDir%/%unpkBin%
	mkdir %eifsFolder%
	mkdir %bmpFolder%
	cd zip
	echo. 
	echo Unzipping EIFs files... please wait.
	echo. 
	for %%a in ("*.zip") do (
		tar.exe -xf "%%a" -C ../%eifsFolder%
	)
	cd ../%eifsFolder%
	echo Converting EIFs files to BMP... please wait.
	echo. 
	for %%a in ("*.eif") do (
		eifviewer -u %%a -o ../%bmpFolder%/%%a.bmp
	)
	echo Done
	echo.
EXIT /B 0

call:unpack_vbf %vbfFile%
