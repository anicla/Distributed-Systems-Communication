// ----- PARTE B: -----


// Necesario para usar algunas funciones POSIX
#define _POSIX_C_SOURCE 200809L 

#include "comun.h"
#include "claves.h"  // prototipos de la API + struct Paquete
#include <mqueue.h>  // colas POSIX
#include <pthread.h> // mutex
#include <errno.h>   
#include <stdio.h>   
#include <string.h>  // memset, strncpy, strlen
#include <stdlib.h>  // malloc, free
#include <unistd.h>  // getpid
#include <time.h>    // clock_gettime


// timeout para esperar respuesta del servidor (segundos)
#define REPLY_TIMEOUT_SEC 2  


// mutex para proteger la generación de nombres únicos de colas de respuesta
static pthread_mutex_t g_name_mtx = PTHREAD_MUTEX_INITIALIZER; 

// contador para generar IDs únicos para las colas de respuesta
static unsigned long g_req_id = 0; 

// validación de formato de clave: no nula, no vacía, no demasiado larga
static int validate_key(const char *key) {
    if (key == NULL) return 0;
    if (key[0] == '\0') return 0;
    if (strnlen(key, MAX_STR) > (MAX_STR - 1)) return 0;
    return 1;
}

// validación de  value1: no sean NULL y que value1 no supere la longitud máxima. La validación completa de la key (no vacía y longitud válida) se realiza en validate_key().
static int validate_strings(const char *key, const char *value1) {
    if (!key || !value1) return 0;
    if (strnlen(value1, MAX_STR) > (MAX_STR - 1)) return 0;
    return 1;
}

// generar un nombre único para la cola de respuesta del cliente. PID del proceso + contador
static int make_reply_queue_name(char out[MAX_QNAME]) {
    pid_t pid = getpid();
    unsigned long id;

    if (pthread_mutex_lock(&g_name_mtx) != 0) return 0; // si falla el mutex, es un error interno en el cliente (error de comunicaciones)
    id = ++g_req_id; // incremento del contador para generar un ID único
    pthread_mutex_unlock(&g_name_mtx); // desbloqueo el mutex

    int n = snprintf(out, MAX_QNAME, "/mq_claves_cli_%ld_%lu", (long)pid, id); // Generamos el nombre de la cola de respuesta
    
    // si el nombre generado cabe en el buffer lo damo por válido, sino error de comunicaciones
    return (n > 0 && n < MAX_QNAME); 
}

// envia una solicitud al servidor y espera la respuesta con timeout. si falla se devuleve -2
static int send_request_and_wait(const Request *req_in, Response *resp_out) {
    
    // copia local de la petición
    Request req = *req_in; 

    // atributos de la cola privada de respuesta del cliente
    struct mq_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(Response);

    mqd_t reply_mq = (mqd_t)-1;
    mqd_t srv = (mqd_t)-1;
    
    // cualquier fallo aquí se interpreta como error de comunicación
    int ret = -2; 

    // si existe una cola con el mismo nombre, la eliminamos antes de crear la nueva
    mq_unlink(req.reply_queue);

    // creamos la cola privada de respuesta del cliente
    reply_mq = mq_open(req.reply_queue, O_CREAT | O_RDONLY, 0600, &attr);
    if (reply_mq == (mqd_t)-1) {
        goto cleanup;
    }

    // abrimos la cola del servidor
    srv = mq_open(SERVER_QUEUE, O_WRONLY);
    if (srv == (mqd_t)-1) {
        goto cleanup;
    }

    // enviamos la petición al servidor
    if (mq_send(srv, (const char*)&req, sizeof(req), 0) == -1) {
        goto cleanup;
    }

    // ya no hace falta la cola del servidor
    mq_close(srv);
    srv = (mqd_t)-1;

    // calculamos el instante límite para esperar la respuesta
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        goto cleanup;
    }
    ts.tv_sec += REPLY_TIMEOUT_SEC;

    // esperamos la respuesta del servidor, con timeout
    ssize_t n = mq_timedreceive(reply_mq, (char*)resp_out, sizeof(*resp_out), NULL, &ts);
    if (n < 0) {
        goto cleanup;
    }

    // si el tamaño recibido no coincide con el esperado, lo tratamos como error de comunicación
    if ((size_t)n != sizeof(*resp_out)) {
        goto cleanup;
    }

    // comunicación correcta: la respuesta es válida
    ret = 0;

cleanup:
    // cerramos las colas que lelgaron a abrirse (si se abrieron)
    if (srv != (mqd_t)-1) {
        mq_close(srv);
    }
    if (reply_mq != (mqd_t)-1) {
        mq_close(reply_mq);
    }

    // la cola privada del cliente siempre se elimina al terminar
    mq_unlink(req.reply_queue);

    return ret;
}

// ----- implementación API pública (claves.h): -----

int destroy(void) { 

    // para destroy solo hace falta indicar la operación
    Request req; // solo empaquetamos op y esperamos ret.
    memset(&req, 0, sizeof(req)); 
    req.op = OP_DESTROY;

    if (!make_reply_queue_name(req.reply_queue)) return -2;

    Response resp; 
    int comm = send_request_and_wait(&req, &resp); 
    if (comm == -2) return -2; 

    return resp.ret; // 0 o -1
}

