# Makefile para Ubuntu con doble terminal y directorio output

CC=gcc
CFLAGS=-Wall -O2 -D_POSIX_C_SOURCE=200809L
OUTDIR=output
TARGET=$(OUTDIR)/p1-dataProgram
CSV=./Data/muse1gb.csv

# Archivos temporales
PIPES=$(OUTDIR)/search_req.pipe $(OUTDIR)/search_res.pipe
READY=$(OUTDIR)/searcher.ready

all: $(TARGET)

$(TARGET): p1-dataProgram.c | $(OUTDIR)
	$(CC) $(CFLAGS) -o $(TARGET) p1-dataProgram.c

$(OUTDIR):
	mkdir -p $(OUTDIR)

run-searcher: all
	@echo -e "\n\n🧠 Ejecutando el proceso de búsqueda en nueva terminal..."
	gnome-terminal -- bash -c "./$(TARGET) searcher $(CSV); exec bash"

run-interface: all
	@echo -e "\n\n🎧 Ejecutando la interfaz interactiva en nueva terminal..."
	gnome-terminal -- bash -c "./$(TARGET) interface $(CSV); exec bash"

clean:
	@echo -e "\n\n🧹 Limpiando archivos compilados y temporales..."
	rm -f $(TARGET) *.o $(PIPES) $(READY)

run-both: all
	$(MAKE) clean
	@echo "🚀 Ejecutando ambos procesos..."
	$(MAKE) run-searcher
	sleep 1
	$(MAKE) run-interface
