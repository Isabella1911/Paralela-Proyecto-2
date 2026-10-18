/* ============================================================================
 * secuencial.c  -  Version secuencial (baseline para el Speedup).
 *
 * Una sola maquina de Turing cuya cinta de clave recorre TODO el espacio
 * [0, 2^56). No recibe avisos: solo se detiene al aceptar (encontrar la frase)
 * o al agotar el rango.
 *
 * Uso:
 *     ./secuencial <cifrado.bin> [limite]
 *
 *   limite (opcional): cuantas claves probar como maximo (util para pruebas
 *   rapidas). Por defecto 2^56. Para la clave 123456 basta con un limite chico.
 *
 * Ejemplo:
 *     ./secuencial datos/cifrado_123456.bin 2000000
 * ==========================================================================*/

/* Habilita clock_gettime / CLOCK_MONOTONIC bajo -std=c11 (estandar estricto). */
#define _POSIX_C_SOURCE 199309L

#include "tm_aes.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char **argv)
{
    const char *ruta;
    unsigned char *cifrado = NULL;
    long cifrado_len = 0;
    uint64_t limite = ESPACIO_CLAVES;
    MaquinaTuring m;
    struct timespec t0, t1;
    double segundos;
    uint64_t n;
    int rc = EXIT_FAILURE;
    int creada = 0;

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Uso: %s <cifrado.bin> [limite]\n", argv[0]);
        return EXIT_FAILURE;
    }
    ruta = argv[1];

    if (argc == 3) {
        if (parsear_clave_numerica(argv[2], &limite) != 0 || limite == 0) {
            fprintf(stderr, "Error: limite invalido '%s'.\n", argv[2]);
            return EXIT_FAILURE;
        }
        if (limite > ESPACIO_CLAVES) {
            limite = ESPACIO_CLAVES;
        }
    }

    cifrado = leer_archivo(ruta, &cifrado_len);
    if (cifrado == NULL) {
        return EXIT_FAILURE;
    }

    if (maquina_crear(&m, cifrado, cifrado_len) != 0) {
        fprintf(stderr, "Error: cifrado invalido (tamano %ld, debe ser multiplo de %d).\n",
                cifrado_len, TAM_BLOQUE);
        goto limpiar;
    }
    creada = 1;

    printf("== Busqueda SECUENCIAL ==\n");
    printf("Cifrado      : %s (%ld bytes)\n", ruta, cifrado_len);
    printf("Frase        : \"%s\"\n", FRASE_BUSQUEDA);
    printf("Limite claves: %llu\n", (unsigned long long)limite);
    fflush(stdout);

    /* La cinta de clave arranca en 0 y avanza como contador. */
    clave_desde_contador(0, m.cinta_clave);

    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (n = 0; n < limite; n++) {
        if (filtro_primer_bloque(&m)) {
            if (verifica_frase(&m, FRASE_BUSQUEDA)) {
                m.encontrada = 1;
                /* Guardar el resultado en la cinta de control. */
                for (int i = 0; i < TAM_CLAVE; i++) {
                    m.clave_encontrada[i] = m.cinta_clave[i];
                }
                break;
            }
        }
        incrementar_clave(m.cinta_clave);
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    segundos = (double)(t1.tv_sec - t0.tv_sec)
             + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;

    printf("-------------------------------------------\n");
    if (m.encontrada) {
        uint64_t clave = contador_desde_clave(m.clave_encontrada);
        printf("CLAVE ENCONTRADA: %llu\n", (unsigned long long)clave);
        printf("Claves probadas : %llu\n", (unsigned long long)(n + 1));
        printf("Descifrado      : \"%.*s\"\n",
               (int)m.len_trabajo, (const char *)m.cinta_trabajo);
    } else {
        printf("Clave NO encontrada en el rango probado.\n");
        printf("Claves probadas : %llu\n", (unsigned long long)n);
    }
    printf("Tiempo          : %.6f s\n", segundos);

    rc = EXIT_SUCCESS;

limpiar:
    if (creada) {
        maquina_destruir(&m);
    }
    free(cifrado);
    return rc;
}
