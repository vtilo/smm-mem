@echo off
setlocal
cd /d "%~dp0"
where cl >nul 2>nul
if errorlevel 1 (
  echo Run this from an x64 Visual Studio developer command prompt.
  exit /b 1
)
set "OUT=Work\build"
for %%d in ("%OUT%" "%OUT%\smm" "%OUT%\dxe" "%OUT%\common") do if not exist %%d mkdir %%d
del /q "%OUT%\smm\*.obj" "%OUT%\dxe\*.obj" "%OUT%\common\*.obj" 2>nul
set "EFI_CL=/nologo /c /O2 /Oi /GS- /GR- /EHs-c- /Zl /W4"
set "EFI_LD=/nologo /dll /nodefaultlib /machine:x64 /fixed:no /dynamicbase:no /nxcompat:no"
cl %EFI_CL% /Fo:%OUT%\common\common.obj src\firmware\common\common.c || exit /b 1
for %%f in (src\firmware\smm\*.c) do (
  cl %EFI_CL% /Fo:%OUT%\smm\%%~nf.obj "%%f"
  if errorlevel 1 exit /b 1
)
for %%f in (src\firmware\dxe\*.c) do (
  cl %EFI_CL% /Fo:%OUT%\dxe\%%~nf.obj "%%f"
  if errorlevel 1 exit /b 1
)
link %EFI_LD% /subsystem:EFI_BOOT_SERVICE_DRIVER /entry:SmmEntry /out:%OUT%\Smm.efi %OUT%\smm\*.obj %OUT%\common\*.obj || exit /b 1
link %EFI_LD% /subsystem:EFI_BOOT_SERVICE_DRIVER /entry:DxeEntry /out:%OUT%\Dxe.efi %OUT%\dxe\*.obj %OUT%\common\*.obj || exit /b 1
echo Built %OUT%\Smm.efi and %OUT%\Dxe.efi