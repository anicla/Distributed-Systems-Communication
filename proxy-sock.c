#define _POSIX_C_SOURCE 200112L

#include "claves.h" // prototipos de la API y struct Paquete

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define MAX_LINE 512 // tamaño máximo para las líneas de texto en el protocolo
#define MAX_STR 256 // maximo tamaño de strings (255 chars útiles + '\0')
#define MAX_V2 32 // máximo número de elementos en v_value2

// codigos de operacion 
#define OP_DESTROY     1
#define OP_SET_VALUE   2
#define OP_GET_VALUE   3
#define OP_MODIFY      4
#define OP_DELETE      5
#define OP_EXIST       6

//FUNCIONES AUXILIARES DE ENTRADA/SALIDA

// escribe exactamente "count" bytes en el descriptor fd desde buf
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
        if (n == 0) return -1; // Si read devuelve 0 -> el otro extremo ha cerrado la conexión antes de completar la lectura
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

// Envía un entero como línea
static int send_int(int fd, int value) {
    char buf[MAX_LINE]; // Buffer para convertir el entero a texto
    snprintf(buf, sizeof(buf), "%d", value); // Convertimos el entero a texto y lo almacenamos en buf
    return send_line(fd, buf); // Enviamos la línea con el entero convertido
}

// Envía un float como línea
static int send_float(int fd, float value) {
    char buf[MAX_LINE]; // Buffer para convertir el float a texto
    snprintf(buf, sizeof(buf), "%.9g", value); // Convertimos el float a texto y lo almacenamos en buf
    return send_line(fd, buf); // Enviamos la línea con el float convertido
}

// Recibe una línea de texto (terminada en '\n') y la almacena en buffer (con capacidad maxlen)
static int recv_line(int fd, char *buffer, size_t maxlen) {
    size_t i = 0; // Índice para almacenar caracteres en buffer
    char c; // Variable para leer caracteres uno a uno

    if (maxlen == 0) return -1; // Si maxlen es 0 -> error (no hay espacio para almacenar nada)

    while (i < maxlen - 1) { // Mientras no hayamos llenado el buffer (dejando espacio para '\0') -> leer caracteres
        ssize_t n = read(fd, &c, 1); // Leemos un carácter del descriptor fd
        if (n < 0) { // Si hay error -> verificamos si fue por interrupción (EINTR) o es un error real
            if (errno == EINTR) continue; // Si fue por interrupción -> intentamos leer de nuevo
            return -1; // Si es un error real -> devolvemos -1
        }
        if (n == 0) return -1; // Si read devuelve 0 -> el otro extremo ha cerrado la conexión antes de completar la lectura

        if (c == '\n') { // Si el carácter leído es '\n' -> hemos terminado de leer la línea
            buffer[i] = '\0'; // Terminamos el string con '\0'
            return 0; // Línea recibida correctamente
        }

        buffer[i++] = c; // Almacenamos el carácter leído en buffer y avanzamos el índice
    }

    buffer[i] = '\0'; // Terminamos el string con '\0' aunque no hayamos leído un '\n' (línea demasiado larga)
    return -1; // Si llegamos aquí -> error (línea es demasiado larga para el buffer o no apareció '\n' a tiempo)
}

// Recibe un entero enviado como línea
static int recv_int(int fd, int *value) {
    char buf[MAX_LINE]; // Buffer para almacenar la línea recibida
    if (recv_line(fd, buf, sizeof(buf)) < 0) return -1; // Si falla al recibir la línea -> error

    char *endptr = NULL; // Puntero para verificar que toda la línea se convirtió correctamente a entero
    long v = strtol(buf, &endptr, 10); // Convertimos la línea a entero usando base 10
    if (*buf == '\0' || *endptr != '\0') return -1; // Si la línea está vacía o no se convirtió completamente a entero -> error

    *value = (int)v; // Almacenamos el valor convertido en la variable apuntada por value
    return 0; // Entero recibido correctamente
}

// Recibe un float enviado como línea
static int recv_float(int fd, float *value) {
    char buf[MAX_LINE]; // Buffer para almacenar la línea recibida
    if (recv_line(fd, buf, sizeof(buf)) < 0) return -1; // Si falla al recibir la línea -> error

    char *endptr = NULL; // Puntero para verificar que toda la línea se convirtió correctamente a float
    float v = strtof(buf, &endptr); // Convertimos la línea a float
    if (*buf == '\0' || *endptr != '\0') return -1; // Si la línea está vacía o no se convirtió completamente a float -> error

    *value = v; // Almacenamos el valor convertido en la variable apuntada por value
    return 0; // Float recibido correctamente
}