// validamos los parametros basicos antes de mandar la solicitud al servidor para evitar tráfico innecesario
int set_value(char *key, char *value1, int N_value2, float *V_value2, struct Paquete value3) {
    
    // validaciones sin contacto
    if (!validate_key(key)) return -1;
    if (!validate_strings(key, value1)) return -1;
    if (N_value2 < 1 || N_value2 > MAX_V2) return -1;
    if (!V_value2) return -1;

    Request req; 
    memset(&req, 0, sizeof(req)); 
    req.op = OP_SET; 

    if (!make_reply_queue_name(req.reply_queue)) return -2; // Si no se pudo generar un nombre de cola de respuesta -> error de comunicaciones

    // copiamos a buffers de tamaño fijo del protocolo (evitamos punteros crudos en mensajes)
    strncpy(req.key, key, MAX_STR - 1); // copiar la clave a la solicitud
    req.key[MAX_STR - 1] = '\0';        // asegurar el fin de la cadena

    strncpy(req.value1, value1, MAX_STR - 1); 
    req.value1[MAX_STR - 1] = '\0';          

    req.N_value2 = N_value2; 

    // inicializamos el vector completo para que el mensaje sea sin basura
    for (int i = 0; i < MAX_V2; i++) req.V_value2[i] = 0.0f; 
    for (int i = 0; i < N_value2; i++) req.V_value2[i] = V_value2[i]; 
    req.value3 = value3;

    Response resp;
    int comm = send_request_and_wait(&req, &resp); 
    if (comm == -2) return -2; 

    return resp.ret; 
}

// si la comunicación va bien pero el servidor devuelve -1 (clave no existe), propagamos -1 tal cual
int get_value(char *key, char *value1, int *N_value2, float *V_value2, struct Paquete *value3) {

    if (!key || !value1 || !N_value2 || !V_value2 || !value3) return -1; // si algún puntero es nulo entonces error del servicio
    if (!validate_key(key)) return -1; // si el formato de la clave no es válido entonces error  del servicio

    Request req;                 
    memset(&req, 0, sizeof(req)); 
    req.op = OP_GET;              

    if (!make_reply_queue_name(req.reply_queue)) return -2; 

    strncpy(req.key, key, MAX_STR - 1); // copia la clave a la solicitud
    req.key[MAX_STR - 1] = '\0';        // asegura terminación de cadena

    Response resp; 
    int comm = send_request_and_wait(&req, &resp); 
    if (comm == -2) return -2; 


    // si la operación fue exitosa, copiamos los resultados a los punteros de salida
    if (resp.ret == 0) {                           
        strncpy(value1, resp.value1, MAX_STR - 1); // copia value1 de la respuesta al puntero de salida
        value1[MAX_STR - 1] = '\0';                // asugra terminación de cadena

        *N_value2 = resp.N_value2; 
        for (int i = 0; i < MAX_V2; i++) V_value2[i] = resp.V_value2[i]; 
        *value3 = resp.value3; 
    }

    return resp.ret; // 0 o -1
}

// igual que en set_value, validamos localmente antes de contactar con el servidor porque la operación exige que la clave exista
int modify_value(char *key, char *value1, int N_value2, float *V_value2, struct Paquete value3) {
    // validaciones sin contacto

    if (!validate_key(key)) return -1;
    if (!validate_strings(key, value1)) return -1;
    if (N_value2 < 1 || N_value2 > MAX_V2) return -1;
    
    // si el puntero al array es nulo entonces error del servicio
    if (!V_value2) return -1; 
    Request req; 
    memset(&req, 0, sizeof(req)); 
    req.op = OP_MODIFY; 

    if (!make_reply_queue_name(req.reply_queue)) return -2;

    strncpy(req.key, key, MAX_STR - 1); 
    req.key[MAX_STR - 1] = '\0'; 

    strncpy(req.value1, value1, MAX_STR - 1); 
    req.value1[MAX_STR - 1] = '\0'; 

    req.N_value2 = N_value2; 
    for (int i = 0; i < MAX_V2; i++) req.V_value2[i] = 0.0f;
    for (int i = 0; i < N_value2; i++) req.V_value2[i] = V_value2[i]; 
    req.value3 = value3; 

    Response resp; 
    int comm = send_request_and_wait(&req, &resp); 
    if (comm == -2) return -2; 

    return resp.ret; // 0 / -1
}

// operación sencilla: enviamos la clave y esperamos el resultado
int delete_key(char *key) {
    // validación sin contacto

    if (!validate_key(key)) return -1; // si el formato de la clave no es válido entonces error  del servicio

    Request req; // Solicitud
    memset(&req, 0, sizeof(req)); // inicializacion a cero 
    req.op = OP_DELETE; 

    if (!make_reply_queue_name(req.reply_queue)) return -2; // si no se pudo generar un nombre de cola de respuesta entonces error de comunicaciones

    strncpy(req.key, key, MAX_STR - 1); // copiaa de la clave a la solicitud
    req.key[MAX_STR - 1] = '\0';        // asegura terminación de cadena

    Response resp;                               // variable para recibir la respuesta del servidor
    int comm = send_request_and_wait(&req, &resp); // enviar solicitud y esperar respuesta
    if (comm == -2) return -2;                     // si hubo un error de comunicaciones se devuelve -2

    return resp.ret; // 0 o -1
}

// devuelve 1 si la clave existe, 0 si no existe, -1 si hay error del servicio y -2 si falla la comunicación con el servidor
int exist(char *key) {

    if (!validate_key(key)) return -1; // Si el formato de la clave no es válido entonces error del servicio

    Request req; 
    memset(&req, 0, sizeof(req)); 
    req.op = OP_EXIST; 

    if (!make_reply_queue_name(req.reply_queue)) return -2; 

    strncpy(req.key, key, MAX_STR - 1); 
    req.key[MAX_STR - 1] = '\0'; 

    Response resp; 
    int comm = send_request_and_wait(&req, &resp); // envia la solicitud y espera la respuesta
    if (comm == -2) return -2;                     // si hubo un error de comunicaciones se devuelve -2

    return resp.ret; // 1 o 0 o -1
}
