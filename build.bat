@echo off

PATH=%PATH%;c:\cygwin64\bin

call e:\ruby\devkit-mingw64-64\devkitvars.bat
del lib\flite_*.so
call :compile flite_210.so win64 213p242-64
call :compile flite_200.so win64 200p0-64
call :build_gem win64

call e:\ruby\devkit-mingw64-32\devkitvars.bat
del lib\flite_*.so
call :compile flite_210.so win32 213p242-32
call :compile flite_200.so win32 200p0-32
call :build_gem win32

goto end

:compile
echo =====================================================
echo == Compiling %1 for %2
echo =====================================================
call pik use %3
cd ext\flite
ruby extconf.rb --with-flite-include=e:/flite/flite-2.0.0-%2/usr/local/include --with-flite-lib=e:/flite/flite-2.0.0-%2/usr/local/lib
make clean
make
copy %1 ..\..\lib
cd ..\..
exit /b

:build_gem
echo =====================================================
echo == Building flite gem for %1
echo =====================================================
call gem build flite.gemspec -- current
exit /b

:end
