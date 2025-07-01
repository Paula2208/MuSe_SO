# 🎵 MuSe: Emociones en tus Canciones

MuSe es un sistema de búsqueda de canciones basado en emociones, intensidad emocional (arousal) y artista. Está diseñado en C utilizando estructuras eficientes como **tablas hash** y comunicación entre procesos mediante **pipes con nombre**. El sistema permite indexar y consultar un dataset extenso de canciones (\~4GB), ofreciendo resultados personalizados y filtrados por criterios afectivos.

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

````

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

2. **Searcher** (`searcher`):
   - Espera peticiones de búsqueda desde la `interface`.
   - Carga el archivo binario correspondiente a la emoción buscada.
   - Recupera las canciones filtrando por arousal y artista.
   - Devuelve resultados a través de `PIPE_RES`.

3. **Interface** (`interface`):
   - Menú interactivo para el usuario.
   - Permite ingresar: emoción, arousal y artista.
   - Envía la solicitud al `searcher` mediante `PIPE_REQ`.
   - Muestra los resultados si el usuario lo desea.

---

## 🚀 Ejecución del Proyecto

### 1. Compilación

Usa el script:

```bash
./run.sh
````

O compila manualmente con:

```bash
make
```

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

**Searcher (en una terminal):**

```bash
./output/searcher searcher data/muse_dataset.csv
```

**Interface (en otra terminal):**

```bash
./output/interface interface
```

> El `searcher` cargará automáticamente el archivo binario correcto según la emoción buscada.

---

## 📦 Estructura de Archivos

```
MuSe_SO/
├── data/
│   └── muse_dataset.csv            # Dataset original
├── output/
│   ├── emotions
|   |     └── index_<emoción>.bin   # Índices binarios por emoción
│   ├── search_req.pipe             # Named pipe para requests
│   ├── search_res.pipe             # Named pipe para responses
│   ├── searcher.ready              # Bandera para avisar al interface que el searcher está disponible
│   ├── searcher                    # Ejecutable del indexador y buscador
│   └── interface                   # Ejecutable de la interfaz de usuario
├── p1-dataProgram.c                # Código fuente principal
├── indexador.h / indexador.c       # Módulo de estructuras e indexación
├── Makefile                        # Makefile para compilación y ejecición de procesos
├── README.md                       # Este archivo
└── run.sh                          # Script de ejecución
```

---

## 📁 Organización y Makefile

Asegúrate de:

1. Que tu CSV esté en la carpeta `data/`.

2. Que el nombre del CSV en el `Makefile` coincida con el tuyo:

```make
CSV=./data/muse_dataset.csv
```
3. Puedes usar los comandos del Makefile por separado

```bash
make
make run-searcher    # Ejecuta solo el searcher
make run-interface   # Ejecuta solo la interfaz
make run-both        # Ejecuta ambos en terminales separadas (sin indexación)
make run-all         # Ejecuta indexador, searcher e interfaz en terminales separadas
```
4. Puedes usar los comandos automáticos:

```bash
./run.sh              # Ejecuta searcher e interfaz en terminales separadas (sin indexación)
./indexing.sh         # Ejecuta indexador, searcher e interfaz en terminales separadas
./info.sh             # Ejecuta la información del proceso en memoria
```

---

## 🧩 Detalles Técnicos

* **Claves de hash**: `<artist>`, con sanitización.
* **Indexación por emoción**: cada emoción tiene su propio archivo.
* **División por intensidad**: se crea un array de 101 posibles arousals por emoción.
* **Persistencia**: los índices binarios evitan reindexar cada vez.
* **Búsqueda eficiente**: solo se accede al arousal y artista solicitados.
* **Múltiples entradas**: si una canción tiene varias emociones, se indexa múltiples veces.

---

## 📌 Requisitos

* Linux/Unix (uso de `mkfifo`, señales, etc.).
* Compilador C (GCC).
* Dataset `.csv` ubicado en `data/`.

---

## 👩‍💻 Autores

* Paula Daniela Guzmán Zabala
* Natalia Lesmes
* Juan Manuel Cristancho

Desarrollado como parte de la práctica 1 de Sistemas Operativos.
2025-1s | Universidad Nacional de Colombia

🎧 ¡Gracias por usar MuSe!