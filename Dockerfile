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

# Usar wget directo desde Hugging Face
RUN wget -O ./Data/muse1gb.csv https://huggingface.co/datasets/pauguzman/muse-csv/resolve/main/muse1gb.csv

# Dar permisos de ejecución al start script
RUN chmod +x start.sh

# Comando de inicio
CMD ["./start.sh"]
