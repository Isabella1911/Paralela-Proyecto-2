/* ============================================================================
 * comunicacion.h
 * ----------------------------------------------------------------------------
 * Comunicacion MPI entre las maquinas (procesos) para la busqueda de claves.
 *
 * Implementa la "cinta de control" repartida entre procesos:
 *   - Al iniciar, cada maquina deja pendiente una recepcion no bloqueante
 *     (MPI_Irecv) del aviso de "clave encontrada".
 *   - Cada K claves (no en cada iteracion) consulta ese aviso con MPI_Test,
 *     para no pagar comunicacion por clave.
 *   - La maquina que acepta envia su clave a las demas; cada una se detiene en
 *     su siguiente revision (a lo sumo K claves despues).
 *   - Al final, todos acuerdan la clave (MPI_Allreduce + MPI_Bcast) y el
 *     proceso 0 la reporta.
 * ==========================================================================*/

#ifndef COMUNICACION_H
#define COMUNICACION_H

#include <mpi.h>
#include "tm_aes.h"

/* Etiqueta del mensaje de "clave encontrada". */
#define TAG_PARADA 101

/* Cada cuantas claves se revisa el aviso de parada (parametro K del modelo). */
#ifndef PERIODO_SONDEO
#define PERIODO_SONDEO 100000ULL
#endif

typedef struct {
    MPI_Request   recv_req;              /* recepcion no bloqueante pendiente   */
    unsigned char buffer[TAM_CLAVE];     /* clave recibida de otra maquina      */
    MPI_Comm      comm;
    int           rank;
    int           size;
    int           aviso_recibido;        /* 1 si ya llego un aviso de parada    */
    int           recv_activa;           /* 1 si recv_req sigue pendiente       */
} CanalParada;

/* Inicia el canal y publica la primera recepcion no bloqueante. */
void canal_iniciar(CanalParada *c, MPI_Comm comm, int rank, int size);

/* Revisa (MPI_Test) si otra maquina aviso que encontro la clave.
 * Devuelve 1 si hay que detenerse. Pensado para llamarse cada K claves. */
int  canal_hay_parada(CanalParada *c);

/* La maquina que encontro la clave avisa a todas las demas (envios no
 * bloqueantes que seran recogidos por sus MPI_Irecv). */
void canal_anunciar(CanalParada *c, const unsigned char clave[TAM_CLAVE]);

/* Acuerdo final: combina el estado local de todas las maquinas.
 *   local_encontrada: 1 si ESTA maquina acepto.
 *   local_clave:      clave que acepto (valida solo si local_encontrada).
 * Escribe en out_clave la clave global y devuelve 1 si alguien la encontro. */
int  canal_acordar_resultado(CanalParada *c,
                             int local_encontrada,
                             const unsigned char local_clave[TAM_CLAVE],
                             unsigned char out_clave[TAM_CLAVE]);

/* Cierra el canal: cancela la recepcion pendiente y espera envios en vuelo. */
void canal_finalizar(CanalParada *c);

#endif /* COMUNICACION_H */
