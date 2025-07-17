# Makefile para Ubuntu con doble terminal y directorio output

CC=gcc
CFLAGS=-Wall -O2 -D_POSIX_C_SOURCE=200809L
OUTDIR=output
TARGET=$(OUTDIR)/p1-dataProgram
CSV=./Data/muse1gb.csv

# Archivos fuente
SRC_MAIN=p1-dataProgram.c
SRC_HELPERS=helpers/indexador.c

# Archivos temporales
PIPES=$(OUTDIR)/search_req.pipe $(OUTDIR)/search_res.pipe
READY=$(OUTDIR)/searcher.ready

all: $(TARGET)

$(TARGET): $(SRC_MAIN) $(SRC_HELPERS) | $(OUTDIR)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC_MAIN) $(SRC_HELPERS)

$(OUTDIR):
	mkdir -p $(OUTDIR)

run-indexer: all
	@echo -e "\n\n📦 Ejecutando el indexador en nueva terminal..."
	gnome-terminal -- bash -c "./$(TARGET) indexer $(CSV); exec bash"

run-searcher: all
	@echo -e "\n\n🧠 Ejecutando el proceso de búsqueda en nueva terminal..."
	gnome-terminal -- bash -c "./$(TARGET) searcher $(CSV); exec bash"

run-interface: all
	@echo -e "\n\n🎧 Ejecutando la interfaz interactiva en nueva terminal..."
	gnome-terminal -- bash -c "./$(TARGET) interface $(CSV); exec bash"

clean-all:
	@echo -e "\n\n🧹 Limpiando archivos indexados, compilados y temporales..."
	rm -rf $(OUTDIR) *.o

clean:
	@echo -e "\n\n🧹 Limpiando archivos compilados y temporales..."
	rm -f $(TARGET) *.o $(PIPES) $(READY)

run-both: all
	$(MAKE) clean
	@echo "🚀 Ejecutando ambos procesos..."
	$(MAKE) run-searcher
	sleep 1
	$(MAKE) run-interface

run-all: all
	$(MAKE) clean-all
	@echo "🔁 Ejecutando indexador, searcher e interface..."
	$(MAKE) run-indexer
	sleep 4
	$(MAKE) run-searcher
	sleep 1
	$(MAKE) run-interface
