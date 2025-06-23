# Makefile para Ubuntu con doble terminal y prints claros

CC=gcc
CFLAGS=-Wall -O2 -D_POSIX_C_SOURCE=200809L
TARGET=p1-dataProgram
CSV=./Others/muse.csv

all: $(TARGET)

$(TARGET): p1-dataProgram.c
	$(CC) $(CFLAGS) -o $(TARGET) p1-dataProgram.c

run-searcher:
	@echo "🧠 Ejecutando el proceso de búsqueda en nueva terminal..."
	gnome-terminal -- bash -c "./$(TARGET) searcher $(CSV); exec bash"

run-interface:
	@echo "🎧 Ejecutando la interfaz interactiva en nueva terminal..."
	gnome-terminal -- bash -c "./$(TARGET) interface $(CSV); exec bash"

run-both:
	@echo "🚀 Ejecutando ambos procesos..."
	make run-searcher
	sleep 1
	make run-interface

clean:
	rm -f $(TARGET) *.o /tmp/search_req /tmp/search_res
