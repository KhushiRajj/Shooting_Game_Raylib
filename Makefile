all:
	gcc main.c -o main.exe -lraylib -lopengl32 -lgdi32 -lwinmm

run: all
	./main.exe

clean:
	rm -f main.exe
