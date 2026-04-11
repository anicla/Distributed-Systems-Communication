#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include "claves.h"

int main(void) {
    pid_t pid = getpid(); //PID para generar una clave única por proceso
    char key[64]; //buffer: clave única del cliente
    char value1[256]; //buffer: recibir value1 desde get_value
    int N = 0; //buffer: recibir N_value2
    float v2_out[32]; //buffer: recibir V_value2
    struct Paquete p_out = {0, 0, 0}; //buffer: recibir value3

    struct Paquete p = {1, 2, 3}; //valor de ejemplo para insertar
    float v2[3] = {1.1f, 2.2f, 3.3f}; //vector de ejemplo para insertar

    snprintf(key, sizeof(key), "clave_conc_%d", pid); //creacion de la clave única usando el PID

    printf("=== CLIENTE CONCURRENTE PID %d ===\n", pid);

    //insertar una clave única para este cliente
    printf("set_value(%s): %d\n", key, set_value(key, "valor_concurrente", 3, v2, p));

    //comprobar que existe
    printf("exist(%s): %d\n", key, exist(key));

    //recuperar la tupla
    printf("get_value(%s): %d\n", key, get_value(key, value1, &N, v2_out, &p_out));
    printf("value1 = %s\n", value1);
    printf("N = %d\n", N);
    printf("p_out = (%d,%d,%d)\n", p_out.x, p_out.y, p_out.z);

    //borrar la clave propia
    printf("delete_key(%s): %d\n", key, delete_key(key));

    //comprobar que ya no existe
    printf("exist(%s) tras delete: %d\n", key, exist(key));

    return 0;
}