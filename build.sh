#!/bin/sh
cd /d/VsRaylib
gcc main.c -o main.exe -lraylib -lopengl32 -lgdi32 -lwinmm
if [ $? -eq 0 ]; then
    echo "Compilation successful!"
    ./main.exe
else
    echo "Compilation failed!"
fi
