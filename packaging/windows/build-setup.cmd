@echo off
rem build-setup.cmd — stage QCBridgeN.exe with its Qt and the NDI runtime, then Inno.
rem   Needs: a Release build in build-release (cmake -G Ninja), Qt 6.11.1 msvc2022_64,
rem   the NDI 6 SDK, Inno Setup 6. Run from anywhere; paths are relative to this file.
setlocal
set "QT=C:\Qt\6.11.1\msvc2022_64"
set "NDI=C:\Program Files\NDI\NDI 6 SDK"
set "BUILD=%~dp0..\..\build-release"
set "STAGE=%~dp0..\..\build\stage"
if not exist "%BUILD%\QCBridgeN.exe" ( echo ERROR: build-release\QCBridgeN.exe missing & exit /b 1 )
if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%"
copy /y "%BUILD%\QCBridgeN.exe" "%STAGE%\" >nul
"%QT%\bin\windeployqt.exe" --release --no-translations --no-opengl-sw --no-system-d3d-compiler --no-system-dxc-compiler --no-svg --no-network --no-pdf --skip-plugin-types qmltooling,generic,networkinformation,tls,imageformats,iconengines "%STAGE%\QCBridgeN.exe" > "%~dp0..\..\build\windeployqt.log" 2>&1 || ( echo ERROR: windeployqt failed & exit /b 1 )
copy /y "%NDI%\Bin\x64\Processing.NDI.Lib.x64.dll" "%STAGE%\" >nul || ( echo ERROR: NDI runtime DLL not found & exit /b 1 )
copy /y "%NDI%\Bin\x64\Processing.NDI.Lib.Licenses.txt" "%STAGE%\" >nul
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" /Q "%~dp0qcbridgen.iss" || ( echo ERROR: Inno failed & exit /b 1 )
echo SETUP OK
