# =========================================================
# Makefile - Práctica de Sistemas Distribuidos
# Parte A: biblioteca local
# Parte B: biblioteca proxy + servidor con colas de mensajes
# =========================================================

# ==============================
# Compilador y banderas
# ==============================
CC = gcc

# -Wall -Wextra : warnings útiles
# -g            : información de depuración
# -pthread      : necesario para hilos y mutex
CFLAGS = -Wall -Wextra -g -pthread

# Código independiente de posición para bibliotecas compartidas
PICFLAGS = -fPIC

# Generación de .so
LDFLAGS_SO = -shared

# Hace que el ejecutable busque las .so en su propio directorio
RPATH = -Wl,-rpath,'$$ORIGIN'

# En Linux, las colas POSIX suelen requerir -lrt
LDLIBS_COMMON = -pthread -lrt


# ==============================
# Ficheros fuente
# ==============================
SRC_LOCAL      = claves.c
SRC_PROXY      = proxy-mq.c
SRC_SERVER     = servidor-mq.c
SRC_CLIENT     = app-cliente.c
SRC_TESTS      = tests.c
SRC_TEST_COM   = test_comunicacion.c


# ==============================
# Ficheros de cabecera
# ==============================
HDR_LOCAL      = claves.h
HDR_COMMON     = comun.h


# ==============================
# Objetos
# ==============================
OBJ_LOCAL          = claves.o
OBJ_LOCAL_PIC      = claves.pic.o
OBJ_PROXY_PIC      = proxy-mq.pic.o
OBJ_SERVER         = servidor-mq.o


# ==============================
# Bibliotecas compartidas
# ==============================
LIB_LOCAL = libclaves.so
LIB_PROXY = libproxyclaves.so


# ==============================
# Ejecutables
# ==============================
CLIENT_LOCAL = cliente_local
CLIENT_DIST  = cliente_distribuido
TESTS_LOCAL  = tests_local
TESTS_DIST   = tests_distribuido
TEST_COM     = test_comunicacion
SERVER       = servidor_mq


# ==============================
# Regla principal
# ==============================
all: $(LIB_LOCAL) $(LIB_PROXY) $(CLIENT_LOCAL) $(CLIENT_DIST) \
     $(TESTS_LOCAL) $(TESTS_DIST) $(TEST_COM) $(SERVER)


# =========================================================
# PARTE A - Biblioteca local
# =========================================================

# Objeto PIC para construir la biblioteca dinámica local
$(OBJ_LOCAL_PIC): $(SRC_LOCAL) $(HDR_LOCAL)
	$(CC) $(CFLAGS) $(PICFLAGS) -c $< -o $@

# Biblioteca compartida local
$(LIB_LOCAL): $(OBJ_LOCAL_PIC)
	$(CC) $(LDFLAGS_SO) -o $@ $^ -lpthread


# =========================================================
# PARTE B - Biblioteca proxy
# =========================================================

# Objeto PIC para construir la biblioteca dinámica proxy
$(OBJ_PROXY_PIC): $(SRC_PROXY) $(HDR_LOCAL) $(HDR_COMMON)
	$(CC) $(CFLAGS) $(PICFLAGS) -c $< -o $@

# Biblioteca compartida proxy
$(LIB_PROXY): $(OBJ_PROXY_PIC)
	$(CC) $(LDFLAGS_SO) -o $@ $^ $(LDLIBS_COMMON)


# =========================================================
# OBJETOS NORMALES (no PIC) para el servidor
# =========================================================

# Implementación local reutilizada por el servidor
$(OBJ_LOCAL): $(SRC_LOCAL) $(HDR_LOCAL)
	$(CC) $(CFLAGS) -c $< -o $@

# Código del servidor
$(OBJ_SERVER): $(SRC_SERVER) $(HDR_LOCAL) $(HDR_COMMON)
	$(CC) $(CFLAGS) -c $< -o $@


# =========================================================
# SERVIDOR DISTRIBUIDO
# =========================================================

# El servidor enlaza con claves.o directamente.
# Así reutiliza la implementación local sin depender en ejecución
# de libclaves.so, lo que suele ser más robusto para la práctica.
$(SERVER): $(OBJ_SERVER) $(OBJ_LOCAL)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS_COMMON)


# =========================================================
# CLIENTES
# =========================================================

# Cliente local: enlaza contra libclaves.so
$(CLIENT_LOCAL): $(SRC_CLIENT) $(LIB_LOCAL) $(HDR_LOCAL)
	$(CC) $(CFLAGS) -o $@ $(SRC_CLIENT) -L. -lclaves $(RPATH)

# Cliente distribuido: enlaza contra libproxyclaves.so
$(CLIENT_DIST): $(SRC_CLIENT) $(LIB_PROXY) $(HDR_LOCAL) $(HDR_COMMON)
	$(CC) $(CFLAGS) -o $@ $(SRC_CLIENT) -L. -lproxyclaves $(RPATH) $(LDLIBS_COMMON)


# =========================================================
# TESTS
# =========================================================

# Tests de la API local
$(TESTS_LOCAL): $(SRC_TESTS) $(LIB_LOCAL) $(HDR_LOCAL)
	$(CC) $(CFLAGS) -o $@ $(SRC_TESTS) -L. -lclaves $(RPATH)

# Tests de la API distribuida
$(TESTS_DIST): $(SRC_TESTS) $(LIB_PROXY) $(HDR_LOCAL) $(HDR_COMMON)
	$(CC) $(CFLAGS) -o $@ $(SRC_TESTS) -L. -lproxyclaves $(RPATH) $(LDLIBS_COMMON)

# Test específico de comunicación:
# debe ejecutarse con el servidor apagado para comprobar el -2
$(TEST_COM): $(SRC_TEST_COM) $(LIB_PROXY) $(HDR_LOCAL) $(HDR_COMMON)
	$(CC) $(CFLAGS) -o $@ $(SRC_TEST_COM) -L. -lproxyclaves $(RPATH) $(LDLIBS_COMMON)


# =========================================================
# REGLAS AUXILIARES
# =========================================================

# Solo Parte A
local: $(LIB_LOCAL) $(CLIENT_LOCAL) $(TESTS_LOCAL)

# Solo Parte B
distributed: $(LIB_PROXY) $(CLIENT_DIST) $(TESTS_DIST) $(TEST_COM) $(SERVER)

# Ejecutar tests locales
run_local_tests: $(TESTS_LOCAL)
	./$(TESTS_LOCAL)

# Ejecutar tests distribuidos (requiere servidor encendido)
run_dist_tests: $(TESTS_DIST)
	./$(TESTS_DIST)

# Ejecutar test de comunicación (requiere servidor apagado)
run_comm_test: $(TEST_COM)
	./$(TEST_COM)


# =========================================================
# LIMPIEZA
# =========================================================
clean:
	rm -f *.o *.so $(CLIENT_LOCAL) $(CLIENT_DIST) \
	      $(TESTS_LOCAL) $(TESTS_DIST) $(TEST_COM) $(SERVER)

.PHONY: all clean local distributed run_local_tests run_dist_tests run_comm_test
