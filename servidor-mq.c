// Parte B: servidor concurrente con colas POSIX 
// El diseño se basa en hilo por petición y se hace detach

#define _POSIX_C_SOURCE 200809L // mq_open y sigaction

#include "comun.h"
#include "claves.h"  // prototipos + struct Paquete
#include <mqueue.h>  // colas POSIX
#include <pthread.h> // threads
#include <signal.h>  // manejo de señales
#include <stdio.h>   // printf, perror
#include <stdlib.h>  // malloc, free
#include <string.h>  // memset, strncpy, strlen
#include <errno.h>   // errno
#include <unistd.h>  // _exit




static mqd_t g_srv_mq = (mqd_t)-1; // Descriptor de la cola del servidor, global para poder cerrarla desde el manejador de señales

volatile sig_atomic_t g_salir = 0; // Flag para indicar que el servidor debe salir

// Función para limpiar la cola del servidor y salir con el código dado
static void cleanup_and_exit(int code) {
    if (g_srv_mq != (mqd_t)-1) mq_close(g_srv_mq);
    mq_unlink(SERVER_QUEUE);
    _exit(code);
}

// Manejador de señales para limpiar la cola del servidor antes de salir
static void on_sigint(int sig) {
    (void)sig;
    g_salir = 1; // Solo levantamos la bandera
}

// Argumento para cada thread worker, contiene la solicitud a procesar
typedef struct {
    Request req;
} worker_arg_t;

// Función que ejecuta cada thread para procesar una solicitud y responder al cliente
static void* worker_fn(void *arg) {
    worker_arg_t *w = (worker_arg_t*)arg; // Convertimos el argumento a nuestro tipo definido
    Request req = w->req;               // Copiamos la solicitud a una variable local para trabajar con ella
    free(w);

    Response resp;                // Variable para construir la respuesta al cliente
    memset(&resp, 0, sizeof(Response)); // Inicializamos la estructura de respuesta a cero

    // Ejecutar operación (API local de la parte A)
    switch (req.op) {

        // Ejecutamos destroy() y guardamos su resultado en resp.ret
        case OP_DESTROY:
            resp.ret = destroy();
            break;

        // Ejecutamos set_value() con los parámetros recibidos en la solicitud y guardamos su resultado en resp.ret
        case OP_SET:
            resp.ret = set_value(req.key, req.value1, req.N_value2, req.V_value2, req.value3);
            break;

        /* Ejecutamos get_value() con los parámetros recibidos en la solicitud y guardamos su resultado en resp.ret
        Si el resultado es 0 -> guardamos los valores obtenidos en resp.value1, resp.N_value2, resp.V_value2 y resp.value3 para enviarlos al cliente*/
        case OP_GET: {
            int N;
            struct Paquete p;
            char v1[MAX_STR];
            float v2[MAX_V2];

            resp.ret = get_value(req.key, v1, &N, v2, &p);
            if (resp.ret == 0) {
                strncpy(resp.value1, v1, MAX_STR-1);
                resp.value1[MAX_STR-1] = '\0';
                resp.N_value2 = N;
                for (int i = 0; i < MAX_V2; i++) resp.V_value2[i] = v2[i];
                resp.value3 = p;
            }
            break;
        }

        // Ejecutamos modify_value() con los parámetros recibidos en la solicitud y guardamos su resultado en resp.ret
        case OP_MODIFY:
            resp.ret = modify_value(req.key, req.value1, req.N_value2, req.V_value2, req.value3);
            break;

        // Ejecutamos delete_key() con los parámetros recibidos en la solicitud y guardamos su resultado en resp.ret
        case OP_DELETE:
            resp.ret = delete_key(req.key);
            break;

        // Ejecutamos exist() con los parámetros recibidos en la solicitud y guardamos su resultado en resp.ret
        case OP_EXIST:
            resp.ret = exist(req.key); // 1 / 0 / -1
            break;

        // Si la operación no es ninguna de las anteriores -> guardamos -1 en resp.ret para indicar error
        default:
            resp.ret = -1;
            break;
    }

        // Abrimos la cola privada del cliente para enviar la respuesta
    mqd_t cli = mq_open(req.reply_queue, O_WRONLY);
    if (cli == (mqd_t)-1) {
        // Si no podemos abrir la cola de respuesta del cliente, no hay forma de responderle.
        // El cliente acabará interpretándolo como error de comunicación (-2) por timeout.
        perror("mq_open client reply queue");
        pthread_exit(NULL);
    }

    // Enviamos la respuesta al cliente
    if (mq_send(cli, (const char*)&resp, sizeof(resp), 0) == -1) {
        // Si falla el envío, el cliente acabará devolviendo -2 por timeout o error de recepción.
        perror("mq_send reply");
    }

    mq_close(cli);
    return NULL;
}

