# Código fuente — Proyecto 02 (búsqueda de clave AES-128 con MPI)

Traducción a C de la propuesta de la **Máquina de Turing de 4 cintas**. Cada
proceso MPI es una máquina con cuatro cintas; el paralelismo viene de correr
varias copias (`mpirun -np 4`) que solo se comunican al encontrar la clave.

## Mapa: cintas de la MT ↔ código

| Cinta | Rol | Dónde está |
|---|---|---|
| 1 — entrada | texto cifrado (solo lectura) | `MaquinaTuring.cinta_entrada` |
| 2 — clave | contador de 16 bytes (LSB primero, acarreo) | `cinta_clave` + `incrementar_clave` |
| 3 — trabajo | buffer del descifrado AES | `cinta_trabajo` + `verifica_frase` |
| 4 — control | bandera "encontrada" + clave resultado + avisos MPI | `cinta_control` / `comunicacion.c` |

- **Filtro de primer bloque** (`filtro_primer_bloque`): descifra solo los primeros
  16 bytes; si alguno no es imprimible, descarta la clave sin descifrar todo.
- **Verificación** (`verifica_frase`): descifra completo y busca `"es una prueba de"`.
- **Clave como contador** (`incrementar_clave`): suma 1 al byte menos
  significativo y solo propaga acarreo (≈1 de cada 256 incrementos).

## Archivos

| Archivo | Qué es |
|---|---|
| `tm_aes.h/.c` | modelo de 4 cintas + AES-128-ECB (OpenSSL) + utilidades |
| `cifrar.c` | inciso 1: cifra un `.txt` con AES-128 y clave numérica |
| `descifrar.c` | inciso 1: descifra con la misma clave |
| `secuencial.c` | versión secuencial (baseline del *Speedup*) |
| `comunicacion.h/.c` | canal de parada MPI (Irecv/Test, anuncio, acuerdo final) |
| `base_mpi.c` | estrategia BASE: segmentos contiguos |
| `alternativa_mpi.c` | estrategia ALTERNATIVA: chunks intercalados |

## Compilar

Requiere `gcc`, **Open MPI** (`mpicc`, `mpirun`) y **OpenSSL dev** (`-lcrypto`).

```bash
sudo apt install build-essential libopenmpi-dev openmpi-bin libssl-dev   # Debian/Ubuntu
make
```

Los binarios quedan en `bin/`.

## Uso

### Inciso 1 — cifrar / descifrar un archivo
```bash
./bin/cifrar   datos/prueba.txt datos/cifrado.bin 123456
./bin/descifrar datos/cifrado.bin datos/recuperado.txt 123456
```

### Inciso 2 — pruebas de fuerza bruta (4 procesos)
El texto de prueba `datos/prueba.txt` ya contiene
`Esta es una prueba de proyecto 2`. Para cada prueba se cifra con la clave
indicada y luego se busca:

```bash
# a) clave 123456
./bin/cifrar datos/prueba.txt datos/c_a.bin 123456
mpirun -np 4 ./bin/base datos/c_a.bin

# b) clave 2^56/4 = 18014398509481984
./bin/cifrar datos/prueba.txt datos/c_b.bin 18014398509481984
mpirun -np 4 ./bin/base datos/c_b.bin        # puede tardar MUCHO

# c) clave 2^56/4 + 1 = 18014398509481985
./bin/cifrar datos/prueba.txt datos/c_c.bin 18014398509481985
mpirun -np 4 ./bin/base datos/c_c.bin
```

Versión secuencial (para `123456`, acotando el rango para que sea rápida):
```bash
./bin/secuencial datos/c_a.bin 2000000
```

### Inciso 3 — estrategia alternativa (mismas claves)
```bash
mpirun -np 4 ./bin/alternativa datos/c_a.bin            # C por defecto (2^20)
mpirun -np 4 ./bin/alternativa datos/c_a.bin 1048576    # C explícito
```

## Notas de diseño
- **AES-128-ECB** en todo el proyecto: en ECB cada bloque es independiente, lo
  que hace barato el filtro de primer bloque. `cifrar/descifrar` usan relleno
  PKCS7 (round-trip exacto); la búsqueda descifra sin relleno y busca la frase
  como subcadena.
- **Mapeo clave numérica → 16 bytes**: little-endian en los bytes bajos, el
  resto en cero (una clave < 2^56 ocupa ≤ 7 bytes). `cifrar` y la búsqueda usan
  exactamente el mismo mapeo, por eso la búsqueda reencuentra la clave.
- **Parámetros**: `PERIODO_SONDEO` (cada cuántas claves se revisa el aviso de
  parada) en `comunicacion.h`; `C` (tamaño de chunk) como argumento de
  `alternativa`.
- **Por qué varía el tiempo (inciso 2d)**: el tiempo depende de la posición de
  la clave dentro del segmento del proceso, no de su valor absoluto. `123456`
  cae al inicio del segmento del rank 0; `2^56/4` es justo el inicio del segmento
  del rank 1 (con `-np 4`, segmento = `2^54 = 18014398509481984`).
