@echo off
setlocal

REM Compiler and filenames
set COMPILER=g++
set WINDRES=windres
set SRC=camera_gui.cpp
set RC=resource.rc
set RES_OBJ=resource.o
set OUTPUT=tk_camera_tweaker.exe

echo Compiling resource file...
%WINDRES% %RC% -o %RES_OBJ%
if %ERRORLEVEL% neq 0 (
    echo Failed to compile resource file!
    exit /b %ERRORLEVEL%
)

echo Compiling main application...
%COMPILER% %SRC% %RES_OBJ% -o %OUTPUT% -mwindows -static
if %ERRORLEVEL% neq 0 (
    echo Compilation failed!
    exit /b %ERRORLEVEL%
)

echo Build successful! Output: %OUTPUT%
exit /b 0
