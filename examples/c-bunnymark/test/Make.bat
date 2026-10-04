@echo off

REM prepare the script to be run from another directory
pushd %~dp0

REM create bin folder if non exiting, since
REM the development tools will not create it themselves
if not exist bin mkdir bin

echo.
echo Pack the ROM
echo --------------------------
packrom "CBunnymark.xml" -o "bin\CBunnymark.v32" || goto :failed
goto :succeeded

:failed
popd
echo.
echo BUILD FAILED
exit /b %errorlevel%

:succeeded
popd
echo.
echo BUILD SUCCESSFUL
exit /b

@echo on
