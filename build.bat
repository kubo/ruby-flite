@echo off

rem
rem Settings
rem

rem Location of git.exe
set GIT_EXE_DIR=c:\cygwin64\bin

rem Location of DevKit-mingw64-64-4.7.2-20130224-1432-sfx.exe
set DEVKIT_64_DIR=e:\ruby\devkit-mingw64-64

rem Location of DevKit-mingw64-32-4.7.2-20130224-1151-sfx.exe
set DEVKIT_32_DIR=e:\ruby\devkit-mingw64-32

rem Installation location of CMU flite compiled by devkit-mingw64-64
set PREFIX_64=e:\win64\usr\local

rem Installation location of CMU flite compiled by devkit-mingw64-32
set PREFIX_32=e:\win32\usr\local

rem
rem End of settings
rem

PATH=%PATH%;%GIT_EXE_DIR%

call %DEVKIT_64_DIR%\devkitvars.bat
del lib\flite_*.so
call :compile flite_210.so win64 %PREFIX_64% 213p242-64
call :compile flite_200.so win64 %PREFIX_64% 200p0-64
call :build_gem win64 %PREFIX_64%

call %DEVKIT_32_DIR%\devkitvars.bat
del lib\flite_*.so
call :compile flite_210.so win32 %PREFIX_32% 213p242-32
call :compile flite_200.so win32 %PREFIX_32% 200p0-32
call :build_gem win32 %PREFIX_32%

goto end

:compile
echo =====================================================
echo == Compiling %1 for %2
echo =====================================================
call pik use %4
cd ext\flite
set PREFIX=%3
ruby extconf.rb --with-flite-include=%PREFIX:\=/%/include --with-flite-lib=%PREFIX:\=/%/lib --with-win32-binary-gem
make clean
make
copy %1 ..\..\lib
cd ..\..
exit /b

:build_gem
echo =====================================================
echo == Building flite gem for %1
echo =====================================================
del lib\flite*.dll
copy %2\bin\flite*.dll lib
copy %2\bin\libmp3lame-0.dll lib
call gem build flite.gemspec -- current
exit /b

:end
