// PARTE A

#include "claves.h"
#include "claves.h"
#include <pthread.h> 
#include <stdlib.h> 
#include <string.h> 


// define para: 255 carateres +'\0' y el nº maximo de elem en v_valu2 (min 1 y max32)
#define MAX_STR 256 
#define MAX_V2  32 

// nodo de la lista enlazada para almacenar cada clave y sus valores asociados
typedef struct Nodo {
    char key[MAX_STR]; // clave (string max 255 )
    char value1[MAX_STR]; // valor1 (string max 255)
    int  N_value2; // nº de elementos en v_value2
    float V_value2[MAX_V2]; // vector de float
    struct Paquete value3; 
    struct Nodo *next; // puntero al siguiente nodo
} Nodo;

// puntero al inicio de la lista enlazada
static Nodo *g_head = NULL;

// mutex global que protege el acceso a la lista enlazada: vita condiciones de carrera cuando varios hilos acceden a la estructura
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

// copia segura: trunca a 255 y garantiza terminación en '\0'
static void safe_strcpy_255(char dst[MAX_STR], const char *src) {
    if (!src) { // si src es NULL entonces la cadena está vacía
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, MAX_STR - 1);
    dst[MAX_STR - 1] = '\0';
}

//busca un nodo por key  -> devuelve el nodo si lo encuentra o NULL si no existe
static Nodo* find_node(const char *key) {
    for (Nodo *cur = g_head; cur != NULL; cur = cur->next) {

        //por si coincide la clave entonces devolvemos el nodo encontrado
        if (strncmp(cur->key, key, MAX_STR) == 0) { 
            return cur;
        }
    }
    return NULL;
}

// valida longitudes máximas de strings según el contrato
static int validate_value1(const char *value1) {
    if (value1 == NULL) return 0;
    if (strnlen(value1, MAX_STR) > MAX_STR - 1) return 0;
    return 1;
}

// vlida formato de clave: no nula, no vacía, no demasiado larga
static int validate_key_local(const char *key) {
    if (key == NULL) return 0;
    if (key[0] == '\0') return 0; // extra: comprueba si la clave es una cadena vacía ("")
    if (strnlen(key, MAX_STR) > MAX_STR - 1) return 0;
    return 1;
}

// libera la memoria entera 
int destroy(void) {
    if (pthread_mutex_lock(&g_mutex) != 0) return -1;

    Nodo *cur = g_head;
    while (cur) {
        Nodo *next = cur->next;
        free(cur);
        cur = next;
    }
    g_head = NULL;

    if (pthread_mutex_unlock(&g_mutex) != 0) return -1;
    return 0;
}

// inserta una nueva tupla asociada a la clave si no existe previamente
int set_value(char *key, char *value1, int N_value2, float *V_value2, struct Paquete value3) {
    if (!validate_key_local(key)) return -1;
    if (!validate_value1(value1)) return -1;
    if (N_value2 < 1 || N_value2 > MAX_V2) return -1;
    if (V_value2 == NULL) return -1;

    // bloqueo para proteger la sección crítica
    if (pthread_mutex_lock(&g_mutex) != 0) return -1;

    // error si ya existe la clave
    if (find_node(key) != NULL) {

        // desbloqueamos mutex antes de retornar
        if (pthread_mutex_unlock(&g_mutex) != 0) return -1; 
        return -1;
    }

    // creamos un nuevo nodo para la nueva tupla
    Nodo *n = (Nodo*)malloc(sizeof(Nodo)); 
    if (!n) {

        // desbloqueamos mutex antes de retornar
        if (pthread_mutex_unlock(&g_mutex) != 0) return -1; 
        return -1;
    }

    //copia de datos en el nuevo nodo
    safe_strcpy_255(n->key, key); // copiar la clave 
    safe_strcpy_255(n->value1, value1); // copiar valor1 
    n->N_value2 = N_value2; // asignamos el número de elementos en v_value2
    for (int i = 0; i < N_value2; i++) { // copiar los valores de v_value2
        n->V_value2[i] = V_value2[i];
    }

    // limpiar el resto del vector
    for (int i = N_value2; i < MAX_V2; i++) {
        n->V_value2[i] = 0.0f;
    }
    n->value3 = value3; // aisnación de valor3

    // insertar el nodo al principio de la lista
    n->next = g_head;
    g_head = n;


    // desbloqueamos después de insertar
    if (pthread_mutex_unlock(&g_mutex) != 0) return -1; 
    return 0;
}

