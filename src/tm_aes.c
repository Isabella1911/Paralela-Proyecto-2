/* ============================================================================
 * tm_aes.c  -  Implementacion del modelo de MT de 4 cintas + AES-128.
 * Ver tm_aes.h para la descripcion de cada cinta y funcion.
 * ==========================================================================*/

#include "tm_aes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

/* ---------------------------------------------------------------------------
 * Ciclo de vida de la maquina.
 * -------------------------------------------------------------------------*/

int maquina_crear(MaquinaTuring *m, const unsigned char *cifrado, long len)
{
    if (m == NULL || cifrado == NULL || len <= 0 || (len % TAM_BLOQUE) != 0) {
        return -1; /* programacion defensiva: entrada invalida */
    }

    memset(m, 0, sizeof(*m));

    /* Cinta 1: entrada (solo lectura). */
    m->cinta_entrada = cifrado;
    m->len_entrada   = len;

    /* Cinta 3: trabajo (mismo tamano que el cifrado). */
    m->cinta_trabajo = (unsigned char *)malloc((size_t)len);
    if (m->cinta_trabajo == NULL) {
        return -1;
    }
    m->len_trabajo = len;

    /* Cinta 2 (clave) y Cinta 4 (control) quedan en cero por el memset. */

    /* Contexto AES reutilizable. */
    m->ctx = EVP_CIPHER_CTX_new();
    if (m->ctx == NULL) {
        free(m->cinta_trabajo);
        m->cinta_trabajo = NULL;
        return -1;
    }

    return 0;
}

void maquina_destruir(MaquinaTuring *m)
{
    if (m == NULL) {
        return;
    }
    if (m->ctx != NULL) {
        EVP_CIPHER_CTX_free(m->ctx);
        m->ctx = NULL;
    }
    if (m->cinta_trabajo != NULL) {
        free(m->cinta_trabajo);
        m->cinta_trabajo = NULL;
    }
}

/* ---------------------------------------------------------------------------
 * Cinta de clave como contador little-endian.
 * -------------------------------------------------------------------------*/

void clave_desde_contador(uint64_t n, unsigned char clave[TAM_CLAVE])
{
    int i;
    /* Bytes bajos primero (little-endian); el resto de la cinta queda en cero. */
    for (i = 0; i < 8; i++) {
        clave[i] = (unsigned char)(n & 0xFFu);
        n >>= 8;
    }
    for (i = 8; i < TAM_CLAVE; i++) {
        clave[i] = 0u;
    }
}

uint64_t contador_desde_clave(const unsigned char clave[TAM_CLAVE])
{
    uint64_t n = 0;
    int i;
    for (i = 7; i >= 0; i--) {
        n = (n << 8) | (uint64_t)clave[i];
    }
    return n;
}

void incrementar_clave(unsigned char clave[TAM_CLAVE])
{
    int i;
    /* Suma 1 al byte menos significativo; avanza solo cuando hay acarreo. */
    for (i = 0; i < TAM_CLAVE; i++) {
        clave[i] = (unsigned char)(clave[i] + 1u);
        if (clave[i] != 0u) {
            break; /* no hubo acarreo: terminamos */
        }
    }
}

/* ---------------------------------------------------------------------------
 * AES-128-ECB de proposito general.
 * -------------------------------------------------------------------------*/

int aes128_ecb_cifrar(EVP_CIPHER_CTX *ctx, const unsigned char clave[TAM_CLAVE],
                      const unsigned char *in, int in_len,
                      unsigned char *out, int padding)
{
    int len = 0;
    int total = 0;

    if (ctx == NULL || clave == NULL || in == NULL || out == NULL || in_len < 0) {
        return -1;
    }
    if (EVP_EncryptInit_ex(ctx, EVP_aes_128_ecb(), NULL, clave, NULL) != 1) {
        return -1;
    }
    EVP_CIPHER_CTX_set_padding(ctx, padding ? 1 : 0);
    if (EVP_EncryptUpdate(ctx, out, &len, in, in_len) != 1) {
        return -1;
    }
    total = len;
    if (EVP_EncryptFinal_ex(ctx, out + total, &len) != 1) {
        return -1;
    }
    total += len;
    return total;
}

int aes128_ecb_descifrar(EVP_CIPHER_CTX *ctx, const unsigned char clave[TAM_CLAVE],
                         const unsigned char *in, int in_len,
                         unsigned char *out, int padding)
{
    int len = 0;
    int total = 0;

    if (ctx == NULL || clave == NULL || in == NULL || out == NULL || in_len < 0) {
        return -1;
    }
    if (EVP_DecryptInit_ex(ctx, EVP_aes_128_ecb(), NULL, clave, NULL) != 1) {
        return -1;
    }
    EVP_CIPHER_CTX_set_padding(ctx, padding ? 1 : 0);
    if (EVP_DecryptUpdate(ctx, out, &len, in, in_len) != 1) {
        return -1;
    }
    total = len;
    if (EVP_DecryptFinal_ex(ctx, out + total, &len) != 1) {
        /* Con padding activo, una clave incorrecta suele fallar aqui. */
        return -1;
    }
    total += len;
    return total;
}

