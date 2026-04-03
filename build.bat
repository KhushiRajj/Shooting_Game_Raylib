@echo off
echo Compiling raylib program...
w64devkit\bin\gcc.exe main.c -o main.exe -lraylib -lopengl32 -lgdi32 -lwinmm
if %errorlevel% equ 0 (
    echo Compilation successful!
    echo Running the program...
    main.exe
) else (
    echo Compilation failed!
    pause
)
