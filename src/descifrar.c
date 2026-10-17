/* ============================================================================
 * descifrar.c  -  Inciso 1 del Proyecto 02.
 * Descifra un archivo producido por 'cifrar' con AES-128 y la misma clave.
 *
 * Uso:
 *     ./descifrar <entrada.bin> <salida.txt> <clave_numerica>
 *
 * Ejemplo:
 *     ./descifrar datos/cifrado_123456.bin datos/recuperado.txt 123456
 * ==========================================================================*/

#include "tm_aes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    const char *ruta_in;
    const char *ruta_out;
    uint64_t clave_num;
    unsigned char clave[TAM_CLAVE];
    unsigned char *cifrado = NULL;
    unsigned char *texto = NULL;
    long cifrado_len = 0;
    int texto_len;
    EVP_CIPHER_CTX *ctx = NULL;
    int rc = EXIT_FAILURE;

    if (argc != 4) {
        fprintf(stderr, "Uso: %s <entrada.bin> <salida.txt> <clave_numerica>\n", argv[0]);
        return EXIT_FAILURE;
    }
    ruta_in  = argv[1];
    ruta_out = argv[2];

    if (parsear_clave_numerica(argv[3], &clave_num) != 0) {
        fprintf(stderr, "Error: la clave '%s' no es un numero valido.\n", argv[3]);
        return EXIT_FAILURE;
    }
    clave_desde_contador(clave_num, clave);

    cifrado = leer_archivo(ruta_in, &cifrado_len);
    if (cifrado == NULL) {
        return EXIT_FAILURE;
    }
    if (cifrado_len <= 0 || (cifrado_len % TAM_BLOQUE) != 0) {
        fprintf(stderr, "Error: el tamano del cifrado (%ld) no es multiplo de %d.\n",
                cifrado_len, TAM_BLOQUE);
        goto limpiar;
    }

    texto = (unsigned char *)malloc((size_t)cifrado_len + 1);
    if (texto == NULL) {
        fprintf(stderr, "Error: sin memoria.\n");
        goto limpiar;
    }

    ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL) {
        fprintf(stderr, "Error: no se pudo crear el contexto AES.\n");
        goto limpiar;
    }

    texto_len = aes128_ecb_descifrar(ctx, clave, cifrado, (int)cifrado_len,
                                     texto, /*padding=*/1);
    if (texto_len < 0) {
        fprintf(stderr, "Error: fallo el descifrado (clave incorrecta o datos corruptos).\n");
        goto limpiar;
    }
    texto[texto_len] = '\0';

    if (escribir_archivo(ruta_out, texto, texto_len) != 0) {
        fprintf(stderr, "Error: no se pudo escribir '%s'.\n", ruta_out);
        goto limpiar;
    }

    printf("== Descifrado AES-128-ECB ==\n");
    printf("Clave numerica  : %llu\n", (unsigned long long)clave_num);
    printf("Entrada         : %s\n", ruta_in);
    printf("Salida          : %s\n", ruta_out);
    printf("Texto recuperado: \"%s\"\n", (const char *)texto);

    rc = EXIT_SUCCESS;

limpiar:
    if (ctx != NULL) {
        EVP_CIPHER_CTX_free(ctx);
    }
    free(texto);
    free(cifrado);
    return rc;
}
