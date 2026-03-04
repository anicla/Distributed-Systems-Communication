#include <stdio.h>
#include <stdlib.h>
#include <mqueue.h>
#include <pthread.h>
#include <signal.h>
#include "comun.h"
#include "claves.h"

// Manejador para cerrar el servidor limpiamente
void manejador_sigint(int sig) {
    (void)sig; // Esto marca la variable como 'usada' para el compilador
    printf("\nCerrando servidor de forma segura...\n");
    mq_unlink(SERVER_QUEUE);
    exit(0);
}

void *atender_cliente(void *arg) {
    struct Peticion req = *(struct Peticion *)arg;
    free(arg);
    struct Respuesta res;
    mqd_t q_cli;

    // Procesar operación según el código de operación (OP_...)
    switch (req.op) {
        case OP_SET_VALUE:
            res.resultado = set_value(req.key, req.value1, req.N_value2, req.V_value2, req.value3);
            break;
        case OP_GET_VALUE:
            res.resultado = get_value(req.key, res.value1, &res.N_value2, res.V_value2, &res.value3);
            break;
        case OP_EXIST:
            res.resultado = exist(req.key);
            break;
        case OP_DESTROY:
            res.resultado = destroy();
            break;
        case OP_DELETE_KEY:
            res.resultado = delete_key(req.key);
            break;
        case OP_MODIFY_VALUE:
            res.resultado = modify_value(req.key, req.value1, req.N_value2, req.V_value2, req.value3);
            break;
        default:
            res.resultado = -1;
    }

    // Abrir cola del cliente para enviar respuesta
    q_cli = mq_open(req.q_cliente, O_WRONLY);
    if (q_cli != -1) {
        if (mq_send(q_cli, (const char *)&res, sizeof(res), 0) == -1) {
            perror("Error al enviar respuesta al cliente");
        }
        mq_close(q_cli);
    }

    pthread_exit(NULL);
}

int main() {
    mqd_t q_servidor;
    struct mq_attr attr = {.mq_maxmsg = 10, .mq_msgsize = sizeof(struct Peticion)};

    // 1. REGISTRAR LA SEÑAL PRIMERO
    // Esto garantiza que si pulsas Ctrl+C en cualquier momento, se limpie la cola.
    signal(SIGINT, manejador_sigint);

    // 2. Limpiar restos de ejecuciones fallidas previas
    mq_unlink(SERVER_QUEUE);

    // 3. Abrir la cola del servidor
    q_servidor = mq_open(SERVER_QUEUE, O_CREAT | O_RDONLY, 0700, &attr);
    if (q_servidor == (mqd_t)-1) {
        perror("Error al abrir la cola del servidor");
        return -1;
    }

    printf("Servidor listo y escuchando en %s...\n", SERVER_QUEUE);

    while (1) {
        struct Peticion *req = malloc(sizeof(struct Peticion));
        if (req == NULL) continue;

        // Recibir petición
        if (mq_receive(q_servidor, (char *)req, sizeof(struct Peticion), NULL) == -1) {
            perror("Error al recibir mensaje");
            free(req);
            continue;
        }

        // Crear hilo para atender al cliente (CONCURRENCIA)
        pthread_t th;
        if (pthread_create(&th, NULL, atender_cliente, req) != 0) {
            perror("Error al crear el hilo");
            free(req);
            continue;
        }

        // Desacoplar el hilo para que libere recursos automáticamente al terminar
        pthread_detach(th);
    }

    return 0;
}