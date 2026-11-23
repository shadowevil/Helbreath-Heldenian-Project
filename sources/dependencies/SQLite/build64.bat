@echo off
setlocal enabledelayedexpansion

set "SQLITE_SRC=.\"
pushd "%SQLITE_SRC%" || exit /b 1

call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

del /f /q sqlite3.obj sqlite3.lib
cl /c /O2 /MT /DSQLITE_THREADSAFE=1 /DSQLITE_OMIT_LOAD_EXTENSION sqlite3.c
if errorlevel 1 exit /b 1

lib /OUT:sqlite3.lib /MACHINE:X64 sqlite3.obj
if errorlevel 1 exit /b 1

for %%H in (*.h) do copy /Y "%%H" "..\%%H" >nul
copy /Y "sqlite3.lib" "..\Server\Libs\sqlite3_x64.lib" >nul

popd
echo Done.
endlocal