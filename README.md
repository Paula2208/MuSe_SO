# 🎵 MuSe: Emociones en tus Canciones

MuSe es un sistema de búsqueda de canciones basado en emociones, intensidad emocional (arousal) y artista. Está diseñado en C utilizando estructuras eficientes como **tablas hash** y comunicación entre procesos mediante **pipes con nombre**. El sistema permite indexar y consultar un dataset extenso de canciones (\~4GB), ofreciendo resultados personalizados y filtrados por criterios afectivos.

---

## 📂 Dataset Base

El sistema trabaja con el dataset:
[**MuSe: The Musical Sentiment Dataset**](https://www.kaggle.com/datasets/cakiki/muse-the-musical-sentiment-dataset)
Este dataset contiene información de canciones como: URL, título, artista, emociones asociadas, valencia, intensidad, dominancia y género.

* valencia (valence): "la gratificación de un estímulo"
* intensidad (arousal): "la intensidad de la emoción provocada por un estímulo"
* Dominio (Dominance): "el grado de control ejercido por un estímulo"


---

## 🧠 Criterios de Búsqueda: ¿Por qué intensidad → emoción → artista?

El sistema está diseñado para buscar canciones utilizando una clave compuesta en el orden:

```
<arousal>_<emotion>_<artist>
```

Este esquema de indexación no es arbitrario. Está basado en consideraciones prácticas y conceptuales sobre cómo se estructura el dataset y cómo los usuarios suelen buscar música emocionalmente relevante:

1. **🎭 Emoción como base del sistema**
   Las emociones son el eje central del dataset MuSe, y constituyen el aspecto más interesante para el usuario. Son el punto de partida natural para organizar las canciones y realizar búsquedas afectivas.

2. **📈 Arousal como filtro de relevancia**
   La intensidad emocional (arousal) permite determinar qué tan presente o dominante es una emoción en una canción. Incluirla primero en la clave permite filtrar rápidamente canciones donde esa emoción sea suficientemente significativa para ser relevante.

3. **🎤 Artista como anclaje emocional**
   Muchas personas tienen una conexión emocional más fuerte con ciertos artistas. Incluir el nombre del artista en la clave ayuda a personalizar la búsqueda, permitiendo encontrar canciones de un artista específico que también evocan una emoción particular con suficiente intensidad.

Este orden de criterios permite una indexación eficiente y búsquedas más precisas, alineadas con cómo las personas exploran música emocionalmente significativa.

---

## 🧪 Ejemplo de Uso: Búsqueda Interactiva

A continuación se muestra un ejemplo de cómo funciona la búsqueda de canciones desde la consola una vez que el sistema está en ejecución:

```
🌟 Menú Principal:
1. Ingresar la intensidad de la emoción (0 a 100)
2. Ingresar emoción
3. Ingresar el artista
4. Realizar la búsqueda
9. Salir
Seleccione una opción: 1

🎚️ Ingrese la intensidad de la emoción (0 a 100) 🎚️: 80

Seleccione una opción: 2

💬 Ingrese una emoción para buscar ❤️: aggressive

Seleccione una opción: 3

🎤 Ingrese el nombre del artista 🎤: eminem

Seleccione una opción: 4

🔍 Buscando: '80_aggressive_eminem'...

🎵 Se encontraron 3 canciones. ¿Desea mostrarlas? (s/n): s

🎶 Resultados encontrados:
-------------------------------------
🎵 Título: Lose Yourself
👤 Artista: Eminem
🔗 URL: https://www.last.fm/music/Eminem/_/Lose+Yourself
❤️ Emociones: 'aggressive', 'focused'
🎧 Género: Rap

...

✅ Total de canciones encontradas: 3

🔁 Volviendo al menú principal...
```

Este flujo permite al usuario refinar su búsqueda paso a paso, asegurando una interacción sencilla y significativa.

---

## ❤️ ¿Cómo funciona MuSe?

MuSe está compuesto por **dos procesos independientes**:

1. **Searcher** (`searcher`):

   * Indexa el archivo CSV usando una **tabla hash** con claves del tipo:

     ```
     <arousal_entero>_<emoción>_<artista>
     ```
   * Responde a las búsquedas enviadas por el proceso interfaz y retorna resultados.
   * Guarda el índice en un archivo binario para evitar reindexación futura.

2. **Interface** (`interface`):

   * Interactúa con el usuario mediante un menú.
   * Permite ingresar los criterios de búsqueda.
   * Se comunica con el `searcher` por medio de **FIFOs** (`PIPE_REQ` y `PIPE_RES`).
   * Muestra los resultados encontrados si el usuario lo desea.

---

## 🚀 Ejecución del Proyecto

### 1. Compilación

Usando el bash creado:

```bash
./run.sh
```

O puedes usar directamente los comandos del Makefile:

```bash
make
make run-both
```

> Si se necesita reindexar el archivo, basta con borrar el binario que se genera en la carpeta /output y correr nuevamente el proyecto, este indexará automáticamente. Puedes utilizar el siguiente comando para hacerlo.

```bash
cd output/ && rm -r index.bin && cd ..
```

### 2. Ejecución

#### Paso 1: Ejecutar el `searcher`

Este proceso debe iniciarse primero.

```bash
./output/searcher searcher data/muse_dataset.csv
```

* Si existe un índice guardado en `output/index.bin`, lo cargará directamente.
* Si no, indexará el archivo CSV y guardará el índice automáticamente en segundo plano.

#### Paso 2: Ejecutar la `interface` (en otro terminal)

```bash
./output/interface interface
```

La interfaz te permitirá:

* Ingresar la intensidad emocional (arousal) \[0–100]
* Ingresar una emoción (ej. `happy`, `sad`)
* Ingresar el artista (ej. `taylor swift`)
* Realizar una búsqueda con los criterios anteriores

---

## 📦 Estructura de Archivos

```
MuSe/
├── data/
│   └── muse_dataset.csv            # Dataset original
├── output/
│   ├── index.bin                   # Archivo binario generado con el índice hash
│   ├── searcher                    # Ejecutable del indexador
│   └── interface                   # Ejecutable de la interfaz de usuario
├── main.c                          # Código fuente principal
├── README.md                       # Este archivo
└── run.sh                          # Script de ejecución opcional
```

---

## 📁 Organización de Archivos y Configuración del Makefile

Para que el sistema funcione correctamente, debes asegurarte de lo siguiente:

1. **Ubicación del CSV**:
   El archivo CSV del dataset debe estar dentro del directorio `Data/`, por ejemplo:

   ```
   Data/muse1gb.csv
   ```

2. **Nombre del archivo CSV en el Makefile**:
   El nombre del CSV utilizado por defecto se define en el `Makefile` mediante la variable:

   ```make
   CSV=./Data/muse1gb.csv
   ```

   Si usas otro archivo (por ejemplo `muse_dataset.csv`), actualiza la línea correspondiente en el Makefile:

   ```make
   CSV=./Data/muse_dataset.csv
   ```

3. **Ejecución con doble terminal**:
   El Makefile tiene objetivos que abren automáticamente dos terminales con los procesos necesarios:

   * `make run-searcher` — lanza el proceso de indexado (`searcher`)
   * `make run-interface` — lanza la interfaz (`interface`)
   * `make run-both` — limpia y ejecuta ambos en paralelo

   Asegúrate de tener GNOME Terminal instalado, o ajusta `gnome-terminal` en el Makefile si usas otro emulador (como `xfce4-terminal`, `konsole`, etc.).

> NOTA: No subimos el dataset ampliado al github porque es muy pesado, igual se puede usar con el dataset original de kaggle.

---

## 🧩 Detalles Técnicos

* **Hashing**: Las claves se generan combinando `arousal`, `emoción`, y `artista` después de aplicar una sanitización alfanumérica.
* **Indexación**: Cada canción puede indexarse múltiples veces si tiene múltiples emociones.
* **Persistencia**: El índice se guarda en `index.bin` para evitar reindexación en futuras ejecuciones.
* **Comunicación entre procesos**: Uso de `mkfifo`, `open`, `read`, `write` y estructuras tipo `Song` para enviar resultados.
* **Optimización**: Las búsquedas se hacen solo sobre los nodos relevantes gracias a la clave hash, evitando escanear todo el dataset.

---

## 📌 Requisitos

* Sistema operativo Linux/Unix (por uso de `mkfifo`, `fork`, señales).
* Compilador C (GCC recomendado).
* Archivo CSV del dataset ubicado en `data/`.

---

## 👩‍💻 Autores

* Paula Daniela Guzmán Zabala
* Natalia Lesmes
* Juan Manuel Cristancho

Desarrollado como parte de la práctica 1 de Sistemas Operativos.

2025-1s | Universidad Nacional de Colombia.

¡Gracias por usar MuSe! 🎧✨
Donde la música y tus emociones se encuentran.
