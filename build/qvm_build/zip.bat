@echo off
rem Set locations
set game_directory="G:\MAPPING\Star Trek Voyager Elite Force"
set zipper="%ProgramFiles%\7-Zip\7z.exe"


rem Verify exes exist
if not exist %zipper% echo Missing %zipper%&pause&exit

cd output
%zipper% a -bd -mx=9 -r -y -- tron_qvm.pk3 "vm\*"
cd ..
