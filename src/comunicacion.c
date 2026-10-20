/* ============================================================================
 * comunicacion.c  -  Ver comunicacion.h para el contrato de cada funcion.
 * ==========================================================================*/

#include "comunicacion.h"

#include <stdlib.h>
#include <string.h>

void canal_iniciar(CanalParada *c, MPI_Comm comm, int rank, int size)
{
    memset(c, 0, sizeof(*c));
    c->comm = comm;
    c->rank = rank;
    c->size = size;
    c->aviso_recibido = 0;
    c->recv_activa = 0;

    /* Recepcion no bloqueante del aviso de parada, desde cualquier maquina. */
    MPI_Irecv(c->buffer, TAM_CLAVE, MPI_UNSIGNED_CHAR,
              MPI_ANY_SOURCE, TAG_PARADA, comm, &c->recv_req);
    c->recv_activa = 1;
}

int canal_hay_parada(CanalParada *c)
{
    int listo = 0;
    MPI_Status st;

    if (c->aviso_recibido) {
        return 1;
    }
    if (!c->recv_activa) {
        return 0;
    }

    MPI_Test(&c->recv_req, &listo, &st);
    if (listo) {
        c->aviso_recibido = 1;
        c->recv_activa = 0; /* MPI_Test dejo recv_req en MPI_REQUEST_NULL */
    }
    return c->aviso_recibido;
}

void canal_anunciar(CanalParada *c, const unsigned char clave[TAM_CLAVE])
{
    int r;
    int idx = 0;

    /* Copia estable de la clave mientras los envios estan en vuelo. */
    memcpy(c->buffer, clave, TAM_CLAVE);

    /* Enviar a todas las demas maquinas (no a si misma). */
    for (r = 0; r < c->size; r++) {
        if (r == c->rank) {
            continue;
        }
        /* Envio bloqueante estandar: hay un MPI_Irecv esperando en cada destino. */
        MPI_Send(c->buffer, TAM_CLAVE, MPI_UNSIGNED_CHAR, r, TAG_PARADA, c->comm);
        idx++;
    }
    (void)idx;
}

int canal_acordar_resultado(CanalParada *c,
                            int local_encontrada,
                            const unsigned char local_clave[TAM_CLAVE],
                            unsigned char out_clave[TAM_CLAVE])
{
    int alguna = 0;
    int mi_rank = local_encontrada ? c->rank : -1;
    int finder = -1;
    unsigned char tmp[TAM_CLAVE];

    /* ¿Alguna maquina encontro la clave? */
    MPI_Allreduce(&local_encontrada, &alguna, 1, MPI_INT, MPI_LOR, c->comm);
    if (!alguna) {
        return 0;
    }

    /* Rank de la maquina que la encontro (el mayor, por si hubiera empate). */
    MPI_Allreduce(&mi_rank, &finder, 1, MPI_INT, MPI_MAX, c->comm);

    /* Esa maquina difunde la clave a todas. */
    if (c->rank == finder) {
        memcpy(tmp, local_clave, TAM_CLAVE);
    }
    MPI_Bcast(tmp, TAM_CLAVE, MPI_UNSIGNED_CHAR, finder, c->comm);

    memcpy(out_clave, tmp, TAM_CLAVE);
    return 1;
}

void canal_finalizar(CanalParada *c)
{
    MPI_Status st;

    /* Cancelar la recepcion pendiente si no fue satisfecha. */
    if (c->recv_activa) {
        MPI_Cancel(&c->recv_req);
        MPI_Wait(&c->recv_req, &st);
        c->recv_activa = 0;
    }
}
