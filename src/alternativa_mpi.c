/* ============================================================================
 * alternativa_mpi.c  -  Estrategia ALTERNATIVA: chunks intercalados.
 *
 * El espacio [0, 2^56) se parte en chunks de tamano C. El chunk j se asigna a
 * la maquina (j mod size), de forma intercalada. Cada maquina recorre sus
 * chunks (j = rank, rank+size, rank+2*size, ...); al iniciar un chunk escribe
 * su valor inicial en la cinta de clave y dentro del chunk solo incrementa.
 *
 * Como los chunks se cubren en orden ascendente global, las cuatro maquinas
 * prueban primero las claves cortas (mas relleno de ceros): el padding funciona
 * tambien como heuristica de orden. C es un parametro:
 *   - C pequeno  -> mejor reparto de las claves bajas.
 *   - C grande   -> menos saltos (reescrituras) de la cinta de clave.
 *
 * Uso:
 *     mpirun -np 4 ./alternativa <cifrado.bin> [C]
 * ==========================================================================*/

#include "tm_aes.h"
#include "comunicacion.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Tamano de chunk por defecto (2^20 claves). */
#define CHUNK_DEFECTO (1ULL << 20)

int main(int argc, char **argv)
{
    int rank = 0, size = 1;
    long cifrado_len = 0;
    unsigned char *cifrado = NULL;
    MaquinaTuring m;
    CanalParada canal;
    uint64_t C = CHUNK_DEFECTO;
    uint64_t j, probadas = 0;
    int local_encontrada = 0, parar = 0;
    unsigned char clave_global[TAM_CLAVE];
    double t0, t1, mi_tiempo, tiempo_max;
    int creada = 0, hubo_error = 0;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 2 || argc > 3) {
        if (rank == 0) {
            fprintf(stderr, "Uso: mpirun -np <N> %s <cifrado.bin> [C]\n", argv[0]);
        }
        MPI_Finalize();
        return EXIT_FAILURE;
    }
    if (argc == 3) {
        if (parsear_clave_numerica(argv[2], &C) != 0 || C == 0) {
            if (rank == 0) {
                fprintf(stderr, "Error: tamano de chunk invalido '%s'.\n", argv[2]);
            }
            MPI_Finalize();
            return EXIT_FAILURE;
        }
    }

    /* El proceso 0 lee el archivo y lo difunde. */
    if (rank == 0) {
        cifrado = leer_archivo(argv[1], &cifrado_len);
        if (cifrado == NULL || cifrado_len <= 0 || (cifrado_len % TAM_BLOQUE) != 0) {
            hubo_error = 1;
            cifrado_len = 0;
        }
    }
    MPI_Bcast(&hubo_error, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (hubo_error) {
        if (rank == 0) {
            fprintf(stderr, "Error: no se pudo leer el cifrado o su tamano es invalido.\n");
        }
        free(cifrado);
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    MPI_Bcast(&cifrado_len, 1, MPI_LONG, 0, MPI_COMM_WORLD);
    if (rank != 0) {
        cifrado = (unsigned char *)malloc((size_t)cifrado_len);
        if (cifrado == NULL) {
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }
    MPI_Bcast(cifrado, (int)cifrado_len, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

    if (maquina_crear(&m, cifrado, cifrado_len) != 0) {
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    creada = 1;

    canal_iniciar(&canal, MPI_COMM_WORLD, rank, size);

    if (rank == 0) {
        printf("== Busqueda PARALELA - estrategia ALTERNATIVA (chunks intercalados) ==\n");
        printf("Procesos     : %d\n", size);
        printf("Cifrado      : %s (%ld bytes)\n", argv[1], cifrado_len);
        printf("Frase        : \"%s\"\n", FRASE_BUSQUEDA);
        printf("Tamano chunk : %llu\n", (unsigned long long)C);
        printf("Sondeo cada  : %llu claves\n", (unsigned long long)PERIODO_SONDEO);
        fflush(stdout);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    t0 = MPI_Wtime();

    /* Esta maquina recorre los chunks j = rank, rank+size, rank+2*size, ... */
    for (j = (uint64_t)rank; j * C < ESPACIO_CLAVES && !parar && !local_encontrada;
         j += (uint64_t)size) {

        uint64_t chunk_ini = j * C;
        uint64_t chunk_fin = chunk_ini + C;
        uint64_t n;

        if (chunk_fin > ESPACIO_CLAVES) {
            chunk_fin = ESPACIO_CLAVES;
        }

        /* Al iniciar el chunk, la cabeza escribe su valor inicial. */
        clave_desde_contador(chunk_ini, m.cinta_clave);

        for (n = chunk_ini; n < chunk_fin; n++) {
            probadas++;

            if (filtro_primer_bloque(&m)) {
                if (verifica_frase(&m, FRASE_BUSQUEDA)) {
                    local_encontrada = 1;
                    memcpy(m.clave_encontrada, m.cinta_clave, TAM_CLAVE);
                    canal_anunciar(&canal, m.cinta_clave);
                    break;
                }
            }

            if ((probadas % PERIODO_SONDEO) == 0ULL) {
                if (canal_hay_parada(&canal)) {
                    parar = 1;
                    break;
                }
            }

            incrementar_clave(m.cinta_clave);
        }
    }

    t1 = MPI_Wtime();
    mi_tiempo = t1 - t0;

    int hay = canal_acordar_resultado(&canal, local_encontrada,
                                       m.clave_encontrada, clave_global);
    MPI_Reduce(&mi_tiempo, &tiempo_max, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("-------------------------------------------\n");
        if (hay) {
            uint64_t clave = contador_desde_clave(clave_global);
            printf("CLAVE ENCONTRADA: %llu\n", (unsigned long long)clave);
        } else {
            printf("Clave NO encontrada en [0, 2^%d).\n", BITS_ESPACIO);
        }
        printf("Tiempo (max)    : %.6f s\n", tiempo_max);
        fflush(stdout);
    }

    canal_finalizar(&canal);
    if (creada) {
        maquina_destruir(&m);
    }
    free(cifrado);

    MPI_Finalize();
    return EXIT_SUCCESS;
}
