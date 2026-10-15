# ============================================================================
# Makefile - Proyecto 02 (Computacion Paralela y Distribuida)
# Busqueda de claves AES-128 por fuerza bruta con Open MPI.
#
# Requisitos (Linux):
#   - gcc
#   - Open MPI       (paquete: libopenmpi-dev / openmpi)   -> mpicc, mpirun
#   - OpenSSL dev    (paquete: libssl-dev / openssl-devel) -> -lcrypto
#
# Compilar todo:   make
# Limpiar:         make clean
# ============================================================================

CC      := gcc
MPICC   := mpicc
CFLAGS  := -O2 -Wall -Wextra -std=c11
LDLIBS  := -lcrypto

SRC     := src
BIN     := bin

# Objeto compartido (AES + modelo de cintas), sin dependencias de MPI.
CORE    := $(BIN)/tm_aes.o

HERRAMIENTAS := $(BIN)/cifrar $(BIN)/descifrar $(BIN)/secuencial
PARALELOS    := $(BIN)/base $(BIN)/alternativa

.PHONY: all clean
all: $(HERRAMIENTAS) $(PARALELOS)

$(BIN):
	mkdir -p $(BIN)

# ---- Objeto compartido -----------------------------------------------------
$(CORE): $(SRC)/tm_aes.c $(SRC)/tm_aes.h | $(BIN)
	$(CC) $(CFLAGS) -c $(SRC)/tm_aes.c -o $@

# ---- Herramientas e implementacion secuencial (gcc) ------------------------
$(BIN)/cifrar: $(SRC)/cifrar.c $(CORE) | $(BIN)
	$(CC) $(CFLAGS) $(SRC)/cifrar.c $(CORE) -o $@ $(LDLIBS)

$(BIN)/descifrar: $(SRC)/descifrar.c $(CORE) | $(BIN)
	$(CC) $(CFLAGS) $(SRC)/descifrar.c $(CORE) -o $@ $(LDLIBS)

$(BIN)/secuencial: $(SRC)/secuencial.c $(CORE) | $(BIN)
	$(CC) $(CFLAGS) $(SRC)/secuencial.c $(CORE) -o $@ $(LDLIBS)

# ---- Versiones paralelas (mpicc) -------------------------------------------
$(BIN)/comunicacion.o: $(SRC)/comunicacion.c $(SRC)/comunicacion.h $(SRC)/tm_aes.h | $(BIN)
	$(MPICC) $(CFLAGS) -c $(SRC)/comunicacion.c -o $@

$(BIN)/base: $(SRC)/base_mpi.c $(CORE) $(BIN)/comunicacion.o | $(BIN)
	$(MPICC) $(CFLAGS) $(SRC)/base_mpi.c $(CORE) $(BIN)/comunicacion.o -o $@ $(LDLIBS)

$(BIN)/alternativa: $(SRC)/alternativa_mpi.c $(CORE) $(BIN)/comunicacion.o | $(BIN)
	$(MPICC) $(CFLAGS) $(SRC)/alternativa_mpi.c $(CORE) $(BIN)/comunicacion.o -o $@ $(LDLIBS)

clean:
	rm -rf $(BIN)
