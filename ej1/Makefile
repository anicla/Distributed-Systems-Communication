CC = gcc
CFLAGS = -Wall -fPIC
# En Windows usamos .dll para evitar problemas de reconocimiento
LDFLAGS = -L.
LDLIBS = -lclaves -lpthread

all: libclaves.dll cliente_test

# 1. Creamos la DLL (Librería dinámica en Windows)
libclaves.dll: claves.c claves.h
	$(CC) $(CFLAGS) -shared -o libclaves.dll claves.c -lpthread

# 2. Compilamos el cliente
# Nota: Quitamos el rpath porque en Windows no funciona igual que en Linux
cliente_test: app-cliente.c libclaves.dll
	$(CC) $(CFLAGS) app-cliente.c -o cliente_test $(LDFLAGS) $(LDLIBS)

clean:
	del *.o *.dll cliente_test.exe