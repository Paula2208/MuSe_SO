# Makefile para la Práctica 2 de Sistemas Operativos (Sockets)
# Adaptado para Ubuntu con gnome-terminal

# --- Configuración del Compilador y Archivos ---
CC=gcc
# Se añade -lpthread para el indexador que usa hilos
CFLAGS=-Wall -O2 -D_POSIX_C_SOURCE=200809L
LDFLAGS=-lpthread

# --- Directorios y Archivos de Entrada/Salida ---
OUTDIR=output
CSV_FILE=./Data/muse1gb.csv # Asegúrate de que esta ruta sea correcta

# --- Archivos Fuente ---
# Asumimos que indexador.c contiene la lógica de indexación
# y que server.c/client.c tienen su propia lógica.
SRC_INDEXER=helpers/indexador.c
SRC_SERVER=server.c
SRC_CLIENT=client.c

# --- Nombres de los Ejecutables ---
TARGET_INDEXER=$(OUTDIR)/indexer
TARGET_SERVER=$(OUTDIR)/server
TARGET_CLIENT=$(OUTDIR)/client

# --- Regla Principal ---
# 'all' compila todos los programas necesarios.
all: $(TARGET_SERVER) $(TARGET_CLIENT)

# --- Reglas de Compilación ---

# Crea el directorio de salida si no existe
$(OUTDIR):
	mkdir -p $(OUTDIR)
	mkdir -p $(OUTDIR)/emotions

# Compila el servidor
$(TARGET_SERVER): $(SRC_SERVER) $(SRC_INDEXER) | $(OUTDIR)
	$(CC) $(CFLAGS) -o $(TARGET_SERVER) $(SRC_SERVER) $(SRC_INDEXER)

# Compila el cliente
$(TARGET_CLIENT): $(SRC_CLIENT) $(SRC_INDEXER) | $(OUTDIR)
	$(CC) $(CFLAGS) -o $(TARGET_CLIENT) $(SRC_CLIENT) $(SRC_INDEXER)


# --- Reglas de Ejecución ---

# Ejecuta el indexador para crear los archivos .bin
# Esto solo necesita hacerse una vez, o cuando el CSV cambie.
run-indexer: $(TARGET_INDEXER)
	@echo "📦 Ejecutando el indexador en una nueva terminal..."
	@echo "Este proceso puede tardar. La terminal se cerrará al finalizar."
	gnome-terminal -- bash -c "./$(TARGET_INDEXER) $(CSV_FILE); echo '✅ Indexación completada.'; read -p 'Presiona Enter para cerrar...' "

# Ejecuta el servidor en una nueva terminal
run-server: $(TARGET_SERVER)
	@echo "🧠 Ejecutando el Servidor (Searcher) en una nueva terminal..."
	gnome-terminal -- bash -c "./$(TARGET_SERVER) $(CSV_FILE); exec bash"

# Ejecuta el cliente en una nueva terminal
run-client: $(TARGET_CLIENT)
	@echo "🎧 Ejecutando el Cliente (Interface) en una nueva terminal..."
	gnome-terminal -- bash -c "./$(TARGET_CLIENT); exec bash"

# --- Reglas de Limpieza ---

# Limpia todos los archivos generados (compilados e índices)
clean-all:
	@echo "🧹 Limpiando todo: compilados, ejecutables y directorio de salida..."
	rm -rf $(OUTDIR)

# Limpia solo los ejecutables
clean:
	@echo "🧹 Limpiando solo los archivos ejecutables..."
	rm -f $(TARGET_INDEXER) $(TARGET_SERVER) $(TARGET_CLIENT)

# --- Reglas Combinadas ---

# Compila y ejecuta el servidor y el cliente
run-both: all
	@echo "🚀 Ejecutando Servidor y Cliente..."
	@$(MAKE) run-server
	@echo "⏳ Esperando 2 segundos para que el servidor inicie..."
	@sleep 2
	@$(MAKE) run-client

# El flujo completo: limpiar todo, indexar, y luego correr el sistema
run-full-system:
	@$(MAKE) clean-all
	@echo "🔁 Ejecutando el flujo completo: Indexador -> Servidor -> Cliente"
	@$(MAKE) run-indexer
	@echo "\n‼️ IMPORTANTE: Espera a que la terminal del indexador confirme que ha terminado."
	@read -p "Una vez finalizada la indexación, presiona Enter para lanzar el servidor y el cliente..."
	@$(MAKE) run
