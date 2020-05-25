echo off
cls
SET PATH="%~dp0";%PATH%

call:convert_bmp
EXIT /B %ERRORLEVEL%

:convert_bmp
	echo Converting all BMP to EIF... please wait
	echo. 
	for %%a in ("%~dp0/*.bmp") do (
		eifviewer -p "%%a" -o "%%~na"
	)
	echo Done
	echo.
EXIT /B 0