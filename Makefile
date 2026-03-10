# compilador 
CC = gcc

# FLAGS DE COMPILACIÓN: -Wall -Wextra : warnings útiles; -g: información de depuración; -pthread: necesario para hilos y mutex
CFLAGS = -Wall -Wextra -g -pthread

# flag necesario para generar bibliotecas compartidas (.so) 
PICFLAGS = -fPIC

# flag para decir que queremos generar .so
LDFLAGS_SO = -shared

# hace que el ejecutable busque las .so en su propio directorio
RPATH = -Wl,-rpath,'$$ORIGIN'

# en linux, las colas POSIX suelen requerir -lrt
LDLIBS_COMMON = -pthread -lrt

# ficheros fuente
SRC_LOCAL      = claves.c
SRC_PROXY      = proxy-mq.c
SRC_SERVER     = servidor-mq.c
SRC_CLIENT     = app-cliente.c
SRC_TESTS      = tests.c
SRC_TEST_COM   = test_comunicacion.c

# archivos de cabecera
HDR_LOCAL      = claves.h
HDR_COMMON     = comun.h


# objetos que se generan al compilar
OBJ_LOCAL          = claves.o
OBJ_LOCAL_PIC      = claves.pic.o
OBJ_PROXY_PIC      = proxy-mq.pic.o
OBJ_SERVER         = servidor-mq.o


# Bibliotecas compartidas que se generan al compilar
LIB_LOCAL = libclaves.so
LIB_PROXY = libproxyclaves.so


# ejecutables que produce el proyecto
CLIENT_LOCAL = cliente_local
CLIENT_DIST  = cliente_distribuido
TESTS_LOCAL  = tests_local
TESTS_DIST   = tests_distribuido
TEST_COM     = test_comunicacion
SERVER       = servidor_mq


# regla que compila todo el proyecto
all: $(LIB_LOCAL) $(LIB_PROXY) $(CLIENT_LOCAL) $(CLIENT_DIST) \
     $(TESTS_LOCAL) $(TESTS_DIST) $(TEST_COM) $(SERVER)


# ----- PARTE A: -----

# compilamos claves.c como objeto PIC para poder crear la biblioteca dinámica
$(OBJ_LOCAL_PIC): $(SRC_LOCAL) $(HDR_LOCAL)
	$(CC) $(CFLAGS) $(PICFLAGS) -c $< -o $@

# crear la biblioteca libclaves.so 
$(LIB_LOCAL): $(OBJ_LOCAL_PIC)
	$(CC) $(LDFLAGS_SO) -o $@ $^ -lpthread


# ----- PARTE B: -----

# compilamos el proxy como objeto PIC
$(OBJ_PROXY_PIC): $(SRC_PROXY) $(HDR_LOCAL) $(HDR_COMMON)
	$(CC) $(CFLAGS) $(PICFLAGS) -c $< -o $@

# generamos la biblioteca libproxyclaves.so
$(LIB_PROXY): $(OBJ_PROXY_PIC)
	$(CC) $(LDFLAGS_SO) -o $@ $^ $(LDLIBS_COMMON)


# ----- OBJETOS NORMALES: -----

# compilación de la implementación local que reutilizará el servidor
$(OBJ_LOCAL): $(SRC_LOCAL) $(HDR_LOCAL)
	$(CC) $(CFLAGS) -c $< -o $@

# compilación del código del servidor
$(OBJ_SERVER): $(SRC_SERVER) $(HDR_LOCAL) $(HDR_COMMON)
	$(CC) $(CFLAGS) -c $< -o $@


# ----- SERVIDOR DISTRIBUIDO: -----
# El servidor enlaza con claves.o directamente así reutiliza la implementación local sin depender  de libclaves.so
$(SERVER): $(OBJ_SERVER) $(OBJ_LOCAL)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS_COMMON)


# ----- CLIENTES: -----

# cliente local: Compilación de la implementación local que reutilizará el servidor
$(CLIENT_LOCAL): $(SRC_CLIENT) $(LIB_LOCAL) $(HDR_LOCAL)
	$(CC) $(CFLAGS) -o $@ $(SRC_CLIENT) -L. -lclaves $(RPATH)

# cliente distribuido: enlaza con libproxyclaves.so
$(CLIENT_DIST): $(SRC_CLIENT) $(LIB_PROXY) $(HDR_LOCAL) $(HDR_COMMON)
	$(CC) $(CFLAGS) -o $@ $(SRC_CLIENT) -L. -lproxyclaves $(RPATH) $(LDLIBS_COMMON)


# ----- TESTS: -----

# tests de la API local
$(TESTS_LOCAL): $(SRC_TESTS) $(LIB_LOCAL) $(HDR_LOCAL)
	$(CC) $(CFLAGS) -o $@ $(SRC_TESTS) -L. -lclaves $(RPATH)

# tests de la API distribuida
$(TESTS_DIST): $(SRC_TESTS) $(LIB_PROXY) $(HDR_LOCAL) $(HDR_COMMON)
	$(CC) $(CFLAGS) -o $@ $(SRC_TESTS) -L. -lproxyclaves $(RPATH) $(LDLIBS_COMMON)

# Test específico de comunicación:debe ejecutarse con el servidor apagado para comprobar el -2
$(TEST_COM): $(SRC_TEST_COM) $(LIB_PROXY) $(HDR_LOCAL) $(HDR_COMMON)
	$(CC) $(CFLAGS) -o $@ $(SRC_TEST_COM) -L. -lproxyclaves $(RPATH) $(LDLIBS_COMMON)


# ----- REGLAS AUXILIARES: -----

# solo Parte A
local: $(LIB_LOCAL) $(CLIENT_LOCAL) $(TESTS_LOCAL)

# solo Parte B
distributed: $(LIB_PROXY) $(CLIENT_DIST) $(TESTS_DIST) $(TEST_COM) $(SERVER)

# tests locales
run_local_tests: $(TESTS_LOCAL)
	./$(TESTS_LOCAL)

#  tests distribuidos (servidor encendido)
run_dist_tests: $(TESTS_DIST)
	./$(TESTS_DIST)

# test de comunicación (servidor apagado)
run_comm_test: $(TEST_COM)
	./$(TEST_COM)


# ----- LIMPIEZA: -----
clean:
	rm -f *.o *.so $(CLIENT_LOCAL) $(CLIENT_DIST) \
	      $(TESTS_LOCAL) $(TESTS_DIST) $(TEST_COM) $(SERVER)

.PHONY: all clean local distributed run_local_tests run_dist_tests run_comm_test
