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

//escribe exactamente "count" (número de bytes que se queiren escirbir) en el descriptor del fichero (fd) desde el buffer (buf)
static int write_all(int fd, const void *buf, size_t count) {
    const char *p = (const char *)buf; //puntero para recorrer el buffer
    size_t total = 0; //total de bytes escritos 

    while (total < count) { //seguir escribiendo hasta que se haya escrito todo
        ssize_t n = write(fd, p + total, count - total); //intentar escribir el resto del buffer
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
    if (write_all(fd, "\n", 1) < 0) return -1; //error: al escribir el carácter de nueva línea 
    return 0; 
}

//envía un entero como línea
static int send_int(int fd, int value) {
    char buf[MAX_LINE]; //buffer para convertir el entero a texto
    snprintf(buf, sizeof(buf), "%d", value); //conversion el entero a texto y almacenarlo en el buf
    return send_line(fd, buf); //envia la línea con el entero convertido
}

//envía un float como línea
static int send_float(int fd, float value) {
    char buf[MAX_LINE]; //buffer para convertir el float a texto
    snprintf(buf, sizeof(buf), "%.9g", value); //conversion del float a texto y lo almacenamos en buf
    return send_line(fd, buf); //envia la línea con el float convertido
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
            return 0; //lina recibida correctamente
        }

        buffer[i++] = c; //almacenamiento del carácter leído en buffer y avanzamos el índice
    }

    buffer[i] = '\0'; //terminar el string con '\0' aunque no hayamos leído un '\n' pq la línea es demasiado larga
    return -1; //error: (línea es demasiado larga para el buffer o no apareció '\n' a tiempo
}

//recibe un entero enviado como línea
static int recv_int(int fd, int *value) {
    char buf[MAX_LINE]; //buffer para almacenar la línea recibida
    if (recv_line(fd, buf, sizeof(buf)) < 0) return -1; // error: falla al recibir la linea

    char *endptr = NULL; // puntero para verificar que toda la línea se convirtió correctamente a entero
    long v = strtol(buf, &endptr, 10); // conversion de la línea a entero usando base 10
    if (*buf == '\0' || *endptr != '\0') return -1; //error: la línea está vacía o no se convirtió completamente a entero 

    *value = (int)v; // almacenamiento del valor convertido en la variable apuntada por "value"
    return 0;
}

//recibe un float enviado como línea
static int recv_float(int fd, float *value) {
    char buf[MAX_LINE]; //buffer para almacenar la línea recibida
    if (recv_line(fd, buf, sizeof(buf)) < 0) return -1; //error: fallo al recibir la línea 

    char *endptr = NULL; // puntero para verificar que toda la línea se convirtió correctamente a float
    float v = strtof(buf, &endptr); // conversion de la línea a float
    if (*buf == '\0' || *endptr != '\0') return -1; //error: la línea está vacía o no se convirtió completamente a float 

    *value = v; //almacenamiento del valor convertido en la variable apuntada por "value"
    return 0; 
}

// Envía una cadena usando el siguiente protocolo:
//   1) La longitud del string en bytes, seguida de '\n'
//   2) El contenido exacto del string (sin modificar)
//   3) Un '\n' final como delimitador

static int send_string(int fd, const char *str) {
    int len; //variable para almacenar la longitud del string

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

// GESTION DE CONEXION CON EL SERVIDOR

// eestablece una conexión TCP con el servidor utilizando las variables de entorno IP_TUPLAS y PORT_TUPLAS
static int connect_to_server(void) {
    char *ip = getenv("IP_TUPLAS"); //sacar de IP_TUPLAS la IP del servidor 
    char *port = getenv("PORT_TUPLAS"); //sacar de PORT_TUPLAS el puerto del servidor
    struct addrinfo hints, *res, *rp; //variables para "getaddrinfo" 
    int fd = -1; //descriptor de socket para la conexión

    if (ip == NULL || port == NULL) { //error: alguna de las variables de entorno no está definida 
        fprintf(stderr, "ERROR: IP_TUPLAS o PORT_TUPLAS no definidas\n"); //mensaje de error en stderr
        return -1; // 1: indicar error
    }

    memset(&hints, 0, sizeof(hints)); //limpieza hints (configuran el comportamiento de getaddrinfo) para usar "getaddrinfo"
    hints.ai_family = AF_UNSPEC;      //IPv4 o IPv6
    hints.ai_socktype = SOCK_STREAM;  //TCP

    int ret = getaddrinfo(ip, port, &hints, &res); //saca las direcciones del servidor mediante getaddrinfo con la IP y puerto especificados
    if (ret != 0) { //error de getaddrinfo:lo imprimimos en stderr y devolvemos -1 para indicar error
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret)); 
        return -1; // -1:indicar error
    }

    for (rp = res; rp != NULL; rp = rp->ai_next) { //iteracion sobre las direcciones devueltas por getaddrinfo
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol); //crear un socket con la dirección actual
        if (fd < 0) continue; //probar otra direccion si falla al crear el socket 

        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) { //se sale del bucl si la conexión se establece correctamente 
            break; 
        }

        close(fd);
        fd = -1;
    }

    freeaddrinfo(res); //liberar la memoria de las direcciones devueltas por getaddrinfo
    return fd; //devolver el descriptor del socket conectado o -1 si no se pudo conectar
}

