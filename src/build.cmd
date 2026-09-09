@echo off
setlocal
cd /d "%~dp0.."
where cl >nul 2>nul
if errorlevel 1 exit /b 1
set "OUT=Work\build"
for %%d in ("%OUT%" "%OUT%\smm" "%OUT%\shared") do if not exist %%d mkdir %%d
set "FLAGS=/nologo /c /Oi /GS- /GR- /EHs-c- /Zl /W4"
set "LINKFLAGS=/nologo /dll /nodefaultlib /machine:x64 /fixed:no /dynamicbase:no /nxcompat:no /subsystem:EFI_BOOT_SERVICE_DRIVER"
cl %FLAGS% /O2 /Fo:%OUT%\shared\Common.obj src\firmware\common.c || exit /b 1
cl %FLAGS% /O2 /Fo:%OUT%\Smm.obj src\Smm.c || exit /b 1
for %%f in (src\smm\*.c) do (
  cl %FLAGS% /O2 /Fo:%OUT%\smm\%%~nf.obj "%%f"
  if errorlevel 1 exit /b 1
)
cl %FLAGS% /O1 /Fo:%OUT%\Dxe.obj src\Dxe.c || exit /b 1
link %LINKFLAGS% /entry:SmmEntry /out:%OUT%\Smm.efi %OUT%\Smm.obj %OUT%\smm\*.obj %OUT%\shared\*.obj || exit /b 1
link %LINKFLAGS% /entry:DxeEntry /out:%OUT%\Dxe.efi %OUT%\Dxe.obj || exit /b 1
cl /nologo /W4 /O2 /MT /DUNICODE /D_UNICODE /Fo:%OUT%\Client.obj /Fe:%OUT%\Client.exe src\Client.c
exit /b %errorlevel%