// recupera los valores asociados a una clave existente
int get_value(char *key, char *value1, int *N_value2, float *V_value2, struct Paquete *value3) {
    if (!validate_key_local(key)) return -1;
    if (value1 == NULL || N_value2 == NULL || V_value2 == NULL || value3 == NULL) return -1;

    // bloqueamos para proteger la sección crítica
    if (pthread_mutex_lock(&g_mutex) != 0) return -1; 

    // buscamos el nodo existente para recuperar sus valores
    Nodo *n = find_node(key);
    if (!n) {

        // desbloqueamosel mutex  antes de retornar
        if (pthread_mutex_unlock(&g_mutex) != 0) return -1; 
        return -1;
    }


    // copia de los valores guardados
    safe_strcpy_255(value1, n->value1); // copiar  valor1 
    *N_value2 = n->N_value2; // copiar  nº de elementos en v_value2
    for (int i = 0; i < n->N_value2; i++) {
        V_value2[i] = n->V_value2[i]; // copiar los valores de v_value2
    }
    for (int i = n->N_value2; i < MAX_V2; i++) {
        V_value2[i] = 0.0f; // limpiar el resto de v_value2
    }
    *value3 = n->value3; // copiar valor3

    // desbloquear el mutex después de recuperar los valores
    if (pthread_mutex_unlock(&g_mutex) != 0) return -1; 
    return 0;
}

// modificar los valores asociados a una clave existente
int modify_value(char *key, char *value1, int N_value2, float *V_value2, struct Paquete value3) {
    if (!validate_key_local(key)) return -1;
    if (!validate_value1(value1)) return -1;
    if (N_value2 < 1 || N_value2 > MAX_V2) return -1;
    if (V_value2 == NULL) return -1;

    // bloqeuar el mutex para proteger la sección crítica
    if (pthread_mutex_lock(&g_mutex) != 0) return -1; 

    // buscar el nodo existente para modificarlo
    Nodo *n = find_node(key);
    if (!n) {

        // desbloquear antes de retornar
        if (pthread_mutex_unlock(&g_mutex) != 0) return -1;
        return -1;
    }

    // actualizacion de valores
    safe_strcpy_255(n->value1, value1); // modificar valor1 
    n->N_value2 = N_value2; // modificar nº de elem en v_value2
    for (int i = 0; i < N_value2; i++) {
        n->V_value2[i] = V_value2[i]; // modificar los valores de v_value2
    }
    for (int i = N_value2; i < MAX_V2; i++) {
        n->V_value2[i] = 0.0f; // limpiar el resto de V_value2
    }
    n->value3 = value3; // modificar valor3

    // desbloquear el mutex al terminar
    if (pthread_mutex_unlock(&g_mutex) != 0) return -1; 
    return 0;
}

// eliminar una clave de la lista
int delete_key(char *key) {
    if (!validate_key_local(key)) return -1; // si key es NULL da error
    
    // bloquea para proteger la sección crítica
    if (pthread_mutex_lock(&g_mutex) != 0) return -1; 
    
    // puntero para recorrer la lista
    Nodo *cur = g_head; 

    // puntero para mantener el nodo anterior (necesario para eliminar)
    Nodo *prev = NULL; 

    // revorrer la lista para encontrar el nodo con la clave dada
    while (cur) {
        if (strncmp(cur->key, key, MAX_STR) == 0) { // si encontramos la clave entonces la eliminamos
            
            // si hay un nodo anterior entonces se actualiza su puntero
            if (prev) prev->next = cur->next; 

            // si no hay nodo anterior entonces estamos eliminando el primer nodo, hay que actualizar g_head
            else g_head = cur->next; 

            // liberar la memoria del nodo eliminado
            free(cur); 

            // desbloquear después de eliminar
            if (pthread_mutex_unlock(&g_mutex) != 0) return -1; 
            return 0;
        }
        
        // actualizar el nodo anterior antes de avanzar
        prev = cur; 
        
        // avanzar al siguiente nodo
        cur = cur->next; 
    }

    // desbloqeuar el mutex después de recorrer la lista
    if (pthread_mutex_unlock(&g_mutex) != 0) return -1; 
    
    // no existe
    return -1; 
}

// verificR si una clave existe en la lista
int exist(char *key) {
    if (!validate_key_local(key)) return -1;
    if (pthread_mutex_lock(&g_mutex) != 0) return -1;

    int res = (find_node(key) != NULL) ? 1 : 0;

    if (pthread_mutex_unlock(&g_mutex) != 0) return -1;
    return res;
}
