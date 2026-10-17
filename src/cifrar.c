/* ============================================================================
 * cifrar.c  -  Inciso 1 del Proyecto 02.
 * Cifra un archivo de texto (.txt) con AES-128 usando una clave numerica dada
 * como parametro. La misma clave sirve para descifrar con 'descifrar'.
 *
 * Uso:
 *     ./cifrar <entrada.txt> <salida.bin> <clave_numerica>
 *
 * Ejemplo (prepara el cifrado de prueba del inciso 2a con la clave 123456):
 *     ./cifrar datos/prueba.txt datos/cifrado_123456.bin 123456
 * ==========================================================================*/

#include "tm_aes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void imprimir_hex(const char *etiqueta, const unsigned char *d, long n)
{
    long i;
    long tope = n < 32 ? n : 32; /* vista previa: hasta 32 bytes */
    printf("%s (%ld bytes): ", etiqueta, n);
    for (i = 0; i < tope; i++) {
        printf("%02x", d[i]);
    }
    if (n > tope) {
        printf("...");
    }
    printf("\n");
}

int main(int argc, char **argv)
{
    const char *ruta_in;
    const char *ruta_out;
    uint64_t clave_num;
    unsigned char clave[TAM_CLAVE];
    unsigned char *texto = NULL;
    unsigned char *cifrado = NULL;
    long texto_len = 0;
    int cifrado_len;
    EVP_CIPHER_CTX *ctx = NULL;
    int rc = EXIT_FAILURE;

    if (argc != 4) {
        fprintf(stderr, "Uso: %s <entrada.txt> <salida.bin> <clave_numerica>\n", argv[0]);
        return EXIT_FAILURE;
    }
    ruta_in  = argv[1];
    ruta_out = argv[2];

    if (parsear_clave_numerica(argv[3], &clave_num) != 0) {
        fprintf(stderr, "Error: la clave '%s' no es un numero valido.\n", argv[3]);
        return EXIT_FAILURE;
    }
    clave_desde_contador(clave_num, clave);

    texto = leer_archivo(ruta_in, &texto_len);
    if (texto == NULL) {
        return EXIT_FAILURE;
    }

    /* Salida: hasta texto_len + un bloque extra por el relleno PKCS7. */
    cifrado = (unsigned char *)malloc((size_t)texto_len + TAM_BLOQUE);
    if (cifrado == NULL) {
        fprintf(stderr, "Error: sin memoria.\n");
        goto limpiar;
    }

    ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL) {
        fprintf(stderr, "Error: no se pudo crear el contexto AES.\n");
        goto limpiar;
    }

    cifrado_len = aes128_ecb_cifrar(ctx, clave, texto, (int)texto_len,
                                    cifrado, /*padding=*/1);
    if (cifrado_len < 0) {
        fprintf(stderr, "Error: fallo el cifrado AES.\n");
        goto limpiar;
    }

    if (escribir_archivo(ruta_out, cifrado, cifrado_len) != 0) {
        fprintf(stderr, "Error: no se pudo escribir '%s'.\n", ruta_out);
        goto limpiar;
    }

    printf("== Cifrado AES-128-ECB ==\n");
    printf("Clave numerica : %llu\n", (unsigned long long)clave_num);
    printf("Entrada        : %s\n", ruta_in);
    printf("Salida         : %s\n", ruta_out);
    printf("Texto original : \"%.*s\"\n", (int)texto_len, (const char *)texto);
    imprimir_hex("Texto cifrado ", cifrado, cifrado_len);

    rc = EXIT_SUCCESS;

limpiar:
    if (ctx != NULL) {
        EVP_CIPHER_CTX_free(ctx);
    }
    free(cifrado);
    free(texto);
    return rc;
}
