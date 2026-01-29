@echo off

if not defined VISUAL_DSA (
	call prepare.bat > nul
)

if "%~1"=="" (
	echo "error: no filename was provided"
	exit /b
) else (
	set filename=%~1
)

call build.bat %filename%

if %errorlevel%==0 (
	pushd build
	call %filename%.exe
	popd build
)