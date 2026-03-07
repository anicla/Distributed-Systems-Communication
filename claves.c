// implementación local (no distribuida)

#include "claves.h" // Prototipos de la API y definición de struct Paquete

#include <pthread.h> // Para mutex
#include <stdlib.h> // Para malloc/free
#include <string.h> // Para strncpy/strlen

#define MAX_STR 256 // 255 chars útiles + '\0'
#define MAX_V2  32 // Máximo número de elementos en V_value2 (según contrato, mínimo 1, máximo 32)

// Nodo de la lista enlazada para almacenar cada clave y sus valores asociados
typedef struct Nodo {
    char key[MAX_STR]; // Clave (string de hasta 255 chars útiles)
    char value1[MAX_STR]; // Valor1 (string de hasta 255 chars útiles)
    int  N_value2; // Número de elementos en V_value2
    float V_value2[MAX_V2]; // Vector de valores float
    struct Paquete value3; // Estructura Paquete
    struct Nodo *next; // Puntero al siguiente nodo
} Nodo;

static Nodo *g_head = NULL; // Puntero al inicio de la lista enlazada
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex para proteger el acceso a la lista enlazada

// Copia segura: trunca a 255 y garantiza terminación en '\0'
static void safe_strcpy_255(char dst[MAX_STR], const char *src) {
    if (!src) { // Si src es NULL -> cadena vacía
        dst[0] = '\0';
        return;
    }
    // Copiamos como máximo 255 y cerramos con '\0'
    strncpy(dst, src, MAX_STR - 1);
    dst[MAX_STR - 1] = '\0';
}

// Busca un nodo por key (asume key no NULL) -> devuelve puntero o NULL
static Nodo* find_node(const char *key) {
    for (Nodo *cur = g_head; cur != NULL; cur = cur->next) {
        if (strncmp(cur->key, key, MAX_STR) == 0) { // Si coincide la clave (comparación segura)
            return cur;
        }
    }
    return NULL;
}

// Valida longitudes máximas de strings según el contrato
static int validate_value1(const char *value1) {
    if (value1 == NULL) return 0;
    if (strnlen(value1, MAX_STR) > MAX_STR - 1) return 0;
    return 1;
}

// Valida formato de clave: no nula, no vacía, no demasiado larga
static int validate_key_local(const char *key) {
    if (key == NULL) return 0;
    if (key[0] == '\0') return 0;              // decisión adicional de diseño
    if (strnlen(key, MAX_STR) > MAX_STR - 1) return 0;
    return 1;
}

/// Libera toda la memoria y deja el estado limpio
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

// Inserta una nueva tupla asociada a la clave si no existe previamente
int set_value(char *key, char *value1, int N_value2, float *V_value2, struct Paquete value3) {
    if (!validate_key_local(key)) return -1;
    if (!validate_value1(value1)) return -1;
    if (N_value2 < 1 || N_value2 > MAX_V2) return -1;
    if (V_value2 == NULL) return -1;
    if (pthread_mutex_lock(&g_mutex) != 0) return -1;// Bloqueamos para proteger la sección crítica

    // Error si ya existe la clave
    if (find_node(key) != NULL) {
        if (pthread_mutex_unlock(&g_mutex) != 0) return -1; // Desbloqueamos antes de retornar
        return -1;
    }

    // Creamos un nuevo nodo para la nueva tupla
    Nodo *n = (Nodo*)malloc(sizeof(Nodo)); 
    if (!n) {
        if (pthread_mutex_unlock(&g_mutex) != 0) return -1; // Desbloqueamos antes de retornar
        return -1;
    }

    safe_strcpy_255(n->key, key); // Copiamos la clave de forma segura
    safe_strcpy_255(n->value1, value1); // Copiamos el valor1 de forma segura
    n->N_value2 = N_value2; // Asignamos el número de elementos en V_value2
    for (int i = 0; i < N_value2; i++) { // Copiamos los valores de V_value2
        n->V_value2[i] = V_value2[i];
    }
    // Limpiar el resto (deja estado consistente)
    for (int i = N_value2; i < MAX_V2; i++) {
        n->V_value2[i] = 0.0f;
    }
    n->value3 = value3; // Asignamos el valor3

    // Insertar al inicio
    n->next = g_head;
    g_head = n;

    if (pthread_mutex_unlock(&g_mutex) != 0) return -1; // Desbloqueamos después de insertar
    return 0;
}

