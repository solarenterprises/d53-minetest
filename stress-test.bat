@echo off

set username=d53bot
set password=test
set num_instances=2

if "%address%"=="" set address=localhost
if "%port%"=="" set port=30000

echo name %name%
echo address %address%
echo port %port%

if exist bin/Release/ (
	cd bin/Release/
) else (
	cd bin
)

for /L %%x in (1, 1, %num_instances%) do (
	start /B "" district53.exe --norender --address %address% --port %port% --password %password% --name %username%%%x
)

PAUSE