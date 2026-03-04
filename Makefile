CC = gcc
CFLAGS = -Wall -Wextra -g -fPIC -pthread
LDFLAGS = -lrt -pthread

all: libclaves.so libproxy.so servidor_mq cliente_distribuido

# Librería con la lógica real (usada por el servidor)
libclaves.so: claves.o
	$(CC) -shared -o $@ $^

# Librería proxy (usada por el cliente)
libproxy.so: proxy-mq.o
	$(CC) -shared -o $@ $^

# Ejecutable del servidor
servidor_mq: servidor-mq.c libclaves.so
	$(CC) $(CFLAGS) -o $@ servidor-mq.c -L. -lclaves -Wl,-rpath,'$$ORIGIN' $(LDFLAGS)

# Ejecutable del cliente distribuido (usa app-cliente.c de la parte A)
cliente_distribuido: app-cliente.c libproxy.so
	$(CC) $(CFLAGS) -o $@ app-cliente.c -L. -lproxy -Wl,-rpath,'$$ORIGIN' $(LDFLAGS)

# Regla para compilar el objeto del proxy
proxy-mq.o: proxy-mq.c comun.h claves.h
	$(CC) $(CFLAGS) -c proxy-mq.c -o proxy-mq.o

clean:
	rm -f *.o *.so servidor_mq cliente_distribuido