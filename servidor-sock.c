#define _POSIX_C_SOURCE 200112L

#include "claves.h" 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#define BACKLOG 64 //nº máximo de conexiones pendientes en la cola del socket
#define MAX_LINE 512 //máximo para líneas de texto en el protocolo
#define MAX_STR 256 //tamaño de strings (255 chars útiles + '\0')
#define MAX_V2 32 //nº de elementos en V_value2

//códigos de operación del protocolo
#define OP_DESTROY     1
#define OP_SET_VALUE   2
#define OP_GET_VALUE   3
#define OP_MODIFY      4
#define OP_DELETE      5
#define OP_EXIST       6

//funciones auxiliares de E/S robusta

//escribe exactamente "count" (número de bytes que se queiren escirbir) en el descriptor del fichero (fd) desde el buffer (buf)
static int write_all(int fd, const void *buf, size_t count) {
    const char *p = (const char *)buf; //puntero para recorrer el buffer
    size_t total = 0; //bytes escritos hasta ahora

    while (total < count) { //seguir escribiendo hasta que se haya escrito todo
        ssize_t n = write(fd, p + total, count - total); // intentar escribir el resto del buffer
        if (n < 0) { //ver si el error es por interrupción (EINTR) o un error real
            if (errno == EINTR) continue; //interrupción: intentar escribir de nuevo
            return -1; //error real: -1
        }
        if (n == 0) return -1; //write = 0: no se ha podido avanzar en la escritura (error)
        total += (size_t)n; //actualización del total de bytes escritos
    }
    return 0; 
}

//lee  el "count" del (número de bytes que se queiren escirbir) en el descriptor del fichero (fd) en buffer (buf)
static int read_all(int fd, void *buf, size_t count) {
    char *p = (char *)buf; //puntero para recorrer el buffer
    size_t total = 0; //total de bytes leídos 

    while (total < count) { //seguir leyendo hasta que se haya leido todo
        ssize_t n = read(fd, p + total, count - total); //intentar leer el resto del buffer
        if (n < 0) { //ver si el error es por interrupción (EINTR) o un error real
            if (errno == EINTR) continue; //interrupción: intentar leer de nuevo
            return -1; //error real: -1
        }
        if (n == 0) return -1; //read = 0: el otro extremo ha cerrado la conexión antes de completar la lectura
        total += (size_t)n; //actualización el total de bytes leídos
    }
    return 0; 
}

//envía una línea de texto (que acabo con '\n')
static int send_line(int fd, const char *line) {
    if (write_all(fd, line, strlen(line)) < 0) return -1; //error: al escribir la linea
    if (write_all(fd, "\n", 1) < 0) return -1;  //error: al escribir el carácter de nueva línea 
    return 0; 
}

//envía un entero como línea con formato de número entero
static int send_int(int fd, int value) {
    char buf[MAX_LINE]; //buffer para convertir el entero a texto
    snprintf(buf, sizeof(buf), "%d", value); //conversion el entero a texto y almacenarlo en el buf
    return send_line(fd, buf); //envia la línea con el entero convertido
}

//envía un float como línea con formato de punto flotante
static int send_float(int fd, float value) {
    char buf[MAX_LINE];  //buffer para convertir el float a texto
    snprintf(buf, sizeof(buf), "%.9g", value); // converesion el float a texto con formato de punto flotante y lo almacenamos en buf
    return send_line(fd, buf); // envia la línea con el float convertido
}

//recibe una línea de texto (acabada con'\n') y la almacena en el buffer (respetando maxlen)
static int recv_line(int fd, char *buffer, size_t maxlen) {
    size_t i = 0; //indice para almacenar caracteres en buffer
    char c; //variable para leer caracteres uno a uno

    if (maxlen == 0) return -1; // error: maxlen = 0 (no hay espacio para almacenar nada)

    while (i < maxlen - 1) { //leer caracteres mientras no hayamos llenado el buffer 
        ssize_t n = read(fd, &c, 1); // leer un carácter del descriptor fd
        if (n < 0) { //ver si el error es por interrupción (EINTR) o un error real
            if (errno == EINTR) continue; // interrupción: intentar leer de nuevo
            return -1; //error real:-1
        }
        if (n == 0) return -1; //read = 0: el otro extremo ha cerrado la conexión antes de completar la lectura

        if (c == '\n') { //hemos terminado de leer la línea porque el caracter leido es '\n'
            buffer[i] = '\0'; //termina el string con '\0'
            return 0; //linea recibida correctamente
        }

        buffer[i++] = c; //almacenamiento del carácter leído en buffer y avanzamos el índice
    }

    buffer[i] = '\0'; //terminar el string con '\0' aunque no hayamos leído un '\n' pq la línea es demasiado larga
    return -1; //error: (línea es demasiado larga para el buffer o no apareció '\n' a tiempo
}

