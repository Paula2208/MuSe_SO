#!/bin/bash

# Obtener PIDs reales del programa
PIDS=$(ps -ef | grep "./output/p1-dataProgram " | grep -E "searcher|interface" | grep -v "bash -c" | grep -v grep | awk '{print $2}')

if [ -z "$PIDS" ]; then
    echo "❌ No se encontraron procesos activos de p1-dataProgram."
    exit 1
fi

echo "🔎 PID(s) encontrados: $PIDS"

# Mostrar info detallada con nombres
echo -e "\n📊 Información de procesos MuSe_SO:"

for PID in $PIDS; do
    CMD=$(ps -p $PID -o cmd=)
    if echo "$CMD" | grep -q "searcher"; then
        echo -e "\n🔍 Proceso SEARCHER (PID: $PID)"
    elif echo "$CMD" | grep -q "interface"; then
        echo -e "\n🎧 Proceso INTERFACE (PID: $PID)"
    else
        echo -e "\n⚙️  Proceso desconocido (PID: $PID)"
    fi
    ps -p $PID -o pid,ppid,%cpu,%mem,etime,cmd
done

# Solo top del searcher
SEARCHER_PID=$(ps -ef | grep "./output/p1-dataProgram" | grep searcher | grep -v "bash -c" | grep -v grep | awk '{print $2}')

if [ -n "$SEARCHER_PID" ]; then
    echo -e "\n🟢 Abriendo monitor en tiempo real de SEARCHER (PID: $SEARCHER_PID)..."
    top -p $SEARCHER_PID
else
    echo "❌ No se encontró el proceso searcher activo."
fi
