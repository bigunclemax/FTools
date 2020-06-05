echo off
cls
SET PATH="%~dp0";%PATH%

call:convert_bmp
EXIT /B %ERRORLEVEL%

:convert_bmp
	echo Converting all BMP to EIF... please wait
	echo.
	for %%a in ("%~dp0/*.bmp") do (
		REM eifviewer -p "%%a" -o "%%~na.eif" -d 8
		eifviewer -p "%%a" -o "%%~na.eif" -s "%%a.pal" -d 16
	)
	echo Done
	echo.
EXIT /B 0