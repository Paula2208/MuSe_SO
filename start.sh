#!/bin/bash

set -e

make

# Descargar el CSV solo si no existe
if [ ! -f "/app/Data/muse1gb.csv" ]; then
    echo "📥 Descargando muse1gb.csv..."
    mkdir -p /app/Data
    wget -O /app/Data/muse1gb.csv https://huggingface.co/datasets/pauguzman/muse-csv/resolve/main/muse1gb.csv
fi

echo "Ejecutando servidor..."
./output/server /app/Data/muse1gb.csv
