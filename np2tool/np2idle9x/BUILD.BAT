@echo off
rem Build all NP2IDLE components.
rem Run in an environment where the Windows 98 DDK/MASM and VC6 tools are available.

echo Cleaning previous outputs...
nmake /f MAKEFILE clean
if errorlevel 1 goto error

echo.
echo Building NP2IDLE.VXD, NP2ICTRL.EXE, NP2ITRAY.EXE and SETUP.EXE...
nmake /f MAKEFILE all
if errorlevel 1 goto error

echo.
echo Build complete.
echo   NP2IDLE.VXD
echo   NP2ICTRL.EXE
echo   NP2ITRAY.EXE
echo   SETUP.EXE
goto end

:error
echo.
echo Build failed.
echo Make sure ML.EXE, CL.EXE, LINK.EXE, RC.EXE, NMAKE.EXE,
echo VMM.INC and VWIN32.INC are reachable.

:end
