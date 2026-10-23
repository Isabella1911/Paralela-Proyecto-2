# Conocimiento base — Proyecto 02 (Computación Paralela y Distribuida)

Documento de referencia que reúne (1) lo que pide el Proyecto 02, (2) lo que ya
existe en el proyecto de Teoría de la Computación que usaremos como base
conceptual, y (3) cómo se conectan ambos para nuestra propuesta de una Máquina de
Turing de 4 cintas que se traduce a C + MPI.

> Fuentes:
> - Enunciado: `CC3069 - Proyecto 02.pdf` (CC3069 Computación Paralela y Distribuida, Ciclo 2 2026).
> - Repo de Teoría: https://github.com/Isabella1911/Proyecto-3-Teoria-de-la-computacion
>   (clonado como referencia en `../repo-teoria`).

---

## 1. Qué pide el Proyecto 02

**Objetivo.** Optimizar un programa paralelo de búsqueda de claves **AES-128**
mediante **Open MPI**, implementando y evaluando estrategias de distribución del
trabajo para mejorar tiempos de ejecución y *Speedup*. Punto de partida: el
programa del **Laboratorio 03**.

### Entregables
- Informe PDF (formato de informes UVG: carátula, índice, introducción, cuerpo,
  conclusiones, apéndices y ≥3 referencias).
- Todo el código fuente funcional.
- Capturas de compilación y ejecución (comandos + resultados).

### Actividades
1. **Cifrar/descifrar un archivo `.txt` con AES-128**, usando una clave de 16
   bytes (128 bits) dada como parámetro. Evidenciar texto original, cifrado y
   recuperado.
2. **Búsqueda de clave por fuerza bruta con 4 procesos MPI (`-np 4`):**
   - Texto a cifrar: `"Esta es una prueba de proyecto 2"`.
   - El programa no conoce la clave; prueba claves por fuerza bruta.
   - Para validar, al descifrar revisa si el resultado contiene la **frase de
     búsqueda**: `"es una prueba de"`.
   - Mediciones de tiempo:
     - a) clave `123456`.
     - b) clave `2^56 / 4 = 18014398509481984`.
     - c) clave `2^56 / 4 + 1 = 18014398509481985`.
     - d) comparar los tres tiempos y explicar por qué varían según la clave.
     - e) capturas de cada prueba.
3. **Estrategia alternativa de distribución/búsqueda** de claves que mejore el
   rendimiento frente al reparto contiguo simple. Comparar tiempos de ambas
   estrategias con las mismas claves del inciso 2; analizar ventajas y
   limitaciones.
4. **Explicar la estrategia alternativa** con diagramas de flujo, pseudocódigo o
   esquemas: cómo se reparten las claves, cómo se busca y cómo se comunican los
   procesos al encontrar la clave.

### Rúbrica (resumen)
- **Informe (36 pts):** antecedentes (AES-128, fuerza bruta, Open MPI) · formato
  UVG · discusión/conclusiones · diagramas de estrategias · Anexo 1 catálogo de
  funciones · Anexo 2 bitácora de pruebas y speedups (sin evidencia no califica).
- **Programa (60 pts):** cifrar/descifrar/buscar correctos con programación
  defensiva, lectura de archivos, frase de búsqueda, pruebas con distintas
  claves, bitácora de tiempos, **versión secuencial**, **versión paralela**,
  **estrategia base** y **estrategia alternativa** funcionales, manejo de memoria
  y uso correcto de Open MPI · documentación y comentarios · orden y claridad.

### Notas técnicas de la búsqueda
- Espacio de claves efectivo: `[0, 2^56)` (se exploran 56 bits; AES-128 usa 16
  bytes pero la prueba acota el contador a 2^56).
- El contador entero se convierte a una clave de **16 bytes**; se descifra el
  bloque y se compara contra la frase de búsqueda.
- El tiempo depende de **la posición de la clave dentro del segmento asignado al
  proceso**, no de su valor absoluto (de ahí que `123456` y `2^56/4` tarden muy
  distinto).

---

## 2. Qué hay en el proyecto de Teoría de la Computación

Repo: *Proyecto 3 – Simulador de Máquina de Turing para Cifrado César*. Nos
interesa como **base conceptual del modelo de MT**, no por el cifrado César en sí.

### Lo reutilizable: simulador de MT multicinta
`maquina/turing.py` ya implementa una **Máquina de Turing determinista de `k`
cintas**, que es exactamente el modelo que necesita nuestra propuesta:

- `TMConfig`: `states, input_alphabet, tape_alphabet, blank, initial_state,
  accept_states, transitions, num_tapes, max_steps`.
- `TuringMachine`: cada cinta es una lista de símbolos con su **propia cabeza**.
  - `reset(input_words)`: una palabra de entrada por cinta (las faltantes inician
    en blanco) — permite inicializar cada cinta con contenido distinto.
  - `_read_all()`: lee el símbolo bajo cada cabeza → tupla de `k` símbolos.
  - `_write_all()` / `_move_all()`: escriben y mueven las `k` cabezas (`L`,`R`,`S`).
  - `step()`: busca `(estado, tupla_de_lecturas)` en `transitions`; aplica
    escritura, cambio de estado y movimiento; se detiene si no hay transición o
    si llega a un estado de aceptación, o al alcanzar `max_steps`.
  - `run(verbose)` y `print_configuration()` para trazar la evolución.

**Función de transición (clave del modelo multicinta):**
```
key = (estado, (s1, s2, ..., sk))
val = (estado_siguiente, (w1, w2, ..., wk), (m1, m2, ..., mk))
```