// Envía un string con este formato:
//   <longitud>\n
//   <bytes exactos del string>
//   \n
static int send_string(int fd, const char *str) {
    int len; // Variable para almacenar la longitud del string

    if (str == NULL) return -1; // Si el string es NULL -> error

    len = (int)strlen(str); // Calculamos la longitud del string (sin contar el '\0')
    if (send_int(fd, len) < 0) return -1; // Si falla al enviar la longitud -> error
    if (len > 0 && write_all(fd, str, (size_t)len) < 0) return -1; // Si falla al enviar el string -> error
    if (write_all(fd, "\n", 1) < 0) return -1; // Si falla al enviar el carácter de nueva línea -> error

    return 0; // String enviado correctamente
}

// Recibe un string con el formato definido en send_string y lo almacena en buffer (con capacidad maxlen)
static int recv_string(int fd, char *buffer, size_t maxlen) {
    int len; // Variable para almacenar la longitud del string que se va a recibir
    char newline; // Debe contener el '\n' que separa el bloque de datos del siguiente campo

    if (recv_int(fd, &len) < 0) return -1; // Si falla al recibir la longitud -> error
    if (len < 0) return -1; // Si la longitud es negativa -> error
    if ((size_t)len >= maxlen) return -1; // Si la longitud excede la capacidad del buffer -> error

    if (len > 0) { // Si hay algo que leer -> leemos el string
        if (read_all(fd, buffer, (size_t)len) < 0) return -1; // Si falla al leer el string -> error
    }
    buffer[len] = '\0'; // Terminamos el string con '\0'

    if (read_all(fd, &newline, 1) < 0) return -1; // Si falla al leer el carácter de nueva línea -> error
    if (newline != '\n') return -1; // Si el carácter no es de nueva línea -> error

    return 0; // String recibido correctamente
}

// Gestión de conexión con el servidor

// Establece una conexión TCP con el servidor usando las variables de entorno IP_TUPLAS y PORT_TUPLAS
static int connect_to_server(void) {
    char *ip = getenv("IP_TUPLAS"); // Obtenemos la IP del servidor desde la variable de entorno IP_TUPLAS
    char *port = getenv("PORT_TUPLAS"); // Obtenemos el puerto del servidor desde la variable de entorno PORT_TUPLAS
    struct addrinfo hints, *res, *rp; // Variables para getaddrinfo
    int fd = -1; // Descriptor de socket para la conexión

    if (ip == NULL || port == NULL) { // Si alguna de las variables de entorno no está definida -> error
        fprintf(stderr, "ERROR: IP_TUPLAS o PORT_TUPLAS no definidas\n"); // Imprimimos un mensaje de error en stderr
        return -1; // Devolvemos -1 para indicar error
    }

    memset(&hints, 0, sizeof(hints)); // Limpiamos la estructura hints para usarla con getaddrinfo
    hints.ai_family = AF_UNSPEC;      // IPv4 o IPv6
    hints.ai_socktype = SOCK_STREAM;  // TCP

    int ret = getaddrinfo(ip, port, &hints, &res); // Obtenemos las direcciones del servidor usando getaddrinfo con la IP y puerto especificados
    if (ret != 0) { // Si getaddrinfo devuelve un error -> lo imprimimos en stderr y devolvemos -1 para indicar error
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret)); // Imprimimos el error
        return -1; // Devolvemos -1 para indicar error
    }

    for (rp = res; rp != NULL; rp = rp->ai_next) { // Iteramos sobre las direcciones devueltas por getaddrinfo
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol); // Intentamos crear un socket con la dirección actual
        if (fd < 0) continue; // Si falla al crear el socket -> intentamos con la siguiente dirección

        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) { // Si la conexión se establece correctamente -> salimos del bucle
            break; // Conexión establecida
        }

        close(fd);
        fd = -1;
    }

    freeaddrinfo(res); // Liberamos la memoria de las direcciones devueltas por getaddrinfo
    return fd; // Devolvemos el descriptor del socket conectado o -1 si no se pudo conectar
}

// Implementación de la API pedida en claves.h

