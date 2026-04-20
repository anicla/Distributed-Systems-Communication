#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tirpc/rpc/rpc.h>

#include "claves.h"
#include "clavesRPC.h"

#define MAX_STR 256
#define MAX_V2 32

static CLIENT *crear_cliente_rpc(void) {
    char *ip = getenv("IP_TUPLAS");
    CLIENT *clnt;

    if (ip == NULL || *ip == '\0') {
        fprintf(stderr, "Error: la variable de entorno IP_TUPLAS no esta definida.\n");
        return NULL;
    }

    clnt = clnt_create(ip, CLAVES_PROG, CLAVES_VERS, "tcp");
    if (clnt == NULL) {
        clnt_pcreateerror(ip);
        return NULL;
    }

    return clnt;
}

static int validar_cadena(const char *s) {
    int i;

    if (s == NULL) {
        return 0;
    }

    for (i = 0; i < MAX_STR; i++) {
        if (s[i] == '\0') {
            return 1;
        }
    }

    return 0;
}

int destroy(void) {
    CLIENT *clnt;
    int result = -1;
    enum clnt_stat stat;

    clnt = crear_cliente_rpc();
    if (clnt == NULL) {
        return -1;
    }

    stat = destroy_1(&result, clnt);
    if (stat != RPC_SUCCESS) {
        clnt_perror(clnt, "destroy_1 fallo");
        clnt_destroy(clnt);
        return -1;
    }

    clnt_destroy(clnt);
    return result;
}

int set_value(char *key, char *value1, int N_value2, float *V_value2, struct Paquete value3) {
    CLIENT *clnt;
    enum clnt_stat stat;
    int result = -1;
    TuplaArg arg;

    if (!validar_cadena(key) || !validar_cadena(value1) || V_value2 == NULL) {
        return -1;
    }
    if (N_value2 < 1 || N_value2 > MAX_V2) {
        return -1;
    }

    clnt = crear_cliente_rpc();
    if (clnt == NULL) {
        return -1;
    }

    memset(&arg, 0, sizeof(arg));
    arg.key = key;
    arg.value1 = value1;
    arg.N_value2 = N_value2;
    arg.V_value2.V_value2_len = (u_int)N_value2;
    arg.V_value2.V_value2_val = V_value2;
    arg.value3.x = value3.x;
    arg.value3.y = value3.y;
    arg.value3.z = value3.z;

    stat = set_value_1(arg, &result, clnt);
    if (stat != RPC_SUCCESS) {
        clnt_perror(clnt, "set_value_1 fallo");
        clnt_destroy(clnt);
        return -1;
    }

    clnt_destroy(clnt);
    return result;
}

int get_value(char *key, char *value1, int *N_value2, float *V_value2, struct Paquete *value3) {
    CLIENT *clnt;
    enum clnt_stat stat;
    KeyArg arg;
    GetValueResult result;
    int i;

    if (!validar_cadena(key) || value1 == NULL || N_value2 == NULL || V_value2 == NULL || value3 == NULL) {
        return -1;
    }

    clnt = crear_cliente_rpc();
    if (clnt == NULL) {
        return -1;
    }

    memset(&result, 0, sizeof(result));
    arg.key = key;

    stat = get_value_1(arg, &result, clnt);
    if (stat != RPC_SUCCESS) {
        clnt_perror(clnt, "get_value_1 fallo");
        clnt_destroy(clnt);
        return -1;
    }

    if (result.status != 0) {
        xdr_free((xdrproc_t)xdr_GetValueResult, (char *)&result);
        clnt_destroy(clnt);
        return -1;
    }

    if (result.value1 == NULL || result.N_value2 < 1 || result.N_value2 > MAX_V2) {
        xdr_free((xdrproc_t)xdr_GetValueResult, (char *)&result);
        clnt_destroy(clnt);
        return -1;
    }

    if (result.V_value2.V_value2_val == NULL || result.V_value2.V_value2_len != (u_int)result.N_value2) {
        xdr_free((xdrproc_t)xdr_GetValueResult, (char *)&result);
        clnt_destroy(clnt);
        return -1;
    }

    strncpy(value1, result.value1, MAX_STR - 1);
    value1[MAX_STR - 1] = '\0';
    *N_value2 = result.N_value2;

    for (i = 0; i < *N_value2; i++) {
        V_value2[i] = result.V_value2.V_value2_val[i];
    }
    for (i = *N_value2; i < MAX_V2; i++) {
        V_value2[i] = 0.0f;
    }

    value3->x = result.value3.x;
    value3->y = result.value3.y;
    value3->z = result.value3.z;

    xdr_free((xdrproc_t)xdr_GetValueResult, (char *)&result);
    clnt_destroy(clnt);
    return 0;
}

int modify_value(char *key, char *value1, int N_value2, float *V_value2, struct Paquete value3) {
    CLIENT *clnt;
    enum clnt_stat stat;
    int result = -1;
    TuplaArg arg;

    if (!validar_cadena(key) || !validar_cadena(value1) || V_value2 == NULL) {
        return -1;
    }
    if (N_value2 < 1 || N_value2 > MAX_V2) {
        return -1;
    }

    clnt = crear_cliente_rpc();
    if (clnt == NULL) {
        return -1;
    }

    memset(&arg, 0, sizeof(arg));
    arg.key = key;
    arg.value1 = value1;
    arg.N_value2 = N_value2;
    arg.V_value2.V_value2_len = (u_int)N_value2;
    arg.V_value2.V_value2_val = V_value2;
    arg.value3.x = value3.x;
    arg.value3.y = value3.y;
    arg.value3.z = value3.z;

    stat = modify_value_1(arg, &result, clnt);
    if (stat != RPC_SUCCESS) {
        clnt_perror(clnt, "modify_value_1 fallo");
        clnt_destroy(clnt);
        return -1;
    }

    clnt_destroy(clnt);
    return result;
}

int delete_key(char *key) {
    CLIENT *clnt;
    enum clnt_stat stat;
    int result = -1;
    KeyArg arg;

    if (!validar_cadena(key)) {
        return -1;
    }

    clnt = crear_cliente_rpc();
    if (clnt == NULL) {
        return -1;
    }

    arg.key = key;
    stat = delete_key_1(arg, &result, clnt);
    if (stat != RPC_SUCCESS) {
        clnt_perror(clnt, "delete_key_1 fallo");
        clnt_destroy(clnt);
        return -1;
    }

    clnt_destroy(clnt);
    return result;
}

int exist(char *key) {
    CLIENT *clnt;
    enum clnt_stat stat;
    int result = -1;
    KeyArg arg;

    if (!validar_cadena(key)) {
        return -1;
    }

    clnt = crear_cliente_rpc();
    if (clnt == NULL) {
        return -1;
    }

    arg.key = key;
    stat = exist_1(arg, &result, clnt);
    if (stat != RPC_SUCCESS) {
        clnt_perror(clnt, "exist_1 fallo");
        clnt_destroy(clnt);
        return -1;
    }

    clnt_destroy(clnt);
    return result;
}