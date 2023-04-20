@echo off
rem Set locations
set game_directory="G:\MAPPING\Star Trek Voyager Elite Force"
set game_exe=ioEF-cMod.x86_64.exe

rem Verify exe exist
if not exist %game_directory%\%game_exe% echo Missing %game_exe%&pause&exit


cd output
xcopy *.pk3 %game_directory%\tron /Y


start "" /D %game_directory% %game_exe% +fs_mod tron +map aeneon +bots 0