// destroy()
int destroy(void) {
    int fd, result; // Variable para almacenar el resultado devuelto por el servidor

    fd = connect_to_server(); // Intentamos establecer una conexión con el servidor
    if (fd < 0) return -1; // Si no se pudo conectar -> error

    if (send_int(fd, OP_DESTROY) < 0) { // Si falla al enviar el código de operación -> error
        close(fd); // Cerramos la conexión con el servidor
        return -1; // Devolvemos -1 para indicar error
    }

    if (recv_int(fd, &result) < 0) { // Si falla al recibir el resultado -> error
        close(fd); // Cerramos la conexión con el servidor
        return -1; // Devolvemos -1 para indicar error
    }

    close(fd); // Cerramos la conexión con el servidor
    return result; // Devolvemos el código de resultado enviado por el servidor
}

// set_value()
int set_value(char *key, char *value1, int N_value2, float *V_value2, struct Paquete value3) {
    int fd, result, i; // Variable para almacenar el resultado devuelto por el servidor y un índice para iterar

    if (key == NULL || value1 == NULL || V_value2 == NULL) return -1; // Si alguno de los punteros es NULL -> error
    if (N_value2 < 1 || N_value2 > MAX_V2) return -1; // Si N_value2 está fuera del rango válido -> error

    fd = connect_to_server(); // Intentamos establecer una conexión con el servidor
    if (fd < 0) return -1; // Si no se pudo conectar -> error

    if (send_int(fd, OP_SET_VALUE) < 0) goto error; // Si falla al enviar el código de operación -> error
    if (send_string(fd, key) < 0) goto error; // Si falla al enviar la clave -> error
    if (send_string(fd, value1) < 0) goto error; // Si falla al enviar el valor1 -> error
    if (send_int(fd, N_value2) < 0) goto error; // Si falla al enviar el número de elementos de V_value2 -> error

    for (i = 0; i < N_value2; i++) { // Iteramos sobre los elementos de V_value2
        if (send_float(fd, V_value2[i]) < 0) goto error; // Si falla al enviar algún elemento de V_value2 -> error
    }

    if (send_int(fd, value3.x) < 0) goto error; // Si falla al enviar el valor x de value3 -> error
    if (send_int(fd, value3.y) < 0) goto error; // Si falla al enviar el valor y de value3 -> error
    if (send_int(fd, value3.z) < 0) goto error; // Si falla al enviar el valor z de value3 -> error

    if (recv_int(fd, &result) < 0) goto error; // Si falla al recibir el resultado -> error

    close(fd); // Cerramos la conexión con el servidor
    return result; // Devolvemos el código de resultado enviado por el servidor

// Si llegamos aquí -> hubo un error
error:
    close(fd); // Cerramos la conexión con el servidor antes de devolver el error
    return -1; // Devolvemos -1 para indicar error
}

// get_value()
int get_value(char *key, char *value1, int *N_value2, float *V_value2, struct Paquete *value3) {
    int fd, result, i; // Variable para almacenar el resultado devuelto por el servidor y un índice para iterar

    if (key == NULL || value1 == NULL || N_value2 == NULL || V_value2 == NULL || value3 == NULL) { // Si N_value2 está fuera del rango válido -> error
        return -1; // Devolvemos -1 para indicar error
    }

    fd = connect_to_server(); // Intentamos establecer una conexión con el servidor
    if (fd < 0) return -1; // Si no se pudo conectar -> error

    if (send_int(fd, OP_GET_VALUE) < 0) goto error; // Si falla al enviar el código de operación -> error
    if (send_string(fd, key) < 0) goto error; // Si falla al enviar la clave -> error

    if (recv_int(fd, &result) < 0) goto error; // Si falla al recibir el resultado -> error
    if (result != 0) { // Si el servidor devuelve un código distinto de 0 -> no hay datos que recibir
        close(fd); // Cerramos la conexión con el servidor
        return result; // Devolvemos el código de resultado enviado por el servidor
    }

    if (recv_string(fd, value1, MAX_STR) < 0) goto error; // Si falla al recibir value1 -> error
    if (recv_int(fd, N_value2) < 0) goto error; // Si falla al recibir N_value2 -> error

    if (*N_value2 < 1 || *N_value2 > MAX_V2) goto error; // Si el número de elementos es inválido -> error

    for (i = 0; i < *N_value2; i++) { // Iteramos sobre los elementos de V_value2
        if (recv_float(fd, &V_value2[i]) < 0) goto error; // Si falla al recibir algún elemento de V_value2 -> error
    }

    for (i = *N_value2; i < MAX_V2; i++) { // Iteramos sobre el resto de los elementos de V_value2
        V_value2[i] = 0.0f; // Limpiamos el resto de V_value2 para evitar basura en campos no usados
    }

    if (recv_int(fd, &value3->x) < 0) goto error; // Si falla al recibir el valor x de value3 -> error
    if (recv_int(fd, &value3->y) < 0) goto error; // Si falla al recibir el valor y de value3 -> error
    if (recv_int(fd, &value3->z) < 0) goto error; // Si falla al recibir el valor z de value3 -> error

    close(fd); // Cerramos la conexión con el servidor
    return result; // Devolvemos el código de resultado enviado por el servidor

// Si llegamos aquí -> hubo un error
error:
    close(fd); // Cerramos la conexión con el servidor antes de devolver el error
    return -1; // Devolvemos -1 para indicar error
}

