#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <unistd.h>
#include "comun.h"

// Eliminamos la redefinición de SERVER_QUEUE porque ya viene de comun.h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <unistd.h>
#include <errno.h>
#include "comun.h"

// Versión refinada de cliente_rpc en proxy-mq.c
static int cliente_rpc(struct Peticion *req, struct Respuesta *res) {
    mqd_t q_serv, q_cli;
    char name_cli[100];
    sprintf(name_cli, "/CLIENTE_%d", getpid());

    // 1. Abrir cola servidor (O_NONBLOCK es clave para robustez)
    q_serv = mq_open(SERVER_QUEUE, O_WRONLY | O_NONBLOCK);
    if (q_serv == -1) return -2;

    // 2. Crear cola cliente
    struct mq_attr attr = {.mq_maxmsg = 10, .mq_msgsize = sizeof(struct Respuesta)};
    // Usamos O_EXCL para evitar conflictos si el PID se reutiliza
    mq_unlink(name_cli);
    q_cli = mq_open(name_cli, O_CREAT | O_RDONLY | O_EXCL, 0700, &attr);
    if (q_cli == -1) {
        mq_close(q_serv);
        return -2;
    }

    strncpy(req->q_cliente, name_cli, MAX_STR);

    // 3. Envío con verificación de error
    if (mq_send(q_serv, (const char *)req, sizeof(struct Peticion), 0) == -1) {
        mq_close(q_serv); mq_close(q_cli); mq_unlink(name_cli);
        return -2;
    }

    // 4. Recepción (aquí podrías añadir un timeout para mayor robustez)
    if (mq_receive(q_cli, (char *)res, sizeof(struct Respuesta), NULL) == -1) {
        mq_close(q_serv); mq_close(q_cli); mq_unlink(name_cli);
        return -2;
    }

    mq_close(q_serv);
    mq_close(q_cli);
    mq_unlink(name_cli);

    return res->resultado;
}
int destroy(void) {
    struct Peticion req = {.op = OP_DESTROY};
    struct Respuesta res;
    return cliente_rpc(&req, &res);
}

int set_value(char *key, char *value1, int N_value2, float *V_value2, struct Paquete value3) {
    if (N_value2 > 32 || strlen(value1) > 255) return -1; // Validación local
    struct Peticion req = {.op = OP_SET_VALUE, .N_value2 = N_value2, .value3 = value3};
    struct Respuesta res;
    strncpy(req.key, key, MAX_STR);
    strncpy(req.value1, value1, MAX_STR);
    for(int i=0; i<N_value2; i++) req.V_value2[i] = V_value2[i];
    return cliente_rpc(&req, &res);
}

int get_value(char *key, char *value1, int *N_value2, float *V_value2, struct Paquete *value3) {
    struct Peticion req = {.op = OP_GET_VALUE};
    struct Respuesta res;
    strncpy(req.key, key, MAX_STR);
    int ret = cliente_rpc(&req, &res);
    if (ret == 0) {
        strncpy(value1, res.value1, MAX_STR);
        *N_value2 = res.N_value2;
        *value3 = res.value3;
        for(int i=0; i<res.N_value2; i++) V_value2[i] = res.V_value2[i];
    }
    return ret;
}

int modify_value(char *key, char *value1, int N_value2, float *V_value2, struct Paquete value3) {
    if (N_value2 > 32 || strlen(value1) > 255) return -1;
    struct Peticion req = {.op = OP_MODIFY_VALUE, .N_value2 = N_value2, .value3 = value3};
    struct Respuesta res;
    strncpy(req.key, key, MAX_STR);
    strncpy(req.value1, value1, MAX_STR);
    for(int i=0; i<N_value2; i++) req.V_value2[i] = V_value2[i];
    return cliente_rpc(&req, &res);
}

int delete_key(char *key) {
    struct Peticion req = {.op = OP_DELETE_KEY};
    struct Respuesta res;
    strncpy(req.key, key, MAX_STR);
    return cliente_rpc(&req, &res);
}

int exist(char *key) {
    struct Peticion req = {.op = OP_EXIST};
    struct Respuesta res;
    strncpy(req.key, key, MAX_STR);
    return cliente_rpc(&req, &res);
}