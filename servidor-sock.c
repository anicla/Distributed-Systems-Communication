#define _POSIX_C_SOURCE 200112L

#include "claves.h" // Prototipos de la API y struct Paquete

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

#define BACKLOG 64 // Número máximo de conexiones pendientes en la cola del socket
#define MAX_LINE 512 // Tamaño máximo para líneas de texto en el protocolo
#define MAX_STR 256 // Máximo tamaño de strings (255 chars útiles + '\0')
#define MAX_V2 32 // Máximo número de elementos en V_value2

// Códigos de operación del protocolo
#define OP_DESTROY     1
#define OP_SET_VALUE   2
#define OP_GET_VALUE   3
#define OP_MODIFY      4
#define OP_DELETE      5
#define OP_EXIST       6

// Funciones auxiliares de E/S robusta

// Escribe exactamente "count" bytes en el descriptor fd desde buf
static int write_all(int fd, const void *buf, size_t count) {
    const char *p = (const char *)buf; // Puntero para recorrer el buffer
    size_t total = 0; // Total de bytes escritos hasta ahora

    while (total < count) { // Mientras no hayamos escrito todo -> escribir
        ssize_t n = write(fd, p + total, count - total); // Intentamos escribir el resto del buffer
        if (n < 0) { // Si hay error -> verificamos si fue por interrupción (EINTR) o es un error real
            if (errno == EINTR) continue; // Si fue por interrupción -> intentamos escribir de nuevo
            return -1; // Si es un error real -> devolvemos -1
        }
        if (n == 0) return -1; // Si write devuelve 0 -> no se ha podido avanzar en la escritura y lo tratamos como error
        total += (size_t)n; // Actualizamos el total de bytes escritos
    }
    return 0; // Todo escrito correctamente
}

// Lee exactamente "count" bytes del descriptor fd en buf
static int read_all(int fd, void *buf, size_t count) {
    char *p = (char *)buf; // Puntero para recorrer el buffer
    size_t total = 0; // Total de bytes leídos hasta ahora

    while (total < count) { // Mientras no hayamos leído todo -> leer
        ssize_t n = read(fd, p + total, count - total); // Intentamos leer el resto del buffer
        if (n < 0) { // Si hay error -> verificamos si fue por interrupción (EINTR) o es un error real
            if (errno == EINTR) continue; // Si fue por interrupción -> intentamos leer de nuevo
            return -1; // Si es un error real -> devolvemos -1
        }
        if (n == 0) return -1; // Si read devuelve 0 -> no se ha podido avanzar en la lectura y lo tratamos como error
        total += (size_t)n; // Actualizamos el total de bytes leídos
    }
    return 0; // Todo leído correctamente
}

// Envía una línea de texto (terminada en '\n')
static int send_line(int fd, const char *line) {
    if (write_all(fd, line, strlen(line)) < 0) return -1; // Si falla al escribir la línea -> error
    if (write_all(fd, "\n", 1) < 0) return -1; // Si falla al escribir el carácter de nueva línea -> error
    return 0; // Línea enviada correctamente
}

// Envía un entero como línea con formato de número entero
static int send_int(int fd, int value) {
    char buf[MAX_LINE]; // Buffer para convertir el entero a texto
    snprintf(buf, sizeof(buf), "%d", value); // Convertimos el entero a texto y lo almacenamos en buf
    return send_line(fd, buf); // Enviamos la línea con el entero convertido
}

// Envía un float como línea con formato de punto flotante
static int send_float(int fd, float value) {
    char buf[MAX_LINE]; // Buffer para convertir el float a texto
    snprintf(buf, sizeof(buf), "%.9g", value); // Convertimos el float a texto con formato de punto flotante y lo almacenamos en buf
    return send_line(fd, buf); // Enviamos la línea con el float convertido
}

