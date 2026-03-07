#ifndef _COMUN_H_
#define _COMUN_H_

#include "claves.h"

#define SERVER_QUEUE "/mq_claves_srv_100475965_100498667"

#define MAX_STR   256
#define MAX_V2    32
#define MAX_QNAME 64

typedef enum {
    OP_DESTROY = 1,
    OP_SET,
    OP_GET,
    OP_MODIFY,
    OP_DELETE,
    OP_EXIST
} OpType;

typedef struct {
    OpType op;
    char reply_queue[MAX_QNAME];

    char key[MAX_STR];
    char value1[MAX_STR];
    int  N_value2;
    float V_value2[MAX_V2];
    struct Paquete value3;
} Request;

typedef struct {
    int ret;
    char value1[MAX_STR];
    int  N_value2;
    float V_value2[MAX_V2];
    struct Paquete value3;
} Response;

#endif
