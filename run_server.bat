@echo off
TITLE Server

cd bin/Release/
district53.exe --server --gameid devtest --world ../../worlds/test_auth --config ../../minetest.conf

PAUSE