//implementar la API pedida en claves.h

// destroy()
int destroy(void) {
    int fd, result; // variable para almacenar el resultado del servidor

    fd = connect_to_server(); //conexión con el servidor
    if (fd < 0) return -1; //error:  no se ha podido conectar 

    if (send_int(fd, OP_DESTROY) < 0) { //error:falla al enviar el código de operación 
        close(fd); //cerrar la conexión con el servidor
        return -1; //-1:indicar error
    }

    if (recv_int(fd, &result) < 0) { //error: falla al recibir el resultado 
        close(fd); //cerrar la conexión con el servidor
        return -1; //-1:indicar error
    }

    close(fd); //cerrar la conexión con el servidor
    return result; // devolver código de resultado enviado por el servidor
}

// set_value()
int set_value(char *key, char *value1, int N_value2, float *V_value2, struct Paquete value3) {
    int fd, result, i; //variable para almacenar el resultado devuelto por el servidor y un índice para iterar

    if (key == NULL || value1 == NULL || V_value2 == NULL) return -1; //error: alguno de los punteros es NULL 
    if (N_value2 < 1 || N_value2 > MAX_V2) return -1; //error: N_value2 está fuera del rango válido 

    fd = connect_to_server(); //establecer una conexión con el servidor
    if (fd < 0) return -1; //error: no se pudo conectar 

    if (send_int(fd, OP_SET_VALUE) < 0) goto error; //error: falla al enviar el código de operación 
    if (send_string(fd, key) < 0) goto error; //error: falla al enviar la clave 
    if (send_string(fd, value1) < 0) goto error; //error: falla al enviar el valor1 
    if (send_int(fd, N_value2) < 0) goto error; //error: falla al enviar el número de elementos de V_value2 

    for (i = 0; i < N_value2; i++) { //iterar sobre los elementos de V_value2
        if (send_float(fd, V_value2[i]) < 0) goto error; //error: falla al enviar algún elemento de V_value2
    }

    if (send_int(fd, value3.x) < 0) goto error; //error: falla al enviar el valor x de value3 
    if (send_int(fd, value3.y) < 0) goto error; //error: falla al enviar el valor y de value3 
    if (send_int(fd, value3.z) < 0) goto error; //error: falla al enviar el valor z de value3 

    if (recv_int(fd, &result) < 0) goto error; //errror: falla al recibir el resultado 

    close(fd); //cerrar la conexión con el servidor
    return result; //código de resultado enviado por el servidor

//si ha habido algun fallo: 
error:
    close(fd); //cerrar la conexión con el servidor antes de devolver el error
    return -1; //-1: indicar error
}

// get_value()
int get_value(char *key, char *value1, int *N_value2, float *V_value2, struct Paquete *value3) {
    int fd, result, i; // variable para almacenar el resultado devuelto por el servidor y un índice para iterar

    if (key == NULL || value1 == NULL || N_value2 == NULL || V_value2 == NULL || value3 == NULL) { //error: N_value2 está fuera del rango válido 
        return -1; //-1: indicar error
    }

    fd = connect_to_server(); //establecer una conexión con el servidor
    if (fd < 0) return -1; //error: no se pudo conectar 

    if (send_int(fd, OP_GET_VALUE) < 0) goto error; //error: falla al enviar el código de operación 
    if (send_string(fd, key) < 0) goto error; //error: falla al enviar la clave 

    if (recv_int(fd, &result) < 0) goto error; //error: falla al recibir el resultado 
    if (result != 0) { //no hay datos que recibir si el servidor devuelve un código distinto de 0 
        close(fd); //cerrar la conexión con el servidor
        return result; //código de resultado enviado por el servidor
    }

    if (recv_string(fd, value1, MAX_STR) < 0) goto error; //error: falla al recibir value1 
    if (recv_int(fd, N_value2) < 0) goto error; //error: falla al recibir N_value2 

    if (*N_value2 < 1 || *N_value2 > MAX_V2) goto error; //error: número de elementos es inválido 

    for (i = 0; i < *N_value2; i++) { //iteramos sobre los elementos de V_value2
        if (recv_float(fd, &V_value2[i]) < 0) goto error; //error: falla al recibir algún elemento de V_value2 
    }

    for (i = *N_value2; i < MAX_V2; i++) { //iterar sobre el resto de los elementos de V_value2
        V_value2[i] = 0.0f; //limpiar el resto de V_value2 para evitar basura en campos no usados
    }

    if (recv_int(fd, &value3->x) < 0) goto error; //error: fallo al recibir el valor x de value3 
    if (recv_int(fd, &value3->y) < 0) goto error; //error: falla al recibir el valor y de value3 
    if (recv_int(fd, &value3->z) < 0) goto error; //error: falla al recibir el valor z de value3 -> error

    close(fd); //cerrar la conexión con el servidor
    return result; //código de resultado enviado por el servidor

//si ha habido algun fallo:
error:
    close(fd); //cerrar la conexión con el servidor antes de devolver el error
    return -1; //-1: indicar error
}

