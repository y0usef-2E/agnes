@echo off
call "%~1" x64

set VISUAL_DSA=1
DOSKEY clear=cls

DOSKEY ls=dir
DOSKEY ll=dir

pushd build
cl "%~2" /Fe:agnes.exe
popd build