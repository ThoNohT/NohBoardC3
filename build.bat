@ECHO OFF

if exist bld.exe (
    bld.exe %*
    if ERRORLEVEL 27 GOTO :bootstrap
) else (
:bootstrap
    echo Bootstrapping build system.

    gcc -o bld.exe bld.c -ggdb 
    bld.exe %*
)
