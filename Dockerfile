FROM ubuntu:22.04

# Variables de entorno para evitar prompts de configuración
ENV DEBIAN_FRONTEND=noninteractive

# Instalar dependencias
RUN apt-get update && apt-get install -y \
    gcc \
    make \
    wget \
    unzip \
    gnome-terminal \
    && apt-get clean

# Crear carpetas necesarias
RUN mkdir -p /app/Data

# Establecer el directorio de trabajo
WORKDIR /app

# Copiar todos los archivos del proyecto
COPY . .

# Descargar CSV desde Google Drive (reemplazr con el ID del archivo - se obtiene de la URL)
# Usamos la herramienta gdown para manejar Google Drive fácilmente
RUN wget -q https://raw.githubusercontent.com/wkentaro/gdown/master/gdown.py \
    && python3 gdown.py --id 1axbzTnzRk-wazs2KoIGjijAn0QSfuLnm -O ./Data/muse1gb.csv \
    && rm gdown.py

# Dar permisos de ejecución al start script
RUN chmod +x start.sh

# Comando de inicio
CMD ["./start.sh"]
