# 🔐 Criptoanálisis Estadístico Distribuido

![Python](https://img.shields.io/badge/Python-3776AB?style=for-the-badge&logo=python&logoColor=white)
![C](https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c&logoColor=white)
![Sockets](https://img.shields.io/badge/Sockets-TCP%2FIP-black?style=for-the-badge)
![Teoría de la Información](https://img.shields.io/badge/Teor%C3%ADa-Informaci%C3%B3n-4CAF50?style=for-the-badge)

Sistema distribuido de alto rendimiento diseñado para romper cifrados de sustitución monoalfabética de forma automatizada. Apoyándose en la teoría de la información, el sistema explota la redundancia estadística del idioma español para recuperar el texto claro sin conocer la clave original de cifrado.

La arquitectura divide estratégicamente la carga de trabajo: un **Orquestador en Python** centraliza la lógica de control y la heurística de búsqueda, mientras múltiples **Nodos Esclavos en C** actúan como motores de cálculo puro, evaluando cientos de miles de claves por segundo.

---

## ⚙️ Arquitectura del Sistema

El proyecto utiliza un modelo de procesamiento por lotes (*chunking*) para maximizar el uso de CPU y minimizar la latencia de red.

### 🐍 Orquestador / Master (Python)
*   **Normalización del Corpus:** Procesa textos de entrenamiento eliminando acentos, puntuación y unificando a mayúsculas.
*   **Generador del Modelo:** Extrae la frecuencia de los trigramas y calcula sus probabilidades logarítmicas.
*   **Búsqueda Heurística (Hill Climbing):** Inicia con claves aleatorias y genera "claves vecinas" intercambiando dos posiciones de la permutación. Implementa reinicios (*restarts*) para escapar de máximos locales[cite: 4].
*   **Despacho Distribuido:** Administra un servidor de sockets TCP que envía lotes masivos de claves a los nodos de evaluación.

### ⚡ Nodos de Evaluación / Esclavos (C)
*   **Precarga en RAM:** Cargan el texto cifrado normalizado y las 17,576 probabilidades de trigramas en memoria antes de evaluar, eliminando operaciones de I/O.
*   **Descifrado Rápido $O(1)$:** Emplean Tablas de Búsqueda (LUT) basadas en ASCII para traducir el texto cifrado instantáneamente.
*   **Evaluación Matemática:** Procesan lotes enviados por el Master y devuelven únicamente la clave con la mejor puntuación del lote.

---

## 🧮 Fundamento Matemático

El criptoanálisis no busca palabras en un diccionario, sino que cuantifica la compatibilidad del texto generado con la estructura estadística del idioma.

1.  **Redundancia y Trigramas:** El lenguaje natural posee redundancia; el contexto de las letras anteriores reduce la incertidumbre de las siguientes[cite: 4]. Esto se modela utilizando ventanas deslizantes de tres letras (trigramas)[cite: 4].
2.  **Función de Puntuación (Score):** Para evitar el desbordamiento negativo (*underflow*) al multiplicar probabilidades muy pequeñas, se utiliza la suma de las cantidades de información (logaritmos base 2)[cite: 4]. El score de un texto tentativo $T$ se define como:
    $$S(T) = \sum_{i=1}^{N-2} \log_2 P(t_i t_{i+1} t_{i+2})$$
3.  **Suavizado (Trigramas Inexistentes):** Si el texto evaluado genera un trigrama que no existe en el corpus de entrenamiento, se evita el error matemático $\log_2(0)$ asignando una probabilidad mínima $P_{min} = \frac{1}{10B}$, donde $B$ es el número total de trigramas del corpus[cite: 4].

---

## 🚀 Instalación y Uso

### Prerrequisitos
*   Python 3.x
*   Compilador GCC o Clang (para C)
*   Entorno Linux/Unix (recomendado para manejo de Sockets POSIX)

### 1. Compilar el Nodo Esclavo (C)
Debido al uso de funciones logarítmicas, es necesario enlazar la librería matemática matemática estándar (`-lm`).
```bash
gcc esclavo.c -o esclavo -lm -O3
