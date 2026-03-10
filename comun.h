#ifndef _COMUN_H_
#define _COMUN_H_

#include "claves.h"

//cola pública del servidor.
#define SERVER_QUEUE "/mq_claves_srv_100475965_100498667"


// tamaños máximos usados en el protocolo de comunicación
#define MAX_STR   256 // 255 caracteres útiles + '\0'
#define MAX_V2    32 // tamaño máximo del vector de floats
#define MAX_QNAME 64 // tamaño máximo para el nombre de una cola de respuesta


// enumerado con los códigos de operación que el cliente puede pedir al servidor
typedef enum {
    OP_DESTROY = 1,
    OP_SET,
    OP_GET,
    OP_MODIFY,
    OP_DELETE,
    OP_EXIST
} OpType;


// estructura que el cliente envía al servidor.
typedef struct {
    OpType op;
    char reply_queue[MAX_QNAME];

    char key[MAX_STR];
    char value1[MAX_STR];
    int  N_value2;
    float V_value2[MAX_V2];
    struct Paquete value3;
} Request;


// estructura que el servidor devuelve al cliente.
typedef struct {
    int ret;
    char value1[MAX_STR];
    int  N_value2;
    float V_value2[MAX_V2];
    struct Paquete value3;
} Response;

#endif
