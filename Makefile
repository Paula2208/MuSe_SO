# Makefile para Ubuntu con doble terminal y prints claros

CC=gcc
CFLAGS=-Wall -O2 -D_POSIX_C_SOURCE=200809L
TARGET=practica1
CSV=muse_v3.csv

all: $(TARGET)

$(TARGET): practica1.c
	$(CC) $(CFLAGS) -o $(TARGET) practica1.c

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
