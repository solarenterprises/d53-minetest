@echo off
setlocal enabledelayedexpansion

set config=stress-test-config.txt

goto :start

:func_write_config
echo address=%address% > %config%
echo port=%port% >> %config%
echo num_instances=%num_instances% >> %config%
goto :eof

:start
TITLE District53 Bots Config
echo Config (%config%)

:: Load config
for /f "tokens=1,2 delims==" %%a in (%config%) do (
    if "%%a"=="address" set address=%%b
    if "%%a"=="port" set port=%%b
    if "%%a"=="num_instances" set num_instances=%%b
)

if "%address%"=="" set address=localhost
if "%port%"=="" set port=30000
if "%num_instances%"=="" set num_instances=50


:: Ask the user if they want to update the address and port
set "update="
set /p update="Address (%address%)? : "
if /i "!update!" NEQ "" (
	set address=!update!
    call :func_write_config
)

set "update="
set /p update="Port (%port%)? : "
if /i "!update!" NEQ "" (
	set port=!update!
    call :func_write_config
)

set "update="
set /p update="How many instances (%num_instances%)? : "
if /i "!update!" NEQ "" (
	set num_instances=!update!
    call :func_write_config
)

echo name %name%
echo address %address%
echo port %port%
echo num_instances %num_instances%

set username=d53bot-%ComputerName%-
set password=test

if exist bin/Release/ (
	cd bin/Release/
) else (
	cd bin
)

TITLE District53 Bots (%num_instances%)

for /L %%x in (1, 1, %num_instances%) do (
	start /B "" district53.exe --norender --address %address% --port %port% --password %password% --name %username%%%x
)

PAUSE