//recibe un entero enviado como línea
static int recv_int(int fd, int *value) {
    char buf[MAX_LINE]; //buffer para almacenar la línea recibida
    char *endptr = NULL; // puntero para verificar que toda la línea se convirtió correctamente a entero
    long v; // variable para almacenar el valor convertido a entero

    if (recv_line(fd, buf, sizeof(buf)) < 0) return -1; // error: falla al recibir la linea

    v = strtol(buf, &endptr, 10); //converesion de la línea a entero usando strtol con base 10
    if (*buf == '\0' || *endptr != '\0') return -1; //error: la línea está vacía o no se convirtió completamente a entero 

    *value = (int)v; //almacenamiento del valor convertido en la variable apuntada por "value"
    return 0; 
}

//recibe un float enviado como línea
static int recv_float(int fd, float *value) {
    char buf[MAX_LINE];  //buffer para almacenar la línea recibida
    char *endptr = NULL; //puntero para verificar que toda la línea se convirtió correctamente a float
    float v; //variable para almacenar el valor convertido a float

    if (recv_line(fd, buf, sizeof(buf)) < 0) return -1; //error: fallo al recibir la línea 

    v = strtof(buf, &endptr); // la línea a float usando strtof
    if (*buf == '\0' || *endptr != '\0') return -1; //error: la línea está vacía o no se convirtió completamente a float 

    *value = v; //almacenamiento del valor convertido en la variable apuntada por "value"
    return 0; 
}

//envía un string con el formato: primero la longitud (como entero), luego el string sin '\0', y finalmente un '\n' para separar del siguiente campo
static int send_string(int fd, const char *str) {
    int len;  //variable para almacenar la longitud del string

    if (str == NULL) return -1; //error: string es NULL 

    len = (int)strlen(str); //calcula la longitud del string (sin contar el '\0')
    if (send_int(fd, len) < 0) return -1; //error: fallo al enviar la longitud 
    if (len > 0 && write_all(fd, str, (size_t)len) < 0) return -1; //error: falla al enviar el string 
    if (write_all(fd, "\n", 1) < 0) return -1; //error: al enviar el carácter de nueva línea 

    return 0; 
}

//recibe un string con el formato definido en send_string y lo almacena en el buffer (respetando maxlen)
static int recv_string(int fd, char *buffer, size_t maxlen) {
    int len; //variable para almacenar la longitud del string que se va a recibir
    char newline; //debe contener el '\n' que separa el bloque de datos del siguiente campo

    if (recv_int(fd, &len) < 0) return -1; //error: al recibir la longitud 
    if (len < 0) return -1; //error: longitud es negativa 
    if ((size_t)len >= maxlen) return -1; //error: la longitud excede la capacidad del buffer 

    if (len > 0) { // leer el string siempre que hayaalgo que leer 
        if (read_all(fd, buffer, (size_t)len) < 0) return -1; //error: falla al leer el string 
    }
    buffer[len] = '\0'; // termina el string con '\0'

    if (read_all(fd, &newline, 1) < 0) return -1; //error: al leer el carácter de nueva línea
    if (newline != '\n') return -1; //error: el carácter no es de nueva línea 

    return 0; 
}

//GESTION DE UNA PETICION

//imprimir en el servidor el tipo de petición recibida según el código de operación op
static void log_peticion(int op) {
    switch (op) {
        case OP_DESTROY:
            printf("[SERVIDOR] Petición recibida: destroy\n");
            break;
        case OP_SET_VALUE:
            printf("[SERVIDOR] Petición recibida: set_value\n");
            break;
        case OP_GET_VALUE:
            printf("[SERVIDOR] Petición recibida: get_value\n");
            break;
        case OP_MODIFY:
            printf("[SERVIDOR] Petición recibida: modify_value\n");
            break;
        case OP_DELETE:
            printf("[SERVIDOR] Petición recibida: delete_key\n");
            break;
        case OP_EXIST:
            printf("[SERVIDOR] Petición recibida: exist\n");
            break;
        default:
            printf("[SERVIDOR] Petición recibida: operación desconocida (%d)\n", op);
            break;
    }

    fflush(stdout);
}

