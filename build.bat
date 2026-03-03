@echo off
call "D:\DevTools\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"
MSBuild WZMediaPlay.sln /p:Configuration=Debug /p:Platform=x64 /m /v:minimal