// Lee del descriptor fd hasta encontrar un '\n' o llenar el buffer (dejando espacio para '\0'), y almacena la línea leída en buffer (sin incluir el '\n')
static int recv_line(int fd, char *buffer, size_t maxlen) {
    size_t i = 0; // Índice para almacenar caracteres en buffer
    char c; // Variable para almacenar el carácter leído

    if (maxlen == 0) return -1; // Si maxlen es 0 -> error (no hay espacio para almacenar nada)

    while (i < maxlen - 1) { // Mientras no hayamos llenado el buffer (dejando espacio para '\0') -> leer caracteres
        ssize_t n = read(fd, &c, 1); // Leemos un carácter del descriptor fd
        if (n < 0) { // Si hay error -> verificamos si fue por interrupción (EINTR) o es un error real
            if (errno == EINTR) continue; // Si fue por interrupción -> intentamos leer de nuevo
            return -1; // Si es un error real -> devolvemos -1
        }
        if (n == 0) return -1; // Si read devuelve 0 -> el otro extremo ha cerrado la conexión antes de completar la lectura

        if (c == '\n') { // Si el carácter leído es '\n' -> hemos terminado de leer la línea
            buffer[i] = '\0'; // Terminamos el string con '\0' (sin incluir el '\n')
            return 0; // Línea recibida correctamente
        }

        buffer[i++] = c; // Almacenamos el carácter leído en buffer y avanzamos el índice
    }

    buffer[i] = '\0'; // Si no se ha encontrado '\n' antes de llenar el buffer -> terminamos igualmente el string con '\0'
    return -1; // Si llegamos aquí -> no se ha encontrado '\n' antes de llenar el buffer -> error
}

// Recibe un entero enviado como línea
static int recv_int(int fd, int *value) {
    char buf[MAX_LINE]; // Buffer para almacenar la línea recibida
    char *endptr = NULL; // Puntero para verificar que toda la línea se convirtió correctamente a entero
    long v; // Variable para almacenar el valor convertido a entero

    if (recv_line(fd, buf, sizeof(buf)) < 0) return -1; // Si falla al recibir la línea -> error

    v = strtol(buf, &endptr, 10); // Convertimos la línea a entero usando strtol con base 10
    if (*buf == '\0' || *endptr != '\0') return -1; // Si la línea está vacía o no se convirtió completamente a entero -> error

    *value = (int)v; // Almacenamos el valor convertido en la variable apuntada por value
    return 0; // Entero recibido correctamente
}

// Recibe un float enviado como línea
static int recv_float(int fd, float *value) {
    char buf[MAX_LINE]; // Buffer para almacenar la línea recibida
    char *endptr = NULL; // Puntero para verificar que toda la línea se convirtió correctamente a float
    float v; // Variable para almacenar el valor convertido a float

    if (recv_line(fd, buf, sizeof(buf)) < 0) return -1; // Si falla al recibir la línea -> error

    v = strtof(buf, &endptr); // Convertimos la línea a float usando strtof
    if (*buf == '\0' || *endptr != '\0') return -1; // Si la línea está vacía o no se convirtió completamente a float -> error

    *value = v; // Almacenamos el valor convertido en la variable apuntada por value
    return 0; // Float recibido correctamente
}

// Envía un string con el formato: primero la longitud (como entero), luego el string sin '\0', y finalmente un '\n' para separar del siguiente campo
static int send_string(int fd, const char *str) {
    int len; // Variable para almacenar la longitud del string 

    if (str == NULL) return -1; // Si el string es NULL -> error

    len = (int)strlen(str); // Calculamos la longitud del string (sin contar el '\0')
    if (send_int(fd, len) < 0) return -1; // Si falla al enviar la longitud -> error
    if (len > 0 && write_all(fd, str, (size_t)len) < 0) return -1; // Si falla al enviar el string -> error
    if (write_all(fd, "\n", 1) < 0) return -1; // Si falla al enviar el carácter de nueva línea -> error

    return 0; // String enviado correctamente
}

// Recibe un string con el formato definido en send_string() y lo almacena en buffer (de tamaño maxlen)
static int recv_string(int fd, char *buffer, size_t maxlen) {
    int len; // Variable para almacenar la longitud del string que se va a recibir
    char newline; // Debe contener el '\n' que separa el bloque de datos del siguiente campo

    if (recv_int(fd, &len) < 0) return -1; // Si falla al recibir la longitud -> error
    if (len < 0) return -1; // Si la longitud es negativa -> error
    if ((size_t)len >= maxlen) return -1; // Si el string no cabe en el buffer dejando espacio para '\0' -> error

    if (len > 0) { // Si hay algo que leer -> leemos el string
        if (read_all(fd, buffer, (size_t)len) < 0) return -1; // Si falla al leer el string -> error
    }
    buffer[len] = '\0'; // Terminamos el string con '\0'

    if (read_all(fd, &newline, 1) < 0) return -1; // Si falla al leer el carácter de nueva línea -> error
    if (newline != '\n') return -1; // Si el carácter leído no es '\n' -> error

    return 0; // String recibido correctamente
}

// Gestión de una petición

// Imprime en el servidor el tipo de petición recibida según el código de operación op
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

// Funciones para atender cada tipo de petición

// Petición de tipo destroy
static int atender_destroy(int fd) {
    int r = destroy(); // Llamamos a destroy() para destruir la estructura de datos y obtener el resultado
    return send_int(fd, r); // Enviamos el resultado al cliente
}

