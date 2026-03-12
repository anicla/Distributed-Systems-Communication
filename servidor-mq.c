// ----- PARTE B: -----

// Necesario para algunas funciones POSIX
#define _POSIX_C_SOURCE 200809L 

#include "comun.h"
#include "claves.h"  // API local del servicio + struct Paquete
#include <mqueue.h>  // colas POSIX
#include <pthread.h> // hilos
#include <signal.h>  // señales
#include <stdio.h>   
#include <stdlib.h>  
#include <string.h>  
#include <errno.h>   
#include <unistd.h>  // _exit



// descriptor de la cola pública del servidor; lo guardamos como global para poder cerrarlo también al salir por señal
static mqd_t g_srv_mq = (mqd_t)-1; 

// bandera global para indicar que el servidor debe terminar
volatile sig_atomic_t g_salir = 0; 

// cierra y elimina la cola del servidor antes de salir
static void cleanup_and_exit(int code) {
    if (g_srv_mq != (mqd_t)-1) mq_close(g_srv_mq);
    mq_unlink(SERVER_QUEUE);
    _exit(code);
}

// manejador de señales; NO cerramos nada aquí directamente: solo activamos la bandera de salida
static void on_sigint(int sig) {
    (void)sig;
    g_salir = 1; 
}

// estructura auxiliar que se pasa a cada hilo worker; contiene una copia de la petición recibida
typedef struct {
    Request req;
} worker_arg_t;

// ejecuta cada thread para procesar una solicitud y responder al cliente
static void* worker_fn(void *arg) {

    // recuperamos la petición y liberamos la memoria reservada para el argumento
    worker_arg_t *w = (worker_arg_t*)arg; 
    Request req = w->req;               
    free(w);

    // preparamos la respuesta; la inicializamos a cero para evitar basura
    Response resp;               
    memset(&resp, 0, sizeof(Response)); 

    const char *op_names[] = {
    "NONE","DESTROY","SET","GET","MODIFY","DELETE","EXIST"
    };

    printf("Servidor: hilo %lu -> operacion %s, key='%s'\n",
        pthread_self(), op_names[req.op], req.key);

    // ejecucion de la operación (API local de la parte A)
    switch (req.op) {

        // ejecutamos destroy() y guardamos su resultado en resp.ret
        case OP_DESTROY:
            resp.ret = destroy();
            break;

        // ejecutamos set_value() con los parámetros recibidos en la solicitud y guardamos su resultado en resp.ret
        case OP_SET:
            resp.ret = set_value(req.key, req.value1, req.N_value2, req.V_value2, req.value3);
            break;

        case OP_GET: {

            // en get_value primero recuperamos los datos en variables auxiliares y luego los copiamos a la estructura de respuesta
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

        // ejecutamos modify_value() con los parámetros recibidos en la solicitud y guardamos su resultado en resp.ret
        case OP_MODIFY:
            resp.ret = modify_value(req.key, req.value1, req.N_value2, req.V_value2, req.value3);
            break;

        // ejecutamos delete_key() con los parámetros recibidos en la solicitud y guardamos su resultado en resp.ret
        case OP_DELETE:
            resp.ret = delete_key(req.key);
            break;

        // ejecutamos exist() con los parámetros recibidos en la solicitud y guardamos su resultado en resp.ret
        case OP_EXIST:
            resp.ret = exist(req.key); // 1 / 0 / -1
            break;

        // si llega una operación desconocida, devolvemos error
        default:
            resp.ret = -1;
            break;
    }

        // abrimos la cola privada del cliente para enviar la respuesta
    mqd_t cli = mq_open(req.reply_queue, O_WRONLY);
    if (cli == (mqd_t)-1) {
        // si no podemos abrir la cola de respuesta del cliente, no hay forma de responderle. El cliente acabará interpretándolo como error de comunicación (-2) por timeout.
        perror("mq_open client reply queue");
        pthread_exit(NULL);
    }

    // enviamos la respuesta al cliente
    if (mq_send(cli, (const char*)&resp, sizeof(resp), 0) == -1) {
        // si falla el envío, el cliente acabará devolviendo -2 por timeout o error de recepción.
        perror("mq_send reply");
    }

    mq_close(cli);
    return NULL;
}

// función principal del servidor
int main(void) {
    struct sigaction sa; 
    memset(&sa, 0, sizeof(sa)); // limpieza de la estructura
    sa.sa_handler = on_sigint;  // asignación del manejador (Ctrl+C)
    sigemptyset(&sa.sa_mask);   // NO se bloquean señales adicionales durante la ejecución del manejador

    sigaction(SIGINT, &sa, NULL);  // Ctrl+C
    sigaction(SIGTERM, &sa, NULL); // terminaicón externa

    // limpiamos la cola por si ya existía de una ejecución anterior
    mq_unlink(SERVER_QUEUE); 

    // configuramos los atributos de la cola del servidor
    struct mq_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.mq_maxmsg  = 10;                // máximo de mensajer pendientes
    attr.mq_msgsize = sizeof(Request); // tamaño máximo de cada petición

    // creamos la cola donde el servidor recibirá las solicitudes de los clientes
    g_srv_mq = mq_open(SERVER_QUEUE, O_CREAT | O_RDONLY, 0666, &attr);
    if (g_srv_mq == (mqd_t)-1) {
        perror("mq_open server");
        return 1;
    }

    printf("Servidor MQ escuchando en %s\n", SERVER_QUEUE); // Mensaje informativo de que el servidor está listo

    // Bucle principal: recibe peticiones y crea un hilo nuevo para cada una
    while (!g_salir) {
        Request req; // variable para recibir la solicitud del cliente
        ssize_t n = mq_receive(g_srv_mq, (char*)&req, sizeof(req), NULL); 
        
        // si hay error al recibir, comprobamos si ha sido por una señal
        if (n < 0) {                      
            if (errno == EINTR) continue; // se interrumpió por señal; volvemos al bucle
            perror("mq_receive");         // si fue otro error 
            cleanup_and_exit(1);          
        }

        // si el tamaño del mensaje no es el esperado, lo ignoramos
        if ((size_t)n != sizeof(req)) { 
        }

        // reservamos memoria para pasar la petición al hilo 
        worker_arg_t *w = malloc(sizeof(*w));
        if (!w) continue; // si no se pudo asignar memoria para el thread entonces ignoramos esta solicitud
        w->req = req; 

        // creamos un hilo para procesar esta petición
        pthread_t th;
        if (pthread_create(&th, NULL, worker_fn, w) == 0) { // Si se pudo crear el thread -> lo detach para que se limpie automáticamente al terminar
            // el hilo se marca como detached para que libere automáticamente sus recursos al terminar
            pthread_detach(th);
        } else { 
            // si no se pudo crear el hilo, liberamos la memoria reservada
            free(w);
        }
    }
    
    printf("\nCerrando servidor de forma segura...\n"); 
    cleanup_and_exit(0); 
    
    return 0;
}
