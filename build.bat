@echo off

if not defined VISUAL_DSA (
	call prepare.bat > nul
)

if "%~1"=="" (
	set filename=main
) else (
	set filename=%~1
)

if not exist build mkdir build

pushd build
clang-cl ..\%filename%.c
popd build