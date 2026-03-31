# compilador
CC = gcc

# FLAGS DE COMPILACIÓN
CFLAGS = -Wall -Wextra -g -pthread

# flags para .so
PICFLAGS = -fPIC
LDFLAGS_SO = -shared

# buscar .so en el mismo directorio
RPATH = -Wl,-rpath,'$$ORIGIN'

# librerías comunes
LDLIBS_COMMON = -pthread

# fuentes
SRC_LOCAL        = claves.c
SRC_PROXY        = proxy-sock.c
SRC_SERVER       = servidor-sock.c
SRC_CLIENT       = app-cliente.c
SRC_CLIENT_CONC  = app-cliente-concurrente.c  

# cabeceras
HDR_LOCAL        = claves.h

# objetos
OBJ_LOCAL        = claves.o
OBJ_LOCAL_PIC    = claves.pic.o
OBJ_PROXY_PIC    = proxy-sock.pic.o
OBJ_SERVER       = servidor-sock.o

# libs
LIB_LOCAL        = libclaves.so
LIB_PROXY        = libproxyclaves.so

# ejecutables
CLIENT           = cliente
CLIENT_CONC      = cliente_concurrente   
SERVER           = servidor

# build completo
all: $(LIB_LOCAL) $(LIB_PROXY) $(CLIENT) $(CLIENT_CONC) $(SERVER)

# ----- PARTE A -----

$(OBJ_LOCAL_PIC): $(SRC_LOCAL) $(HDR_LOCAL)
	$(CC) $(CFLAGS) $(PICFLAGS) -c $< -o $@

$(LIB_LOCAL): $(OBJ_LOCAL_PIC)
	$(CC) $(LDFLAGS_SO) -o $@ $^ -lpthread

# ----- PARTE B -----

$(OBJ_PROXY_PIC): $(SRC_PROXY) $(HDR_LOCAL)
	$(CC) $(CFLAGS) $(PICFLAGS) -c $< -o $@

$(LIB_PROXY): $(OBJ_PROXY_PIC)
	$(CC) $(LDFLAGS_SO) -o $@ $^ $(LDLIBS_COMMON)

# ----- OBJETOS -----

$(OBJ_LOCAL): $(SRC_LOCAL) $(HDR_LOCAL)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_SERVER): $(SRC_SERVER) $(HDR_LOCAL)
	$(CC) $(CFLAGS) -c $< -o $@

# ----- SERVIDOR -----

$(SERVER): $(OBJ_SERVER) $(OBJ_LOCAL)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS_COMMON)

# ----- CLIENTES -----

# cliente normal
$(CLIENT): $(SRC_CLIENT) $(LIB_PROXY) $(HDR_LOCAL)
	$(CC) $(CFLAGS) -o $@ $(SRC_CLIENT) -L. -lproxyclaves $(RPATH) $(LDLIBS_COMMON)

# cliente concurrente (NUEVO)
$(CLIENT_CONC): $(SRC_CLIENT_CONC) $(LIB_PROXY) $(HDR_LOCAL)
	$(CC) $(CFLAGS) -o $@ $(SRC_CLIENT_CONC) -L. -lproxyclaves $(RPATH) $(LDLIBS_COMMON)

# ----- RUNS -----

run_server: $(SERVER)
	./$(SERVER) 4500

run_client: $(CLIENT)
	env IP_TUPLAS=127.0.0.1 PORT_TUPLAS=4500 ./$(CLIENT)

# TEST CONCURRENCIA LIMPIO
run_concurrent: $(CLIENT_CONC)
	for i in $$(seq 1 10); do \
		env IP_TUPLAS=127.0.0.1 PORT_TUPLAS=4500 ./$(CLIENT_CONC) & \
	done; \
	wait

# ----- CLEAN -----

clean:
	rm -f *.o *.so $(CLIENT) $(CLIENT_CONC) $(SERVER)

.PHONY: all clean run_server run_client run_concurrent