// modify_value()
int modify_value(char *key, char *value1, int N_value2, float *V_value2, struct Paquete value3) {
    int fd, result, i; // Variable para almacenar el resultado devuelto por el servidor y un índice para iterar

    if (key == NULL || value1 == NULL || V_value2 == NULL) return -1;
    if (N_value2 < 1 || N_value2 > MAX_V2) return -1; // Si N_value2 está fuera del rango válido -> error

    fd = connect_to_server(); // Intentamos establecer una conexión con el servidor
    if (fd < 0) return -1; // Si no se pudo conectar -> error

    if (send_int(fd, OP_MODIFY) < 0) goto error; // Si falla al enviar el código de operación -> error
    if (send_string(fd, key) < 0) goto error; // Si falla al enviar la clave -> error
    if (send_string(fd, value1) < 0) goto error; // Si falla al enviar value1 -> error
    if (send_int(fd, N_value2) < 0) goto error; // Si falla al enviar N_value2 -> error

    for (i = 0; i < N_value2; i++) { // Iteramos sobre los elementos de V_value2
        if (send_float(fd, V_value2[i]) < 0) goto error; // Si falla al enviar algún elemento de V_value2 -> error
    }

    if (send_int(fd, value3.x) < 0) goto error; // Si falla al enviar el valor x de value3 -> error
    if (send_int(fd, value3.y) < 0) goto error; // Si falla al enviar el valor y de value3 -> error
    if (send_int(fd, value3.z) < 0) goto error; // Si falla al enviar el valor z de value3 -> error

    if (recv_int(fd, &result) < 0) goto error; // Si falla al recibir el resultado -> error

    close(fd); // Cerramos la conexión con el servidor
    return result; // Devolvemos el código de resultado enviado por el servidor

// Si llegamos aquí -> hubo un error
error:
    close(fd); // Cerramos la conexión con el servidor antes de devolver el error
    return -1; // Devolvemos -1 para indicar error
}

// delete_key()
int delete_key(char *key) {
    int fd, result; // Variable para almacenar el resultado devuelto por el servidor

    if (key == NULL) return -1; // Si la clave es NULL -> error

    fd = connect_to_server(); // Intentamos establecer una conexión con el servidor
    if (fd < 0) return -1; // Si no se pudo conectar -> error

    if (send_int(fd, OP_DELETE) < 0) goto error; // Si falla al enviar el código de operación -> error
    if (send_string(fd, key) < 0) goto error; // Si falla al enviar la clave -> error

    if (recv_int(fd, &result) < 0) goto error; // Si falla al recibir el resultado -> error

    close(fd); // Cerramos la conexión con el servidor
    return result; // Devolvemos el código de resultado enviado por el servidor

// Si llegamos aquí -> hubo un error
error:
    close(fd); // Cerramos la conexión con el servidor antes de devolver el error
    return -1; // Devolvemos -1 para indicar error
}

// exist()
int exist(char *key) {
    int fd, result; // Variable para almacenar el resultado devuelto por el servidor

    if (key == NULL) return -1; // Si la clave es NULL -> error

    fd = connect_to_server(); // Intentamos establecer una conexión con el servidor
    if (fd < 0) return -1; // Si no se pudo conectar -> error

    if (send_int(fd, OP_EXIST) < 0) goto error; // Si falla al enviar el código de operación -> error
    if (send_string(fd, key) < 0) goto error; // Si falla al enviar la clave -> error

    if (recv_int(fd, &result) < 0) goto error; // Si falla al recibir el resultado -> error

    close(fd); // Cerramos la conexión con el servidor
    return result; // Devolvemos el código de resultado enviado por el servidor (1 si existe, 0 si no existe, -1 si error)

// Si llegamos aquí -> hubo un error
error:
    close(fd); // Cerramos la conexión con el servidor antes de devolver el error
    return -1; // Devolvemos -1 para indicar error
}