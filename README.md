# 🎵 MuSe: Emociones en tus Canciones

MuSe es un sistema de búsqueda de canciones basado en emociones, intensidad emocional (arousal) y artista. Está diseñado en C utilizando estructuras eficientes como **tablas hash** y comunicación entre procesos (máquinas) mediante **sockets**. El sistema permite indexar y consultar un dataset extenso de canciones (\~2GB a \~4GB), ofreciendo resultados personalizados y filtrados por criterios afectivos.

---

## 📡 Nueva Arquitectura: Cliente–Servidor con Sockets

El sistema se ha actualizado para usar **Sockets TCP/IP** entre los procesos:

- El **servidor** (`server`) escucha en un puerto fijo (`3550`) y responde a búsquedas desde clientes.
- El **cliente** (`client`) se conecta por red y permite búsquedas interactivas desde una terminal.
- Esto permite ejecución distribuida: puedes ejecutar el cliente en tu PC y el servidor en la nube.

---

## 📂 Dataset Base

El sistema trabaja con el dataset:
[**MuSe: The Musical Sentiment Dataset**](https://www.kaggle.com/datasets/cakiki/muse-the-musical-sentiment-dataset)

Este dataset contiene información de canciones como: URL, título, artista, emociones asociadas, valencia, intensidad, dominancia y género.

- **valencia (valence)**: "la gratificación de un estímulo"
- **intensidad (arousal)**: "la intensidad de la emoción provocada por un estímulo"
- **dominancia (dominance)**: "el grado de control ejercido por un estímulo"

---

## 🧠 Criterios de Búsqueda: ¿Por qué intensidad → emoción → artista?

El sistema utiliza una clave de búsqueda compuesta en el siguiente orden:

emotion → arousal → artist

Esto permite una búsqueda rápida y eficaz a partir del nivel de intensidad emocional (arousal), filtrando por emoción y artista para obtener resultados personalizados y emocionalmente relevantes.

---

## 🧪 Ejemplo de Uso: Búsqueda Interactiva

```bash

🌟 Menú Principal:

1. Ingresar emoción ❤️
2. Ingresar la intensidad de la emoción (0 a 100) 🎚️
3. Ingresar el artista 🎤
4. Realizar la búsqueda
9. Salir

Seleccione una opción: 1
💬 Ingrese una emoción para buscar ❤️: aggressive

Seleccione una opción: 2
🎚️ Ingrese la intensidad de la emoción (0 a 100) 🎚️: 80

Seleccione una opción: 3
🎤 Ingrese el nombre del artista 🎤: eminem

Seleccione una opción: 4

🎵 Se encontraron 2 canciones. ¿Desea mostrarlas? (s/n): s

🎶 === Canción encontrada ===
🎵 Track: Till I Collapse
🎤 Artista: Eminem
💼 Género: Rap
💬 Emociones: aggressive
🎚️ Valence: 4.55 | Arousal: 5.27 | Dominance: 5.69
🔗 URL: [https://www.last.fm/music/eminem/\_/till+I+collapse](https://www.last.fm/music/eminem/_/till+I+collapse)

🎶 === Canción encontrada ===
🎵 Track: The Sauce
🎤 Artista: Eminem
💼 Género: Rap
💬 Emociones: aggressive
🎚️ Valence: 3.00 | Arousal: 5.84 | Dominance: 4.71
🔗 URL: [https://www.last.fm/music/eminem/\_/the+sauce](https://www.last.fm/music/eminem/_/the+sauce)

```

---

## ❤️ ¿Cómo funciona MuSe?

MuSe está compuesto por **tres componentes** principales:

1. **Indexer** (`indexer`):
   - Procesa el archivo CSV y genera un archivo binario por emoción:  
     ```
     ./output/index_<emoción>.bin
     ```
   - Cada archivo contiene un arreglo de 101 niveles de arousal (0 a 100).
   - En cada arousal hay una tabla hash de artistas y sus posiciones en el CSV.
   - Cada canción se indexa múltiples veces, una por cada emoción que contiene.

2. **Server** (`searcher`):
   - Escucha peticiones de bśuqueda de clientes `interface` vía socket TCP (puerto 3550).
   - Carga el archivo binario correspondiente a la emoción buscada.
   - Recupera las canciones filtrando por arousal y artista.
   - Devuelve resultados a través de `sockets`.
   - (Antiguo searcher en p1-dataProgram.c)

3. **Client** (`interface`):
   - Menú interactivo para el usuario.
   - Permite ingresar: emoción, arousal y artista.
   - Envía la solicitud al `searcher` en cloud mediante `sockets`.
   - Muestra los resultados si el usuario lo desea.
   - (Antiguo interface en p1-dataProgram.c)

---

### 2. Indexación del Dataset

Antes de usar el sistema, debes indexar el archivo CSV:

```bash
./output/searcher indexer data/muse_dataset.csv
```

Esto creará múltiples archivos binarios en `./output/`:

```
index_aggressive.bin
index_happy.bin
...
```

### 3. Ejecución de los procesos

**Searcher:**

Este puede ser accedido mediante el pierto de la instancia de la máquina virtual en Google Cloud. Ya está activo, no se necesita hacer más.

```bash
# Asegurarse de que exista misocket-server

gcloud compute instances list
```

```bash
# Para crear la instancia o actualizarla

./deploy.sh
```

**Interface (cliente en terminal):**

```bash
./client.sh
```
---

## 📦 Estructura de Archivos

```
MuSe_SO/
├── data/
│   └── muse1gb.csv                 # Dataset original
├── helpers/
│   └── indexador.h / indexador.c   # Módulo de estructuras e indexación
├── output/
│   ├── emotions
|   |     └── index_<emoción>.bin   # Índices binarios por emoción
│   ├── server                      # Ejecutable del buscador (servidor)
│   └── client                      # Ejecutable de la interfaz de usuario (cliente)
├── server.c                        # Código fuente principal del servidor
├── client.c                        # Código fuente principal del cliente
├── Makefile                        # Makefile para compilación y ejecución de procesos
├── Dockerfile                      # Dockerización de la máquina virtual para el server
├── README.md                       # Este archivo
├── deploy.sh                       # Script para subir la máquina virtual del servidor a gcloud
├── client.sh                       # Script de ejecución del cliente (local)
└── start.sh                        # Script de ejecución para el Dockerfile (server)
```

---

## 🧩 Detalles Técnicos

* **Claves de hash**: `<artist>`, con sanitización.
* **Indexación por emoción**: cada emoción tiene su propio archivo.
* **División por intensidad**: se crea un array de 101 posibles arousals por emoción.
* **Persistencia**: los índices binarios evitan reindexar cada vez.
* **Búsqueda eficiente**: solo se accede al arousal y artista solicitados.
* **Múltiples entradas**: si una canción tiene varias emociones, se indexa múltiples veces.
* **Conexión por socket en la nube**: El servidor se encuentra en constante espera de clientes ya que está desplegado en una máquina virtual de Google Cloud.
---

## 📌 Requisitos

* Linux/Unix (uso de sockets y hilos)
* GCC (compilador C)
* wget, make, etc.

---

## 👩‍💻 Autores

* Paula Daniela Guzmán Zabala
* Natalia Lesmes
* Juan Manuel Cristancho

Desarrollado como parte de la práctica 2 de Sistemas Operativos.
2025-1s | Universidad Nacional de Colombia

🎧 ¡Gracias por usar MuSe!