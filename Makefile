CC = gcc
CFLAGS = -Wall -Wextra -g -fPIC -I/usr/include/tirpc
RPATH = -Wl,-rpath,'$$ORIGIN'
RPCGEN = rpcgen

RPC_BASE = clavesRPC
RPC_X = $(RPC_BASE).x
RPC_GEN = $(RPC_BASE).h $(RPC_BASE)_clnt.c $(RPC_BASE)_svc.c $(RPC_BASE)_xdr.c

LIB_LOCAL = libclaves.so
LIB_PROXY = libproxyclaves.so
SERVER = clavesRPC_server
CLIENT = cliente

LDLIBS_RPC = -ltirpc -lpthread
LDLIBS_LOCAL = -lpthread

all: $(LIB_LOCAL) $(LIB_PROXY) $(SERVER) $(CLIENT)

rpc: $(RPC_GEN)

$(RPC_GEN): $(RPC_X)
	$(RPCGEN) -NM $(RPC_X)

claves.pic.o: claves.c claves.h
	$(CC) $(CFLAGS) -c claves.c -o $@

$(LIB_LOCAL): claves.pic.o
	$(CC) -shared -o $@ $^ $(LDLIBS_LOCAL)

proxy_rpc.pic.o: proxy_rpc.c claves.h $(RPC_BASE).h
	$(CC) $(CFLAGS) -c proxy_rpc.c -o $@

$(RPC_BASE)_clnt.pic.o: $(RPC_BASE)_clnt.c $(RPC_BASE).h
	$(CC) $(CFLAGS) -c $(RPC_BASE)_clnt.c -o $@

$(RPC_BASE)_xdr.pic.o: $(RPC_BASE)_xdr.c $(RPC_BASE).h
	$(CC) $(CFLAGS) -c $(RPC_BASE)_xdr.c -o $@

$(LIB_PROXY): rpc proxy_rpc.pic.o $(RPC_BASE)_clnt.pic.o $(RPC_BASE)_xdr.pic.o
	$(CC) -shared -o $@ proxy_rpc.pic.o $(RPC_BASE)_clnt.pic.o $(RPC_BASE)_xdr.pic.o $(LDLIBS_RPC)

$(RPC_BASE)_svc.o: $(RPC_BASE)_svc.c $(RPC_BASE).h
	$(CC) $(CFLAGS) -c $(RPC_BASE)_svc.c -o $@

rpc_service.o: rpc_service.c claves.h $(RPC_BASE).h
	$(CC) $(CFLAGS) -c rpc_service.c -o $@

$(RPC_BASE)_xdr.o: $(RPC_BASE)_xdr.c $(RPC_BASE).h
	$(CC) $(CFLAGS) -c $(RPC_BASE)_xdr.c -o $@

$(SERVER): rpc $(RPC_BASE)_svc.o rpc_service.o $(RPC_BASE)_xdr.o claves.pic.o
	$(CC) -o $@ $(RPC_BASE)_svc.o rpc_service.o $(RPC_BASE)_xdr.o claves.pic.o $(LDLIBS_RPC)

app-cliente.o: app-cliente.c claves.h
	$(CC) -Wall -Wextra -g -c app-cliente.c -o $@

$(CLIENT): app-cliente.o $(LIB_PROXY)
	$(CC) -Wall -Wextra -g -o $@ app-cliente.o -L. -lproxyclaves $(RPATH) $(LDLIBS_RPC)

clean:
	rm -f *.o *.so $(SERVER) $(CLIENT) $(RPC_GEN)

.PHONY: all clean rpc