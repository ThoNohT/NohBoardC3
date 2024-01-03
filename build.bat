if exist bld.exe (
    bld.exe %*

) else (
    echo Bootstrapping build system.

    gcc -o bld.exe bld.c -ggdb 
    bld.exe %*
)