//FUNCIONES PARA CADA TIPO DE PETICION

//petición: destroy
static int atender_destroy(int fd) {
    int r = destroy(); // Llamamos a destroy() para destruir la estructura de datos y obtener el resultado
    return send_int(fd, r); // Enviamos el resultado al cliente
}

//petición: set_value
static int atender_set(int fd) {
    char key[MAX_STR]; // buffer clave
    char value1[MAX_STR]; // buffer valor1
    int N_value2, i, r; // variable para el número de elementos de V_value2, un índice para iterar, y el resultado de la operación
    float V_value2[MAX_V2]; // Array para almacenar los elementos de V_value2
    struct Paquete p; // variable para almacenar el valor3 (struct Paquete)

    if (recv_string(fd, key, sizeof(key)) < 0) return -1; //error: falla al recibir la clave 
    if (recv_string(fd, value1, sizeof(value1)) < 0) return -1; //error: falla al recibir el valor1 
    if (recv_int(fd, &N_value2) < 0) return -1; //error: falla al recibir el número de elementos de V_value2 

    if (N_value2 < 1 || N_value2 > MAX_V2) { //error: número de elementos de V_value2 no es válido 
        return send_int(fd, -1); //-1:indicar error al cliente
    }

    for (i = 0; i < N_value2; i++) { //iteracion para recibir cada elemento de V_value2
        if (recv_float(fd, &V_value2[i]) < 0) return -1; //error: falla al recibir algún elemento de V_value2 
    }

    if (recv_int(fd, &p.x) < 0) return -1; //error: falla al recibir el valor3.x 
    if (recv_int(fd, &p.y) < 0) return -1; //error: falla al recibir el valor3.y 
    if (recv_int(fd, &p.z) < 0) return -1; //error: falla al recibir el valor3.z 

    r = set_value(key, value1, N_value2, V_value2, p); //set_value() con los datos recibidos 
    return send_int(fd, r); // resultado para el cliente
}

//petición: get_value
static int atender_get(int fd) {
    char key[MAX_STR]; //buffer para la clave
    char value1[MAX_STR]; //buffer para almacenar el valor1 que se va a recibir de get_value()
    int N_value2, i, r; //variable para almacenar el número de elementos de V_value2 que se va a recibir, un índice para iterar, y el resultado de la operación
    float V_value2[MAX_V2]; //array para almacenar los elementos de V_value2 que se van a recibir
    struct Paquete p; //variable para almacenar el valor3 que se va a recibir

    if (recv_string(fd, key, sizeof(key)) < 0) return -1; //error: falla al recibir la clave

    r = get_value(key, value1, &N_value2, V_value2, &p); //get_value() con la clave recibida y almacenamos el resultado en r, y los datos recibidos en value1, N_value2, V_value2, y p

    if (send_int(fd, r) < 0) return -1; //error: falla al enviar el resultado 
    if (r != 0) return 0; //no hay dato que enviar si el resultado  es distinto de 0

    if (send_string(fd, value1) < 0) return -1; //error: falla al enviar value1 
    if (send_int(fd, N_value2) < 0) return -1; //error: falla al enviar N_value2 

    for (i = 0; i < N_value2; i++) { //iteracion para enviar cada elemento de V_value2
        if (send_float(fd, V_value2[i]) < 0) return -1; //error: falla al enviar algún elemento de V_value2 
    }

    if (send_int(fd, p.x) < 0) return -1; //error: falla al enviar p.x 
    if (send_int(fd, p.y) < 0) return -1; //error: falla al enviar p.y
    if (send_int(fd, p.z) < 0) return -1; //error: falla al enviar p.z 

    return 0; 
}