// modify_value()
int modify_value(char *key, char *value1, int N_value2, float *V_value2, struct Paquete value3) {
    int fd, result, i; // variable para almacenar el resultado devuelto por el servidor y un índice para iterar

    if (key == NULL || value1 == NULL || V_value2 == NULL) return -1;
    if (N_value2 < 1 || N_value2 > MAX_V2) return -1; //error:  N_value2 está fuera del rango válido 

    fd = connect_to_server(); //establecer una conexión con el servidor
    if (fd < 0) return -1; //error: no se pudo conectar 

    if (send_int(fd, OP_MODIFY) < 0) goto error; //error: falla al enviar el código de operación 
    if (send_string(fd, key) < 0) goto error; //error: falla al enviar la clave 
    if (send_string(fd, value1) < 0) goto error; //error: falla al enviar value1 
    if (send_int(fd, N_value2) < 0) goto error; //error: falla al enviar N_value2 

    for (i = 0; i < N_value2; i++) { //iterar sobre los elementos de V_value2
        if (send_float(fd, V_value2[i]) < 0) goto error; //error: falla al enviar algún elemento de V_value2 
    }

    if (send_int(fd, value3.x) < 0) goto error; //error: falla al enviar el valor x de value3 
    if (send_int(fd, value3.y) < 0) goto error; //error: falla al enviar el valor y de value3
    if (send_int(fd, value3.z) < 0) goto error; //error:  falla al enviar el valor z de value3 

    if (recv_int(fd, &result) < 0) goto error; //error: falla al recibir el resultado 

    close(fd); //cerrar la conexión con el servidor
    return result; //código de resultado enviado por el servidor

//si ha habido algun fallo:
error:
    close(fd); //cerrar la conexión con el servidor antes de devolver el error
    return -1; //-1:indicar error
}

// delete_key()
int delete_key(char *key) {
    int fd, result; // variable para almacenar el resultado devuelto por el servidor

    if (key == NULL) return -1; // error: clave es NULL 

    fd = connect_to_server(); //establecer una conexión con el servidor
    if (fd < 0) return -1; //error: no se pudo conectar 

    if (send_int(fd, OP_DELETE) < 0) goto error; //error: falla al enviar el código de operación 
    if (send_string(fd, key) < 0) goto error; //error: falla al enviar la clave 

    if (recv_int(fd, &result) < 0) goto error; //error: falla al recibir el resultado

    close(fd); //cerrar la conexión con el servidor
    return result; //código de resultado enviado por el servidor

//si ha habido algun fallo:
error:
    close(fd); //cerrrar la conexión con el servidor antes de devolver el error
    return -1; //-1: indicar error
}

// exist()
int exist(char *key) {
    int fd, result; //variable para almacenar el resultado devuelto por el servidor

    if (key == NULL) return -1; //errror: la clave es NULL 

    fd = connect_to_server(); //establecer una conexión con el servidor
    if (fd < 0) return -1; //error: no se pudo conectar 

    if (send_int(fd, OP_EXIST) < 0) goto error; //error: falla al enviar el código de operación 
    if (send_string(fd, key) < 0) goto error; //error: falla al enviar la clave 

    if (recv_int(fd, &result) < 0) goto error; //error: falla al recibir el resultado 

    close(fd); //cerrar la conexión con el servidor
    return result; //código de resultado enviado por el servidor (1 si existe, 0 si no existe, -1 si error)

//si ha habido algun fallo:
error:
    close(fd); //cerrar la conexión con el servidor antes de devolver el error
    return -1; //-1: indicar error
}