/* ---------------------------------------------------------------------------
 * Prueba de una clave.
 * -------------------------------------------------------------------------*/

static int es_imprimible(unsigned char b)
{
    /* ASCII imprimible o espacios en blanco comunes. */
    if (b >= 32u && b < 127u) {
        return 1;
    }
    return (b == '\t' || b == '\n' || b == '\r') ? 1 : 0;
}

int filtro_primer_bloque(MaquinaTuring *m)
{
    unsigned char bloque[TAM_BLOQUE];
    int i;
    int n;

    if (m == NULL || m->len_entrada < TAM_BLOQUE) {
        return 0;
    }

    /* Descifrar solo el primer bloque (ECB: cada bloque es independiente). */
    n = aes128_ecb_descifrar(m->ctx, m->cinta_clave,
                             m->cinta_entrada, TAM_BLOQUE,
                             bloque, /*padding=*/0);
    if (n != TAM_BLOQUE) {
        return 0;
    }

    for (i = 0; i < TAM_BLOQUE; i++) {
        if (!es_imprimible(bloque[i])) {
            return 0; /* bloque con basura => clave rechazada */
        }
    }
    return 1; /* candidato: vale la pena descifrar completo */
}

int verifica_frase(MaquinaTuring *m, const char *frase)
{
    int n;
    long flen;

    if (m == NULL || frase == NULL) {
        return 0;
    }

    n = aes128_ecb_descifrar(m->ctx, m->cinta_clave,
                             m->cinta_entrada, (int)m->len_entrada,
                             m->cinta_trabajo, /*padding=*/0);
    if (n <= 0) {
        return 0;
    }

    flen = (long)strlen(frase);
    return contiene_subcadena(m->cinta_trabajo, n, frase, flen);
}

/* ---------------------------------------------------------------------------
 * Utilidades.
 * -------------------------------------------------------------------------*/

int contiene_subcadena(const unsigned char *pajar, long pajar_len,
                       const char *aguja, long aguja_len)
{
    long i;

    if (pajar == NULL || aguja == NULL || aguja_len <= 0) {
        return 0;
    }
    if (aguja_len > pajar_len) {
        return 0;
    }

    for (i = 0; i <= pajar_len - aguja_len; i++) {
        if (memcmp(pajar + i, aguja, (size_t)aguja_len) == 0) {
            return 1;
        }
    }
    return 0;
}

unsigned char *leer_archivo(const char *ruta, long *out_len)
{
    FILE *f;
    long tam;
    unsigned char *buf;
    size_t leidos;

    if (ruta == NULL || out_len == NULL) {
        return NULL;
    }

    f = fopen(ruta, "rb");
    if (f == NULL) {
        fprintf(stderr, "Error: no se pudo abrir '%s': %s\n", ruta, strerror(errno));
        return NULL;
    }

    if (fseek(f, 0L, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    tam = ftell(f);
    if (tam < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);

    buf = (unsigned char *)malloc((size_t)tam > 0 ? (size_t)tam : 1);
    if (buf == NULL) {
        fclose(f);
        return NULL;
    }

    leidos = fread(buf, 1, (size_t)tam, f);
    fclose(f);

    if ((long)leidos != tam) {
        free(buf);
        return NULL;
    }

    *out_len = tam;
    return buf;
}

int escribir_archivo(const char *ruta, const unsigned char *datos, long len)
{
    FILE *f;
    size_t escritos;

    if (ruta == NULL || datos == NULL || len < 0) {
        return -1;
    }

    f = fopen(ruta, "wb");
    if (f == NULL) {
        fprintf(stderr, "Error: no se pudo crear '%s': %s\n", ruta, strerror(errno));
        return -1;
    }

    escritos = fwrite(datos, 1, (size_t)len, f);
    fclose(f);

    return ((long)escritos == len) ? 0 : -1;
}

int parsear_clave_numerica(const char *texto, uint64_t *out)
{
    char *fin = NULL;
    unsigned long long v;

    if (texto == NULL || out == NULL || *texto == '\0') {
        return -1;
    }

    errno = 0;
    v = strtoull(texto, &fin, 10);
    if (errno != 0 || fin == texto || *fin != '\0') {
        return -1;
    }

    *out = (uint64_t)v;
    return 0;
}