// Petición de tipo set_value
static int atender_set(int fd) {
    char key[MAX_STR]; // Buffer para la clave
    char value1[MAX_STR]; // Buffer para el valor1
    int N_value2, i, r; // Variable para el número de elementos de V_value2, un índice para iterar, y el resultado de la operación
    float V_value2[MAX_V2]; // Array para almacenar los elementos de V_value2
    struct Paquete p; // Variable para almacenar el valor3 (struct Paquete)

    if (recv_string(fd, key, sizeof(key)) < 0) return -1; // Si falla al recibir la clave -> error
    if (recv_string(fd, value1, sizeof(value1)) < 0) return -1; // Si falla al recibir el valor1 -> error
    if (recv_int(fd, &N_value2) < 0) return -1; // Si falla al recibir el número de elementos de V_value2 -> error

    if (N_value2 < 1 || N_value2 > MAX_V2) { // Si el número de elementos de V_value2 no es válido -> error
        return send_int(fd, -1); // Enviamos -1 para indicar error al cliente
    }

    for (i = 0; i < N_value2; i++) { // Iteramos para recibir cada elemento de V_value2
        if (recv_float(fd, &V_value2[i]) < 0) return -1; // Si falla al recibir algún elemento de V_value2 -> error
    }

    if (recv_int(fd, &p.x) < 0) return -1; // Si falla al recibir el valor3.x -> error
    if (recv_int(fd, &p.y) < 0) return -1; // Si falla al recibir el valor3.y -> error
    if (recv_int(fd, &p.z) < 0) return -1; // Si falla al recibir el valor3.z -> error

    r = set_value(key, value1, N_value2, V_value2, p); // set_value() con los datos recibidos y almacenamos el resultado en r
    return send_int(fd, r); // Enviamos el resultado al cliente
}

// Petición de tipo get_value
static int atender_get(int fd) {
    char key[MAX_STR]; // Buffer para la clave
    char value1[MAX_STR]; // Buffer para almacenar el valor1 que se va a recibir de get_value()
    int N_value2, i, r; // Variable para almacenar el número de elementos de V_value2 que se va a recibir, un índice para iterar, y el resultado de la operación
    float V_value2[MAX_V2]; // Array para almacenar los elementos de V_value2 que se van a recibir
    struct Paquete p; // Variable para almacenar el valor3 (struct Paquete) que se va a recibir

    if (recv_string(fd, key, sizeof(key)) < 0) return -1; // Si falla al recibir la clave -> error

    r = get_value(key, value1, &N_value2, V_value2, &p); // get_value() con la clave recibida y almacenamos el resultado en r, y los datos recibidos en value1, N_value2, V_value2, y p

    if (send_int(fd, r) < 0) return -1; // Si falla al enviar el resultado -> error
    if (r != 0) return 0; // Si el resultado no es 0 -> no hay datos que enviar, terminamos aquí

    if (send_string(fd, value1) < 0) return -1; // Si falla al enviar value1 -> error
    if (send_int(fd, N_value2) < 0) return -1; // Si falla al enviar N_value2 -> error

    for (i = 0; i < N_value2; i++) { // Iteramos para enviar cada elemento de V_value2
        if (send_float(fd, V_value2[i]) < 0) return -1; // Si falla al enviar algún elemento de V_value2 -> error
    }

    if (send_int(fd, p.x) < 0) return -1; // Si falla al enviar p.x -> error
    if (send_int(fd, p.y) < 0) return -1; // Si falla al enviar p.y -> error
    if (send_int(fd, p.z) < 0) return -1; // Si falla al enviar p.z -> error

    return 0; // Datos enviados correctamente
}