### Formato JSON de una MT (`maquina/parser.py`)
```json
{
  "Q": ["..."], "Sigma": ["..."], "Gamma": ["..."],
  "blank": "_", "q0": "q0", "F": ["qAccept"],
  "num_tapes": 4, "max_steps": 500000,
  "transitions": [
    ["estado", ["r1","r2","r3","r4"], "estado'", ["w1","w2","w3","w4"], ["m1","m2","m3","m4"]]
  ]
}
```
`load_mt_from_json` valida que `len(reads)==len(writes)==len(moves)==num_tapes` y
que no haya transiciones duplicadas.

### Resto del repo (contexto, menos relevante para nosotros)
- `generate_machines.py`: genera las tablas de transición del César (1 cinta).
- `maquina/encoder_mt.py`, `decoder_mt.py`: capa de ejecución (cargan JSON, hacen
  `reset` + `run`, devuelven la cinta).
- `gui.py` + `run_gui.py`: GUI Tkinter con 4 pestañas y "Ver trazado MT".
- `main_encoder.py`, `main_decoder.py`: CLI.
- Complejidad del simulador: O(n) en tiempo y espacio sobre la longitud de la cinta.

---

## 3. Puente: de la MT de 4 cintas al programa C + MPI

### Propuesta (modelo formal)
Modelamos la búsqueda por fuerza bruta como una **MT de 4 cintas**, donde **cada
cinta ≡ un proceso MPI**. El espacio de claves `[0, 2^56)` se divide en **4
segmentos contiguos de igual tamaño**; cada cinta recorre el suyo de forma
secuencial. Para cada valor del contador:

1. convertir el contador a una **clave de 16 bytes (128 bits)**,
2. descifrar el mensaje con **AES-128**,
3. verificar si el resultado contiene la frase **"es una prueba de"**.

Cuando una cinta encuentra la clave, **notifica a las demás por un mensaje MPI** y
todas detienen su ejecución. El tiempo depende de la posición de la clave dentro
de su segmento, no de su valor absoluto.

### Mapeo MT ↔ MPI
| Concepto en la MT de 4 cintas | Equivalente en C + MPI |
|---|---|
| Cinta `i` (i = 0..3) | Proceso MPI con `rank = i` (`-np 4`) |
| Contenido inicial de la cinta `i` | Inicio del segmento `i`: `rank * (2^56 / 4)` |
| Cabeza avanzando por la cinta | Contador local incrementándose en el segmento |
| Símbolo bajo la cabeza → transición | Clave actual → `AES-128 decrypt` → ¿contiene la frase? |
| Estado de aceptación `qAccept` | Clave encontrada |
| "Notifica a las demás y todas paran" | Mensaje MPI de hallazgo + parada global |

### Segmentación del espacio de claves (contiguo, estrategia base)
```
TOTAL = 2^56
POR_PROCESO = TOTAL / 4 = 2^54 = 18014398509481984
segmento del rank r = [ r * 2^54 , (r+1) * 2^54 )
```
Esto explica las pruebas del inciso 2:
- `123456` cae al inicio del segmento del rank 0 → se encuentra casi de inmediato.
- `2^56/4 = 18014398509481984` es **justo el inicio del segmento del rank 1** →
  lo encuentra rápido ese proceso.
- `2^56/4 + 1` cae también al inicio del rank 1, una posición después.
  (El contraste de tiempos se analiza en el informe; la posición dentro del
  segmento es lo que manda.)

### Comunicación MPI (parada temprana)
El reto es que, al hallar la clave, **todos los procesos paren pronto** sin
esperar a recorrer su segmento completo. Opciones a evaluar en la implementación:
- Sondeo no bloqueante periódico con `MPI_Iprobe` / `MPI_Irecv` + `MPI_Test`
  (cada proceso revisa cada N iteraciones si alguien ya avisó).
- `MPI_Ibcast` / bandera compartida de "encontrado".
- Difundir también la clave hallada al proceso 0 para reportarla.

### Estrategia alternativa (inciso 3) — ideas a analizar
El reparto contiguo es injusto cuando la clave está al final de un segmento. A
comparar e implementar al menos una:
- **Entrelazado / round-robin:** el rank `r` prueba las claves `r, r+4, r+8, ...`
  (`clave = base + r + 4*t`). Reparte uniformemente la carga sin importar dónde
  caiga la clave.
- **Reparto dinámico por bloques (master–worker):** un proceso reparte rangos
  pequeños bajo demanda; mejora el balance pero añade comunicación.
- (Analizar ventajas/limitaciones de cada una frente a la base.)

### Plan de port a C + MPI (alto nivel)
1. **Secuencial** primero: cifrar/descifrar `.txt` con AES-128 (OpenSSL/libcrypto)
   + búsqueda lineal, para tener el baseline del *Speedup*.
2. **Paralelo base**: `MPI_Init`, `MPI_Comm_rank/size`, segmentos contiguos,
   parada temprana por mensaje.
3. **Paralelo alternativo**: misma base con la estrategia entrelazada/dinámica.
4. **Bitácora**: medir con `MPI_Wtime()` las tres claves del inciso 2 en ambas
   estrategias; calcular *Speedup* vs. secuencial.
5. Programación defensiva, manejo de memoria y comentarios (rúbrica del programa).

---

## 4. Pendientes / preguntas abiertas
- Conseguir el código del **Laboratorio 03** (punto de partida obligatorio).
- Confirmar función exacta `contador(56 bits) → clave de 16 bytes` (padding de los
  bytes altos a cero) para que coincida con cómo se cifró el mensaje de prueba.
- Decidir biblioteca AES en C (OpenSSL `EVP` / `AES_*`) y modo (ECB/CBC) y el
  manejo de padding al comparar la frase de búsqueda.
- Definir cada cuántas iteraciones se sondea el mensaje de parada (costo vs.
  latencia de parada).
