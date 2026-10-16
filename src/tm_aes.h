/* ============================================================================
 * tm_aes.h
 * ----------------------------------------------------------------------------
 * Modelo de "Maquina de Turing de 4 cintas" para la busqueda de claves AES-128
 * por fuerza bruta (Proyecto 02 - Computacion Paralela y Distribuida).
 *
 * Cada proceso MPI es UNA maquina con cuatro cintas:
 *   - Cinta 1 (entrada): texto cifrado leido del archivo. Solo lectura.
 *   - Cinta 2 (clave):   16 bytes; se trata como un CONTADOR (no se reconstruye
 *                        en cada paso, solo se incrementa el byte menos
 *                        significativo con acarreo).
 *   - Cinta 3 (trabajo): buffer donde cae el descifrado AES-128.
 *   - Cinta 4 (control): bandera "encontrada" + la clave resultado.
 *
 * El paralelismo NO esta en las cintas (una MT multicinta es secuencial): viene
 * de ejecutar varias copias de la maquina (mpirun -np 4) que solo se comunican
 * cuando alguna encuentra la clave.
 * ==========================================================================*/

#ifndef TM_AES_H
#define TM_AES_H

#include <stdint.h>
#include <openssl/evp.h>

/* Tamanos de AES-128 (todos en bytes). */
#define TAM_CLAVE   16   /* clave AES-128 = 128 bits = 16 bytes */
#define TAM_BLOQUE  16   /* tamano de bloque AES = 16 bytes     */

/* Espacio de claves explorado por el proyecto: [0, 2^56). */
#define BITS_ESPACIO   56
#define ESPACIO_CLAVES (1ULL << BITS_ESPACIO)

/* Frase de busqueda que valida un descifrado correcto (enunciado inciso 2). */
#define FRASE_BUSQUEDA "es una prueba de"

/* ----------------------------------------------------------------------------
 * La maquina de 4 cintas.
 * --------------------------------------------------------------------------*/
typedef struct {
    /* Cinta 1: entrada (texto cifrado). Apunta a memoria externa; solo lectura. */
    const unsigned char *cinta_entrada;
    long                 len_entrada;      /* multiplo de TAM_BLOQUE */

    /* Cinta 2: clave de 16 bytes usada como contador little-endian. */
    unsigned char cinta_clave[TAM_CLAVE];

    /* Cinta 3: trabajo (descifrado). Reservada por maquina_crear. */
    unsigned char *cinta_trabajo;
    long           len_trabajo;

    /* Cinta 4: control. */
    int           encontrada;
    unsigned char clave_encontrada[TAM_CLAVE];

    /* Contexto AES reutilizado en todo el recorrido (evita reasignar por clave). */
    EVP_CIPHER_CTX *ctx;
} MaquinaTuring;

/* ----------------------------------------------------------------------------
 * Ciclo de vida de la maquina.
 * --------------------------------------------------------------------------*/

/* Inicializa la maquina sobre un texto cifrado dado (cinta de entrada).
 * Reserva la cinta de trabajo. Devuelve 0 en exito, -1 en error. */
int  maquina_crear(MaquinaTuring *m, const unsigned char *cifrado, long len);

/* Libera los recursos (cinta de trabajo y contexto AES). */
void maquina_destruir(MaquinaTuring *m);

/* ----------------------------------------------------------------------------
 * Operaciones sobre la cinta de clave (contador de 16 bytes, little-endian).
 * --------------------------------------------------------------------------*/

/* Escribe el entero n en la cinta de clave: bytes bajos primero, resto en cero. */
void clave_desde_contador(uint64_t n, unsigned char clave[TAM_CLAVE]);

/* Lee los 8 bytes bajos de la cinta de clave como entero (para reportar). */
uint64_t contador_desde_clave(const unsigned char clave[TAM_CLAVE]);

/* Incrementa la clave como contador: +1 al byte 0, con acarreo hacia arriba.
 * En promedio solo toca un byte (acarreo 1 de cada 256 incrementos). */
void incrementar_clave(unsigned char clave[TAM_CLAVE]);

/* ----------------------------------------------------------------------------
 * Prueba de una clave: filtro de primer bloque + verificacion de la frase.
 * --------------------------------------------------------------------------*/

/* Descifra SOLO el primer bloque (16 bytes) con la clave actual de la maquina.
 * Devuelve 1 si todos los bytes son imprimibles (candidato), 0 si se rechaza. */
int filtro_primer_bloque(MaquinaTuring *m);

/* Descifra el texto cifrado completo con la clave actual y busca la frase.
 * Devuelve 1 si el descifrado contiene 'frase'. Deja el descifrado en la
 * cinta de trabajo. */
int verifica_frase(MaquinaTuring *m, const char *frase);

/* ----------------------------------------------------------------------------
 * AES-128-ECB de proposito general (usado por las herramientas de archivo).
 * Devuelven la cantidad de bytes de salida, o -1 en error.
 * Con 'padding' != 0 se usa relleno PKCS7 (round-trip exacto de archivos).
 * --------------------------------------------------------------------------*/
int aes128_ecb_cifrar(EVP_CIPHER_CTX *ctx, const unsigned char clave[TAM_CLAVE],
                      const unsigned char *in, int in_len,
                      unsigned char *out, int padding);

int aes128_ecb_descifrar(EVP_CIPHER_CTX *ctx, const unsigned char clave[TAM_CLAVE],
                         const unsigned char *in, int in_len,
                         unsigned char *out, int padding);

/* ----------------------------------------------------------------------------
 * Utilidades.
 * --------------------------------------------------------------------------*/

/* Busca 'aguja' (needle) dentro de 'pajar' (haystack). Devuelve 1 si aparece. */
int contiene_subcadena(const unsigned char *pajar, long pajar_len,
                       const char *aguja, long aguja_len);

/* Lee un archivo completo a memoria. Devuelve el buffer (malloc) y escribe el
 * tamano en *out_len. El llamador libera con free(). NULL en error. */
unsigned char *leer_archivo(const char *ruta, long *out_len);

/* Escribe 'len' bytes de 'datos' en 'ruta'. 0 en exito, -1 en error. */
int escribir_archivo(const char *ruta, const unsigned char *datos, long len);

/* Convierte un argumento de clave numerica (decimal) a entero de 64 bits.
 * Devuelve 0 en exito, -1 si el texto no es un numero valido. */
int parsear_clave_numerica(const char *texto, uint64_t *out);

#endif /* TM_AES_H */
