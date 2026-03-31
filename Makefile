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

# librerías comunes: en esta práctica con sockets TCP necesitamos -pthread, pero no -lrt
LDLIBS_COMMON = -pthread

# ficheros fuente
SRC_LOCAL        = claves.c
SRC_PROXY        = proxy-sock.c
SRC_SERVER       = servidor-sock.c
SRC_CLIENT       = app-cliente.c

# archivos de cabecera
HDR_LOCAL        = claves.h

# objetos que se generan al compilar
OBJ_LOCAL        = claves.o
OBJ_LOCAL_PIC    = claves.pic.o
OBJ_PROXY_PIC    = proxy-sock.pic.o
OBJ_SERVER       = servidor-sock.o

# bibliotecas compartidas que se generan al compilar
LIB_LOCAL        = libclaves.so
LIB_PROXY        = libproxyclaves.so

# ejecutables que produce el proyecto
CLIENT           = cliente
SERVER           = servidor

# regla que compila todo el proyecto
all: $(LIB_LOCAL) $(LIB_PROXY) $(CLIENT) $(SERVER)

# ----- PARTE A: -----

# compilamos claves.c como objeto PIC para poder crear la biblioteca dinámica
$(OBJ_LOCAL_PIC): $(SRC_LOCAL) $(HDR_LOCAL)
	$(CC) $(CFLAGS) $(PICFLAGS) -c $< -o $@

# crear la biblioteca libclaves.so
$(LIB_LOCAL): $(OBJ_LOCAL_PIC)
	$(CC) $(LDFLAGS_SO) -o $@ $^ -lpthread

# ----- PARTE B: -----

# compilamos el proxy como objeto PIC
$(OBJ_PROXY_PIC): $(SRC_PROXY) $(HDR_LOCAL)
	$(CC) $(CFLAGS) $(PICFLAGS) -c $< -o $@

# generamos la biblioteca libproxyclaves.so
$(LIB_PROXY): $(OBJ_PROXY_PIC)
	$(CC) $(LDFLAGS_SO) -o $@ $^ $(LDLIBS_COMMON)

# ----- OBJETOS NORMALES: -----

# compilación de la implementación local que reutilizará el servidor
$(OBJ_LOCAL): $(SRC_LOCAL) $(HDR_LOCAL)
	$(CC) $(CFLAGS) -c $< -o $@

# compilación del código del servidor
$(OBJ_SERVER): $(SRC_SERVER) $(HDR_LOCAL)
	$(CC) $(CFLAGS) -c $< -o $@

# ----- SERVIDOR DISTRIBUIDO: -----
# El servidor enlaza con claves.o directamente así reutiliza la implementación local sin depender de libclaves.so
$(SERVER): $(OBJ_SERVER) $(OBJ_LOCAL)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS_COMMON)

# ----- CLIENTE: -----

# cliente distribuido: enlaza con libproxyclaves.so
$(CLIENT): $(SRC_CLIENT) $(LIB_PROXY) $(HDR_LOCAL)
	$(CC) $(CFLAGS) -o $@ $(SRC_CLIENT) -L. -lproxyclaves $(RPATH) $(LDLIBS_COMMON)

# ----- REGLAS AUXILIARES: -----

# ejecuta el servidor en el puerto 4500
run_server: $(SERVER)
	./$(SERVER) 4500

# ejecuta el cliente con las variables de entorno necesarias
run_client: $(CLIENT)
	env IP_TUPLAS=127.0.0.1 PORT_TUPLAS=4500 ./$(CLIENT)

# ----- LIMPIEZA: -----
clean:
	rm -f *.o *.so $(CLIENT) $(SERVER)

.PHONY: all clean run_server run_client