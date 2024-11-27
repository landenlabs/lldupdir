@echo off


cd lldupdir-ms

@echo Clean up remove x64
rmdir /s  x64

@echo.
@echo Build release target
F:\opt\VisualStudio\2022\Preview\Common7\IDE\devenv.exe lldupdir.sln /Build  "Release|x64"
cd ..

@echo Copy Release to d:\opt\bin
copy lldupdir-ms\x64\Release\lldupdir.exe d:\opt\bin\lldupdir.exe

@echo.
@echo Compare md5 hash
cmp -h lldupdir-ms\x64\Release\lldupdir.exe d:\opt\bin\lldupdir.exe
ld -a d:\opt\bin\lldupdir.exe