//petición: modify_value
static int atender_modify(int fd) {
    char key[MAX_STR]; //buffer para la clave
    char value1[MAX_STR]; //buffer para almacenar el valor1 que se va a recibir de modify_value()
    int N_value2, i, r; //variable para almacenar el número de elementos de V_value2 que se va a recibir, un índice para iterar, y el resultado de la operación
    float V_value2[MAX_V2]; //array para almacenar los elementos de V_value2 que se van a recibir
    struct Paquete p; //variable para almacenar el valor3 que se va a recibir

    if (recv_string(fd, key, sizeof(key)) < 0) return -1; //error: falla al recibir la clave 
    if (recv_string(fd, value1, sizeof(value1)) < 0) return -1; //errr:falla al recibir value1 
    if (recv_int(fd, &N_value2) < 0) return -1; //error: falla al recibir N_value2 

    if (N_value2 < 1 || N_value2 > MAX_V2) { //error: nº de elementos de V_value2 no es válido 
        return send_int(fd, -1); //-1: indicar error al cliente
    }

    for (i = 0; i < N_value2; i++) { //iteracion para recibir cada elemento de V_value2
        if (recv_float(fd, &V_value2[i]) < 0) return -1; //error: falla al recibir algún elemento de V_value2
    }

    if (recv_int(fd, &p.x) < 0) return -1; //error: falla al recibir p.x 
    if (recv_int(fd, &p.y) < 0) return -1; //error: falla al recibir p.y 
    if (recv_int(fd, &p.z) < 0) return -1; //error: falla al recibir p.z 

    r = modify_value(key, value1, N_value2, V_value2, p); //modify_value() con los datos recibidos y almacenamos el resultado en r
    return send_int(fd, r); //resultado al cliente
}

//petición: delete
static int atender_delete(int fd) {
    char key[MAX_STR]; //buffer para la clave
    int r; //variable para almacenar el resultado de la operación

    if (recv_string(fd, key, sizeof(key)) < 0) return -1; //error: falla al recibir la clave 
    r = delete_key(key); //delete_key() con la clave recibida y almacenamos el resultado en r
    return send_int(fd, r); //resultado al cliente
}

//petición: exist
static int atender_exist(int fd) {
    char key[MAX_STR]; //buffer para la clave
    int r; //variable para almacenar el resultado de la operación

    if (recv_string(fd, key, sizeof(key)) < 0) return -1; //error: falla al recibir la clave 
    r = exist(key); //exist() con la clave recibida y almacenamos el resultado en r
    return send_int(fd, r); // resultado al cliente
}

//hilo por cliente
static void *thread_cliente(void *arg) {
    int fd = *((int *)arg); //obtenemos el descriptor de la conexión con el cliente a partir del argumento
    int op; //variable para almacenar el código de operación recibido del cliente

    free(arg); //liberar la memoria del argumento ya que no la necesitamos más

    if (recv_int(fd, &op) < 0) { //error: falla al recibir el código de operación 
        close(fd); //cerrar la conexión con el cliente
        return NULL; 
    }

    log_peticion(op); //imprime el tipo de petición recibida en el servidor
    printf("[SERVIDOR] Hilo %lu atendiendo operacion %d\n",
       (unsigned long)pthread_self(), op); //id del hilo que atiende la petición y el código de operación
    fflush(stdout); //forzamos la impresión inmediata en pantalla

    switch (op) { //cumplir petición según el código de operación recibido
        case OP_DESTROY:
            if (atender_destroy(fd) < 0) { //en caso de que falle al atender destroy se cierra la conexión y termina el hilo
                close(fd);
                return NULL;
            }
            break;
        case OP_SET_VALUE:
            if (atender_set(fd) < 0) { //en casod e que falle al atender set_value se cierra la conexión y terminamos el hilo
                close(fd);
                return NULL;
            }
            break;
        case OP_GET_VALUE:
            if (atender_get(fd) < 0) { //en caso de que falla al atender get_value se cierra la conexión y terminamos el hilo
                close(fd);
                return NULL;
            }
            break;
        case OP_MODIFY:
            if (atender_modify(fd) < 0) { //en caso de que falle al atender modify_value se cierra la conexión y terminamos el hilo
                close(fd);
                return NULL;
            }
            break;
        case OP_DELETE:
            if (atender_delete(fd) < 0) { //en caso de que falla al atender delete_key se cierra la conexión y terminamos el hilo
                close(fd);
                return NULL;
            }
            break;
        case OP_EXIST:
            if (atender_exist(fd) < 0) { //en caso de que falla al atender exist se cierra la conexión y terminamos el hilo
                close(fd);
                return NULL;
            }
            break;
        default:
            send_int(fd, -1); // error al cliente: la operación no existe 
            break;
    }

    close(fd); // cierra de conexión con el cliente
    return NULL;
}

