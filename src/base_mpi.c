/* ============================================================================
 * base_mpi.c  -  Estrategia BASE: segmentos contiguos de igual tamano.
 *
 * Cuatro maquinas (mpirun -np 4). La maquina r recorre el segmento
 *     [ r * (2^56 / size) , (r+1) * (2^56 / size) )
 * incrementando su cinta de clave como contador. Cada PERIODO_SONDEO claves
 * consulta si otra maquina ya encontro la clave, y si alguien acepta, todas
 * se detienen.
 *
 * Uso:
 *     mpirun -np 4 ./base <cifrado.bin>
 * ==========================================================================*/

#include "tm_aes.h"
#include "comunicacion.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    int rank = 0, size = 1;
    long cifrado_len = 0;
    unsigned char *cifrado = NULL;
    MaquinaTuring m;
    CanalParada canal;
    uint64_t seg, inicio, fin, n, probadas = 0;
    int local_encontrada = 0;
    unsigned char clave_global[TAM_CLAVE];
    double t0, t1, mi_tiempo, tiempo_max;
    int creada = 0, hubo_error = 0;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc != 2) {
        if (rank == 0) {
            fprintf(stderr, "Uso: mpirun -np <N> %s <cifrado.bin>\n", argv[0]);
        }
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    /* El proceso 0 lee el archivo y lo difunde a las demas maquinas. */
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

    /* Segmento contiguo de esta maquina. */
    seg    = ESPACIO_CLAVES / (uint64_t)size;
    inicio = (uint64_t)rank * seg;
    fin    = (rank == size - 1) ? ESPACIO_CLAVES : inicio + seg;

    if (rank == 0) {
        printf("== Busqueda PARALELA - estrategia BASE (segmentos contiguos) ==\n");
        printf("Procesos     : %d\n", size);
        printf("Cifrado      : %s (%ld bytes)\n", argv[1], cifrado_len);
        printf("Frase        : \"%s\"\n", FRASE_BUSQUEDA);
        printf("Sondeo cada  : %llu claves\n", (unsigned long long)PERIODO_SONDEO);
        fflush(stdout);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    t0 = MPI_Wtime();

    /* La cinta de clave arranca en el inicio del segmento y solo se incrementa. */
    clave_desde_contador(inicio, m.cinta_clave);

    for (n = inicio; n < fin; n++) {
        probadas++;

        if (filtro_primer_bloque(&m)) {
            if (verifica_frase(&m, FRASE_BUSQUEDA)) {
                local_encontrada = 1;
                memcpy(m.clave_encontrada, m.cinta_clave, TAM_CLAVE);
                canal_anunciar(&canal, m.cinta_clave); /* avisar a las demas */
                break;
            }
        }

        /* Revisar el aviso de parada solo cada PERIODO_SONDEO claves. */
        if ((probadas % PERIODO_SONDEO) == 0ULL) {
            if (canal_hay_parada(&canal)) {
                break;
            }
        }

        incrementar_clave(m.cinta_clave);
    }

    t1 = MPI_Wtime();
    mi_tiempo = t1 - t0;

    /* Acuerdo final de la clave y del tiempo (el mayor de todas las maquinas). */
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
