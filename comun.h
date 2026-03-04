#ifndef _COMUN_H_
#define _COMUN_H_

#include "claves.h"

// Define aquí el nombre de tu cola (debe empezar por /)
#define SERVER_QUEUE "/SERVIDOR_GRUPO_DPAZ"

#define MAX_STR 256
#define MAX_V2  32

// Tipos de operaciones para el servidor
typedef enum {
    OP_DESTROY,
    OP_SET_VALUE,
    OP_GET_VALUE,
    OP_MODIFY_VALUE,
    OP_DELETE_KEY,
    OP_EXIST
} OpType;

// Estructura de petición (Cliente -> Servidor)
struct Peticion {
    OpType op;
    char q_cliente[MAX_STR];
    char key[MAX_STR];
    char value1[MAX_STR];
    int  N_value2;
    float V_value2[MAX_V2];
    struct Paquete value3;
};

// Estructura de respuesta (Servidor -> Cliente)
struct Respuesta {
    int resultado;
    char value1[MAX_STR];
    int  N_value2;
    float V_value2[MAX_V2];
    struct Paquete value3;
};

#endif