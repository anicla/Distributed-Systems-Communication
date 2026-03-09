// PARTE A

//includes para: la API y definición de struct Paquete mutex, malloc/freess y strncpy/strlen
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

static Nodo *g_head = NULL; // puntero al inicio de la lista enlazada
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER; // mutex para proteger el acceso a la lista enlazada

// copia segura: trunca a 255 y garantiza terminación en '\0'
static void safe_strcpy_255(char dst[MAX_STR], const char *src) {
    if (!src) { // si src es NULL entonces la cadena está vacía
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, MAX_STR - 1);
    dst[MAX_STR - 1] = '\0';
}

//busca un nodo por key (asume key no NULL) -> devuelve puntero o NULL
static Nodo* find_node(const char *key) {
    for (Nodo *cur = g_head; cur != NULL; cur = cur->next) {
        if (strncmp(cur->key, key, MAX_STR) == 0) { //por si coincide la clave 
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
    if (pthread_mutex_lock(&g_mutex) != 0) return -1;// bloqueamos para proteger la sección crítica

    // error si ya existe la clave
    if (find_node(key) != NULL) {
        if (pthread_mutex_unlock(&g_mutex) != 0) return -1; // desbloqueamos antes de retornar
        return -1;
    }

    // creamos un nuevo nodo para la nueva tupla
    Nodo *n = (Nodo*)malloc(sizeof(Nodo)); 
    if (!n) {
        if (pthread_mutex_unlock(&g_mutex) != 0) return -1; // desbloqueamos antes de retornar
        return -1;
    }

    safe_strcpy_255(n->key, key); // copiar la clave 
    safe_strcpy_255(n->value1, value1); // copiar valor1 
    n->N_value2 = N_value2; // asignamos el número de elementos en v_value2
    for (int i = 0; i < N_value2; i++) { // Copiar los valores de v_value2
        n->V_value2[i] = V_value2[i];
    }
    // limpiar el resto 
    for (int i = N_value2; i < MAX_V2; i++) {
        n->V_value2[i] = 0.0f;
    }
    n->value3 = value3; // asignamos valor3

    // insertar al inicio
    n->next = g_head;
    g_head = n;

    if (pthread_mutex_unlock(&g_mutex) != 0) return -1; // desbloqueamos después de insertar
    return 0;
}

// recupera los valores asociados a una clave existente
int get_value(char *key, char *value1, int *N_value2, float *V_value2, struct Paquete *value3) {
    if (!validate_key_local(key)) return -1;
    if (value1 == NULL || N_value2 == NULL || V_value2 == NULL || value3 == NULL) return -1;
    if (pthread_mutex_lock(&g_mutex) != 0) return -1; // bloqueamos para proteger la sección crítica

    // buscamos el nodo existente para recuperar sus valores
    Nodo *n = find_node(key);
    if (!n) {
        if (pthread_mutex_unlock(&g_mutex) != 0) return -1; // desbloqueamos antes de retornar
        return -1;
    }

    safe_strcpy_255(value1, n->value1); // copir  valor1 
    *N_value2 = n->N_value2; // copiar  nº de elementos en v_value2
    for (int i = 0; i < n->N_value2; i++) {
        V_value2[i] = n->V_value2[i]; // copiar los valores de v_value2
    }
    for (int i = n->N_value2; i < MAX_V2; i++) {
        V_value2[i] = 0.0f; // limpiar el resto de v_value2
    }
    *value3 = n->value3; // copiar valor3

    if (pthread_mutex_unlock(&g_mutex) != 0) return -1; // desbloquear después de recuperar los valores
    return 0;
}

// modificar los valores asociados a una clave existente
int modify_value(char *key, char *value1, int N_value2, float *V_value2, struct Paquete value3) {
    if (!validate_key_local(key)) return -1;
    if (!validate_value1(value1)) return -1;
    if (N_value2 < 1 || N_value2 > MAX_V2) return -1;
    if (V_value2 == NULL) return -1;
    if (pthread_mutex_lock(&g_mutex) != 0) return -1; // bloqeuar para proteger la sección crítica

    // buscar el nodo existente para modificarlo
    Nodo *n = find_node(key);
    if (!n) {
        if (pthread_mutex_unlock(&g_mutex) != 0) return -1;// desbloquear antes de retornar
        return -1;
    }

    safe_strcpy_255(n->value1, value1); // modificar valor1 
    n->N_value2 = N_value2; // modificar nº de elem en v_value2
    for (int i = 0; i < N_value2; i++) {
        n->V_value2[i] = V_value2[i]; // modificar los valores de v_value2
    }
    for (int i = N_value2; i < MAX_V2; i++) {
        n->V_value2[i] = 0.0f; // limpiar el resto de V_value2
    }
    n->value3 = value3; // modificar valor3

    if (pthread_mutex_unlock(&g_mutex) != 0) return -1; // desbloquear el mutex al terminar
    return 0;
}

// eliminar una clave de la lista
int delete_key(char *key) {
    if (!validate_key_local(key)) return -1; // si key es NULL -> error
    
    if (pthread_mutex_lock(&g_mutex) != 0) return -1; // bloquear para proteger la sección crítica
    
    Nodo *cur = g_head; // puntero para recorrer la lista
    Nodo *prev = NULL; // puntero para mantener el nodo anterior (necesario para eliminar)

    // revorrer la lista para encontrar el nodo con la clave dada
    while (cur) {
        if (strncmp(cur->key, key, MAX_STR) == 0) { // si encontramos la clave -> la eliminamos
            if (prev) prev->next = cur->next; // si hay un nodo anterior -> actualizamos su puntero
            else g_head = cur->next; // si no hay nodo anterior -> estamos eliminando el primer nodo, actualizamos g_head

            free(cur); // liberar la memoria del nodo eliminado
            if (pthread_mutex_unlock(&g_mutex) != 0) return -1; // desbloquear después de eliminar
            return 0;
        }
        prev = cur; // actualizar el nodo anterior antes de avanzar
        cur = cur->next; // avanzar al siguiente nodo
    }

    if (pthread_mutex_unlock(&g_mutex) != 0) return -1; // desbloqeuar después de recorrer la lista
    return -1; // no existe
}

// verificR si una clave existe en la lista
int exist(char *key) {
    if (!validate_key_local(key)) return -1;
    if (pthread_mutex_lock(&g_mutex) != 0) return -1;

    int res = (find_node(key) != NULL) ? 1 : 0;

    if (pthread_mutex_unlock(&g_mutex) != 0) return -1;
    return res;
}
