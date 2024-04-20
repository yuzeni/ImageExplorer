@echo off
mkdir build
pushd build
del .\ImageExplorer.exe
rem /Zi
cl /EHsc /std:c++20 /I..\raylib50 ..\src\ImageExplorer.cpp gdi32.lib msvcrt.lib ..\raylib50\raylib.lib user32.lib shell32.lib winmm.lib /link /libpath:..\raylib50 /NODEFAULTLIB:libcmt /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup
xcopy /e /i /y "..\resources" "resources"
.\ImageExplorer.exe
popd