// Petición de tipo modify_value
static int atender_modify(int fd) {
    char key[MAX_STR]; // Buffer para la clave
    char value1[MAX_STR]; // Buffer para almacenar el valor1 que se va a recibir de modify_value()
    int N_value2, i, r; // Variable para almacenar el número de elementos de V_value2 que se va a recibir, un índice para iterar, y el resultado de la operación
    float V_value2[MAX_V2]; // Array para almacenar los elementos de V_value2 que se van a recibir
    struct Paquete p; // Variable para almacenar el valor3 (struct Paquete) que se va a recibir

    if (recv_string(fd, key, sizeof(key)) < 0) return -1; // Si falla al recibir la clave -> error
    if (recv_string(fd, value1, sizeof(value1)) < 0) return -1; // Si falla al recibir value1 -> error
    if (recv_int(fd, &N_value2) < 0) return -1; // Si falla al recibir N_value2 -> error

    if (N_value2 < 1 || N_value2 > MAX_V2) { // Si el número de elementos de V_value2 no es válido -> error
        return send_int(fd, -1); // Enviamos -1 para indicar error al cliente
    }

    for (i = 0; i < N_value2; i++) { // Iteramos para recibir cada elemento de V_value2
        if (recv_float(fd, &V_value2[i]) < 0) return -1; // Si falla al recibir algún elemento de V_value2 -> error
    }

    if (recv_int(fd, &p.x) < 0) return -1; // Si falla al recibir p.x -> error
    if (recv_int(fd, &p.y) < 0) return -1; // Si falla al recibir p.y -> error
    if (recv_int(fd, &p.z) < 0) return -1; // Si falla al recibir p.z -> error

    r = modify_value(key, value1, N_value2, V_value2, p); // modify_value() con los datos recibidos y almacenamos el resultado en r
    return send_int(fd, r); // Enviamos el resultado al cliente
}

// Petición de tipo delete
static int atender_delete(int fd) {
    char key[MAX_STR]; // Buffer para la clave
    int r; // Variable para almacenar el resultado de la operación

    if (recv_string(fd, key, sizeof(key)) < 0) return -1; // Si falla al recibir la clave -> error
    r = delete_key(key); // delete_key() con la clave recibida y almacenamos el resultado en r
    return send_int(fd, r); // Enviamos el resultado al cliente
}

// Petición de tipo exist
static int atender_exist(int fd) {
    char key[MAX_STR]; // Buffer para la clave
    int r; // Variable para almacenar el resultado de la operación

    if (recv_string(fd, key, sizeof(key)) < 0) return -1; // Si falla al recibir la clave -> error
    r = exist(key); // exist() con la clave recibida y almacenamos el resultado en r
    return send_int(fd, r); // Enviamos el resultado al cliente
}

// Hilo por cliente
static void *thread_cliente(void *arg) {
    int fd = *((int *)arg); // Obtenemos el descriptor de la conexión con el cliente a partir del argumento
    int op; // Variable para almacenar el código de operación recibido del cliente

    free(arg); // Liberamos la memoria del argumento ya que no la necesitamos más

    if (recv_int(fd, &op) < 0) { // Si falla al recibir el código de operación -> error
        close(fd); // Cerramos la conexión con el cliente
        return NULL; // Terminamos el hilo
    }

    log_peticion(op); // Imprimimos el tipo de petición recibida en el servidor

    switch (op) { // Atendemos la petición según el código de operación recibido
        case OP_DESTROY:
            if (atender_destroy(fd) < 0) { // Si falla al atender destroy -> cerramos la conexión y terminamos el hilo
                close(fd);
                return NULL;
            }
            break;
        case OP_SET_VALUE:
            if (atender_set(fd) < 0) { // Si falla al atender set_value -> cerramos la conexión y terminamos el hilo
                close(fd);
                return NULL;
            }
            break;
        case OP_GET_VALUE:
            if (atender_get(fd) < 0) { // Si falla al atender get_value -> cerramos la conexión y terminamos el hilo
                close(fd);
                return NULL;
            }
            break;
        case OP_MODIFY:
            if (atender_modify(fd) < 0) { // Si falla al atender modify_value -> cerramos la conexión y terminamos el hilo
                close(fd);
                return NULL;
            }
            break;
        case OP_DELETE:
            if (atender_delete(fd) < 0) { // Si falla al atender delete_key -> cerramos la conexión y terminamos el hilo
                close(fd);
                return NULL;
            }
            break;
        case OP_EXIST:
            if (atender_exist(fd) < 0) { // Si falla al atender exist -> cerramos la conexión y terminamos el hilo
                close(fd);
                return NULL;
            }
            break;
        default:
            send_int(fd, -1); // Si la operación no existe -> enviamos error al cliente
            break;
    }

    close(fd); // Cerramos la conexión con el cliente
    return NULL; // Terminamos el hilo
}