// MAIN
int main(int argc, char *argv[]) {
    struct addrinfo hints, *res, *rp; //variables para configurar el socket y almacenar las direcciones
    int listen_fd = -1; //variable para el descriptor del socket de escucha
    int yes = 1; //variable para la opción SO_REUSEADDR

    if (argc != 2) { // error: no se especifica el puerto como argumento 
        fprintf(stderr, "Uso: %s <PUERTO>\n", argv[0]); //imprimir el mensaje de uso en stderr
        return 1; //1: para indicar error
    }

    memset(&hints, 0, sizeof(hints)); // limpeiza de la estructura hints con ceros
    hints.ai_family = AF_UNSPEC;      // IPv4 o IPv6
    hints.ai_socktype = SOCK_STREAM;  // TCP
    hints.ai_flags = AI_PASSIVE;     

    int ret = getaddrinfo(NULL, argv[1], &hints, &res); //saca las direcciones del servidor mediante getaddrinfo con la IP y puerto especificados
    if (ret != 0) { //error de getaddrinfo:lo imprimimos en stderr y devolvemos -1 para indicar error
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        return 1; // -1:indicar error
    }

    for (rp = res; rp != NULL; rp = rp->ai_next) { //iteracion sobre las direcciones devueltas por getaddrinfo
        listen_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol); //crear un socket con la dirección actual
        if (listen_fd < 0) continue; //probar otra direccion si falla al crear el socket 


        if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) { //opción SO_REUSEADDR para permitir reutilizar el puerto rápidamente después de cerrar el servidor
            perror("setsockopt");
            close(listen_fd);
            listen_fd = -1;
            continue;
        }

        if (bind(listen_fd, rp->ai_addr, rp->ai_addrlen) == 0) { //se ha encontrado una dircc valida para el socket de escucha si bind es exitoso 
            break; 
        }

        close(listen_fd); //fallo al bindear el socket: cerrarlo e intentar con otra dircc 
        listen_fd = -1; // se reinicia listen_fd para indicar que no tenemos un socket válido aún
    }

    freeaddrinfo(res); //libreracion en memoria de las direcciones obtenidas por getaddrinfo

    if (listen_fd < 0) { //error: no hemos podido crear un socket de escucha válido 
        perror("bind/socket"); 
        return 1; //1: indicar error
    }

    if (listen(listen_fd, BACKLOG) < 0) { // listen = error: lo imprimimos y cerramos el socket
        perror("listen"); 
        close(listen_fd); //cerramos el socket 
        return 1; //1: indicar error
    }

    printf("[SERVIDOR] Escuchando en el puerto %s...\n", argv[1]); //mensaje: indicar que el servidor está escuchando en el puerto especificado
    fflush(stdout); // el mensaje se debe imprimir antes de aceptar conexiones

    while (1) { //bucle para aceptar conexiones de clientes
        int client_fd; // variable para el descriptor de la conexión con el cliente
        struct sockaddr_storage client_addr; // variable para almacenar la dirección del cliente
        socklen_t client_len = sizeof(client_addr); //variable para almacenar la longitud de la dirección del cliente
        pthread_t th; //variable para el identificador del hilo que atenderá al cliente
        int *pclient; //variable para almacenar el descriptor del cliente que se pasará al hilo

        client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len); //aceptamos una conexión entrante de un cliente y obtenemos el descriptor de la conexión y la dirección del cliente
        if (client_fd < 0) { //accept devuelve un error: imprimimos y continuamos
            if (errno == EINTR) continue; //error por interrupción: intentamos aceptar de nuevo
            perror("accept");
            continue; // continuamos para aceptar la siguiente conexión entrante
        }

        pclient = (int *)malloc(sizeof(int)); // reserva de memoria para almacenar el descriptor del cliente que se pasará al hilo
        if (pclient == NULL) { //falla la reserva de memoria: lo imprimimos, cerramos la conexión con el cliente, y continuamos
            perror("malloc"); 
            close(client_fd); //cerrar la conexión con el cliente
            continue; //continua para aceptar la siguiente conexión entrante
        }

        *pclient = client_fd; //almacenamos el descriptor del cliente en la variable que se pasará al hilo

        if (pthread_create(&th, NULL, thread_cliente, pclient) != 0) { //fallo al crear el hilo: lo imprimimos, cerramos la conexión con el cliente, liberamos la memoria, y continuamos
            perror("pthread_create"); 
            close(client_fd); //cerrar la conexión con el cliente
            free(pclient); //liberacion de la memoria reservada para el descriptor del cliente
            continue; //continuar para aceptar la siguiente conexión entrante
        }

        pthread_detach(th); //desacoplamiento del hilo para no tener que hacer join()
    }

    close(listen_fd); //cierre del socket de escucha
    return 0; //devolver 0 para indicar que el servidor terminó correctamente
}