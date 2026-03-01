# Compilador que voy a usar
CC = gcc

# Flags de compilación:
# -Wall -Wextra → para que me avise de posibles errores
# -g → para poder depurar si hace falta
# -fPIC → necesario para crear la librería dinámica (.so)
# -pthread → porque usamos mutex (hilos)
CFLAGS = -Wall -Wextra -g -fPIC -pthread

# Flags de enlace (también necesito pthread aquí)
LDFLAGS = -pthread


# Cuando hago simplemente "make", quiero que se generen
# la librería, el cliente y los tests
all: libclaves.so app-cliente tests_local


# Aquí creo la librería dinámica a partir del .o
libclaves.so: claves.o
	$(CC) -shared -o $@ $^


# Compilo claves.c (depende también de claves.h)
claves.o: claves.c claves.h
	$(CC) $(CFLAGS) -c claves.c


# Cliente sencillo para probar manualmente el funcionamiento
# -L. → busca la librería en el directorio actual
# -lclaves → enlaza con libclaves.so
# rpath → así no tengo que usar LD_LIBRARY_PATH
app-cliente: app-cliente.o libclaves.so
	$(CC) -o $@ app-cliente.o -L. -lclaves $(LDFLAGS) -Wl,-rpath,'$$ORIGIN'


# Compilo el cliente
app-cliente.o: app-cliente.c claves.h
	$(CC) $(CFLAGS) -c app-cliente.c


# Ejecutable con todos los tests automáticos; también enlaza contra la librería
tests_local: tests.o libclaves.so
	$(CC) -o $@ tests.o -L. -lclaves $(LDFLAGS) -Wl,-rpath,'$$ORIGIN'


# Compilo el fichero de tests
tests.o: tests.c claves.h
	$(CC) $(CFLAGS) -c tests.c


# Borra todo lo generado al compilar
clean:
	rm -f *.o *.so app-cliente tests_local