// Main del servidor
int main(int argc, char *argv[]) {
    struct addrinfo hints, *res, *rp; // Variables para configurar el socket y almacenar las direcciones
    int listen_fd = -1; // Variable para el descriptor del socket de escucha
    int yes = 1; // Variable para la opción SO_REUSEADDR

    if (argc != 2) { // Si no se especifica el puerto como argumento -> error
        fprintf(stderr, "Uso: %s <PUERTO>\n", argv[0]); // Imprimimos el mensaje de uso en stderr
        return 1; // Devolvemos 1 para indicar error
    }

    memset(&hints, 0, sizeof(hints)); // Limpiamos la estructura hints con ceros
    hints.ai_family = AF_UNSPEC;      // IPv4 o IPv6
    hints.ai_socktype = SOCK_STREAM;  // TCP
    hints.ai_flags = AI_PASSIVE;      // Escuchar en cualquier interfaz

    int ret = getaddrinfo(NULL, argv[1], &hints, &res);
    if (ret != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        return 1;
    }

    for (rp = res; rp != NULL; rp = rp->ai_next) { // Iteramos sobre las direcciones obtenidas por getaddrinfo para intentar crear y bindear el socket
        listen_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol); // Intentamos crear el socket con la dirección actual
        if (listen_fd < 0) continue; // Si falla al crear el socket -> intentamos con la siguiente dirección

        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)); // Establecemos la opción SO_REUSEADDR para permitir reutilizar el puerto rápidamente después de cerrar el servidor

        if (bind(listen_fd, rp->ai_addr, rp->ai_addrlen) == 0) { // Si bind es exitoso -> hemos encontrado una dirección válida para el socket de escucha
            break; // Salimos del bucle ya que hemos creado y bindeado el socket correctamente
        }

        close(listen_fd); // Si falla al bindear el socket -> cerramos el socket y seguimos intentando con la siguiente dirección
        listen_fd = -1; // Reiniciamos listen_fd para indicar que no tenemos un socket válido aún
    }

    freeaddrinfo(res); // Liberamos la memoria de las direcciones obtenidas por getaddrinfo

    if (listen_fd < 0) { // Si no hemos podido crear un socket de escucha válido -> error
        perror("bind/socket"); // Imprimimos el error de bind o socket
        return 1; // Devolvemos 1 para indicar error
    }

    if (listen(listen_fd, BACKLOG) < 0) { // Si listen devuelve un error -> lo imprimimos y cerramos el socket
        perror("listen"); // Imprimimos el error de listen
        close(listen_fd); // Cerramos el socket de escucha
        return 1; // Devolvemos 1 para indicar error
    }

    printf("[SERVIDOR] Escuchando en el puerto %s...\n", argv[1]); // Imprimimos un mensaje indicando que el servidor está escuchando en el puerto especificado
    fflush(stdout); // Aseguramos que el mensaje se imprima antes de aceptar conexiones

    while (1) { // Bucle principal para aceptar conexiones de clientes
        int client_fd; // Variable para el descriptor de la conexión con el cliente
        struct sockaddr_storage client_addr; // Variable para almacenar la dirección del cliente
        socklen_t client_len = sizeof(client_addr); // Variable para almacenar la longitud de la dirección del cliente
        pthread_t th; // Variable para el identificador del hilo que atenderá al cliente
        int *pclient; // Variable para almacenar el descriptor del cliente que se pasará al hilo

        client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len); // Aceptamos una conexión entrante de un cliente y obtenemos el descriptor de la conexión y la dirección del cliente
        if (client_fd < 0) { // Si accept devuelve un error -> lo imprimimos y continuamos
            if (errno == EINTR) continue; // Si fue por interrupción -> intentamos aceptar de nuevo
            perror("accept"); // Imprimimos el error de accept
            continue; // Continuamos para aceptar la siguiente conexión entrante
        }

        pclient = (int *)malloc(sizeof(int)); // Reservamos memoria para almacenar el descriptor del cliente que se pasará al hilo
        if (pclient == NULL) { // Si falla la reserva de memoria -> lo imprimimos, cerramos la conexión con el cliente, y continuamos
            perror("malloc"); // Imprimimos el error de malloc
            close(client_fd); // Cerramos la conexión con el cliente
            continue; // Continuamos para aceptar la siguiente conexión entrante
        }

        *pclient = client_fd; // Almacenamos el descriptor del cliente en la variable que se pasará al hilo

        if (pthread_create(&th, NULL, thread_cliente, pclient) != 0) { // Si falla al crear el hilo -> lo imprimimos, cerramos la conexión con el cliente, liberamos la memoria, y continuamos
            perror("pthread_create"); // Imprimimos el error de pthread_create
            close(client_fd); // Cerramos la conexión con el cliente
            free(pclient); // Liberamos la memoria reservada para el descriptor del cliente
            continue; // Continuamos para aceptar la siguiente conexión entrante
        }

        pthread_detach(th); // Desacoplamos el hilo para no tener que hacer join()
    }

    close(listen_fd); // Cerramos el socket de escucha
    return 0; // Devolvemos 0 para indicar que el servidor terminó correctamente
}