// Recupera los valores asociados a una clave existente
int get_value(char *key, char *value1, int *N_value2, float *V_value2, struct Paquete *value3) {
    if (!validate_key_local(key)) return -1;
    if (value1 == NULL || N_value2 == NULL || V_value2 == NULL || value3 == NULL) return -1;
    if (pthread_mutex_lock(&g_mutex) != 0) return -1; // Bloqueamos para proteger la sección crítica

    // Buscamos el nodo existente para recuperar sus valores
    Nodo *n = find_node(key);
    if (!n) {
        if (pthread_mutex_unlock(&g_mutex) != 0) return -1; // Desbloqueamos antes de retornar
        return -1;
    }

    safe_strcpy_255(value1, n->value1); // Copiamos el valor1 de forma segura
    *N_value2 = n->N_value2; // Copiamos el número de elementos en V_value2
    for (int i = 0; i < n->N_value2; i++) {
        V_value2[i] = n->V_value2[i]; // Copiamos los valores de V_value2
    }
    for (int i = n->N_value2; i < MAX_V2; i++) {
        V_value2[i] = 0.0f; // Limpiamos el resto de V_value2
    }
    *value3 = n->value3; // Copiamos el valor3

    if (pthread_mutex_unlock(&g_mutex) != 0) return -1; // Desbloqueamos después de recuperar los valores
    return 0;
}

// Modifica los valores asociados a una clave existente
int modify_value(char *key, char *value1, int N_value2, float *V_value2, struct Paquete value3) {
    if (!validate_key_local(key)) return -1;
    if (!validate_value1(value1)) return -1;
    if (N_value2 < 1 || N_value2 > MAX_V2) return -1;
    if (V_value2 == NULL) return -1;
    if (pthread_mutex_lock(&g_mutex) != 0) return -1; // Bloqueamos para proteger la sección crítica

    // Buscamos el nodo existente para modificarlo
    Nodo *n = find_node(key);
    if (!n) {
        if (pthread_mutex_unlock(&g_mutex) != 0) return -1;// Desbloqueamos antes de retornar
        return -1;
    }

    safe_strcpy_255(n->value1, value1); // Modificamos el valor1 de forma segura
    n->N_value2 = N_value2; // Modificamos el número de elementos en V_value2
    for (int i = 0; i < N_value2; i++) {
        n->V_value2[i] = V_value2[i]; // Modificamos los valores de V_value2
    }
    for (int i = N_value2; i < MAX_V2; i++) {
        n->V_value2[i] = 0.0f; // Limpiamos el resto de V_value2
    }
    n->value3 = value3; // Modificamos el valor3

    if (pthread_mutex_unlock(&g_mutex) != 0) return -1; // Desbloqueamos después de modificar
    return 0;
}

// Elimina una clave de la lista
int delete_key(char *key) {
    if (!validate_key_local(key)) return -1; // Si key es NULL -> error
    
    if (pthread_mutex_lock(&g_mutex) != 0) return -1; // Bloqueamos para proteger la sección crítica
    
    Nodo *cur = g_head; // Puntero para recorrer la lista
    Nodo *prev = NULL; // Puntero para mantener el nodo anterior (necesario para eliminar)

    // Recorremos la lista para encontrar el nodo con la clave dada
    while (cur) {
        if (strncmp(cur->key, key, MAX_STR) == 0) { // Si encontramos la clave -> la eliminamos
            if (prev) prev->next = cur->next; // Si hay un nodo anterior -> actualizamos su puntero
            else g_head = cur->next; // Si no hay nodo anterior -> estamos eliminando el primer nodo, actualizamos g_head

            free(cur); // Liberamos la memoria del nodo eliminado
            if (pthread_mutex_unlock(&g_mutex) != 0) return -1; // Desbloqueamos después de eliminar
            return 0;
        }
        prev = cur; // Actualizamos el nodo anterior antes de avanzar
        cur = cur->next; // Avanzamos al siguiente nodo
    }

    if (pthread_mutex_unlock(&g_mutex) != 0) return -1; // Desbloqueamos después de recorrer la lista
    return -1; // no existe
}

// Verifica si una clave existe en la lista
int exist(char *key) {
    if (!validate_key_local(key)) return -1;
    if (pthread_mutex_lock(&g_mutex) != 0) return -1;

    int res = (find_node(key) != NULL) ? 1 : 0;

    if (pthread_mutex_unlock(&g_mutex) != 0) return -1;
    return res;
}