// Función principal del servidor
int main(void) {
    struct sigaction sa; 
    memset(&sa, 0, sizeof(sa)); // Limpiamos la estructura
    sa.sa_handler = on_sigint;  // Asignamos el manejador para SIGINT (Ctrl+C)
    sigemptyset(&sa.sa_mask);   // No bloqueamos ninguna señal adicional durante la ejecución del manejador

    sigaction(SIGINT, &sa, NULL);  // Configuramos el manejador para SIGINT (Ctrl+C) para salir limpiamente
    sigaction(SIGTERM, &sa, NULL); // Configuramos el manejador para SIGTERM para salir limpiamente

    mq_unlink(SERVER_QUEUE); // Limpiamos la cola por si ya existía de una ejecución anterior (evita errores al crearla)

    // Configuramos los atributos de la cola del servidor
    struct mq_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.mq_maxmsg  = 10;                // Límite seguro para el número de mensajes en la cola
    attr.mq_msgsize = sizeof(Request); // Tamaño máximo de cada mensaje

    // Creamos la cola del servidor para recibir solicitudes de los clientes
    g_srv_mq = mq_open(SERVER_QUEUE, O_CREAT | O_RDONLY, 0666, &attr);
    if (g_srv_mq == (mqd_t)-1) {
        perror("mq_open server");
        return 1;
    }

    printf("Servidor MQ escuchando en %s\n", SERVER_QUEUE); // Mensaje informativo de que el servidor está listo

    // Bucle principal: se ejecuta hasta que llegue una señal (Ctrl+C) y g_salir cambie a 1
    while (!g_salir) {
        Request req; // Variable para recibir la solicitud del cliente
        ssize_t n = mq_receive(g_srv_mq, (char*)&req, sizeof(req), NULL); // Recibimos una solicitud de un cliente
        
        if (n < 0) {                      // Si hubo un error al recibir -> comprobamos si fue por señal o por otro motivo
            if (errno == EINTR) continue; // Si fue interrumpido por señal -> continuamos el bucle para salir si g_salir ya es 1
            perror("mq_receive");         // Si fue otro error -> lo informamos y salimos limpiamente
            cleanup_and_exit(1);          // Limpiamos y salimos con error
        }
        if ((size_t)n != sizeof(req)) { // Si el mensaje recibido no tiene el tamaño esperado -> lo ignoramos
            continue;
        }

        // Creamos un thread para procesar esta solicitud
        worker_arg_t *w = malloc(sizeof(*w));
        if (!w) continue; // Si no se pudo asignar memoria para el thread -> ignoramos esta solicitud
        w->req = req; 

        // Creamos el thread y lo detach para que se limpie automáticamente al terminar
        pthread_t th;
        if (pthread_create(&th, NULL, worker_fn, w) == 0) { // Si se pudo crear el thread -> lo detach para que se limpie automáticamente al terminar
            pthread_detach(th);
        } else { 
            free(w);
        }
    }
    
    printf("\nCerrando servidor de forma segura...\n"); 
    cleanup_and_exit(0); 
    
    return 0;
}
