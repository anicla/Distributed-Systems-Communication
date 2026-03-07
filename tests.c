#include "claves.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>

#define MAX_V2  32    // tamaño máximo del vector según el enunciado
#define MAX_STR 256   // 255 chars útiles + '\0'

// contadores para saber cuántos tests han ido bien y cuántos han fallado
static int tests_ok = 0, tests_fail = 0;

/* ------------------ funciones de ayuda para comprobar resultados ------------------ */

// para tests que devuelven int (0, -1, etc.)
static void check_int(const char *name, int got, int expected) {
    if (got == expected) {
        printf("[OK]   %s -> %d\n", name, got);
        tests_ok++;
    } else {
        printf("[FAIL] %s -> got %d expected %d\n", name, got, expected);
        tests_fail++;
    }
}

// para comparar strings (value1 normalmente)
static void check_str(const char *name, const char *got, const char *expected) {
    if (strcmp(got, expected) == 0) {
        printf("[OK]   %s -> \"%s\"\n", name, got);
        tests_ok++;
    } else {
        printf("[FAIL] %s -> got \"%s\" expected \"%s\"\n", name, got, expected);
        tests_fail++;
    }
}

// para comprobar el vector de floats (que coincida elemento a elemento)
static void check_float_vec(const char *name, float *got, float *expected, int n) {
    for (int i = 0; i < n; i++) {
        if (got[i] != expected[i]) {
            printf("[FAIL] %s -> mismatch at %d (got %.6f expected %.6f)\n",
                   name, i, got[i], expected[i]);
            tests_fail++;
            return;
        }
    }
    printf("[OK]   %s -> vector matches (%d elems)\n", name, n);
    tests_ok++;
}

// para comprobar el struct Paquete (x,y,z)
static void check_paquete(const char *name, struct Paquete got, struct Paquete expected) {
    if (got.x == expected.x && got.y == expected.y && got.z == expected.z) {
        printf("[OK]   %s -> (%d,%d,%d)\n", name, got.x, got.y, got.z);
        tests_ok++;
    } else {
        printf("[FAIL] %s -> got (%d,%d,%d) expected (%d,%d,%d)\n",
               name, got.x, got.y, got.z, expected.x, expected.y, expected.z);
        tests_fail++;
    }
}

/* ------------------ test de concurrencia para ver que el mutex funciona ------------------ */

typedef struct {
    int id;       // id del hilo (solo para generar valores distintos)
    int result;   // aquí guardamos el resultado del set_value
} thread_arg_t;

// función que ejecuta cada hilo: intenta hacer set_value con la misma key ("race")
static void *thread_set_same_key(void *arg) {
    thread_arg_t *a = (thread_arg_t*)arg;

    // creo un value1 distinto según el hilo para que no sea todo igual
    char value1[64];
    snprintf(value1, sizeof(value1), "v%d", a->id);

    float vec[3] = { (float)a->id, 2.0f, 3.0f };
    struct Paquete p = { a->id, a->id+1, a->id+2 };

    // todos los hilos intentan meter la misma key a la vez
    a->result = set_value("race", value1, 3, vec, p);
    return NULL;
}

// test: lanzo 10 hilos y compruebo que solo uno consigue insertar (0) y los demás fallan (-1) porque la clave ya existe
static void test_concurrency_set_same_key(void) {
    destroy(); // dejo el almacén limpio antes de empezar

    const int T = 10;
    pthread_t th[T];
    thread_arg_t args[T];

    // creo hilos
    for (int i = 0; i < T; i++) {
        args[i].id = i;
        args[i].result = 999;
        pthread_create(&th[i], NULL, thread_set_same_key, &args[i]);
    }

    // espero a que terminen
    for (int i = 0; i < T; i++) pthread_join(th[i], NULL);

    // cuento cuántos han devuelto 0, -1 u otra cosa 
    int ok0 = 0, okm1 = 0, other = 0;
    for (int i = 0; i < T; i++) {
        if (args[i].result == 0) ok0++;
        else if (args[i].result == -1) okm1++;
        else other++;
    }

    // en local lo normal: 1 éxito y 9 fallos por duplicado
    if (ok0 == 1 && okm1 == 9 && other == 0) {
        printf("[OK]   concurrency set_value same key -> 1 success, 9 duplicate fails\n");
        tests_ok++;
    } else {
        printf("[FAIL] concurrency set_value same key -> successes=%d, -1=%d, other=%d\n",
               ok0, okm1, other);
        tests_fail++;
    }

    destroy(); // limpio después del test también
}

/* ------------------ tests principales ------------------ */

int main(void) {
    printf("==== TESTS: Parte A (local) / Parte B (mismo fichero) ====\n");

    /* 1) Tests válidos */

    check_int("destroy (empty)", destroy(), 0);

    float v1[3] = {1.0f, 2.0f, 3.0f};
    struct Paquete p1 = {1,2,3};

    float bigV[33];
    for (int i = 0; i < 33; i++) bigV[i] = (float)i;

    char longkey[400];
    memset(longkey, 'A', sizeof(longkey));
    longkey[399] = '\0';

    check_int("set_value k1", set_value("k1", "valor1", 3, v1, p1), 0);
    check_int("exist k1", exist("k1"), 1);

    // variables donde guardo lo que devuelve get_value
    char out_value1[MAX_STR];
    int outN = 0;
    float outV[MAX_V2];
    struct Paquete outP = {0,0,0};

    check_int("get_value k1", get_value("k1", out_value1, &outN, outV, &outP), 0);
    check_str("get_value.value1", out_value1, "valor1");
    check_int("get_value.N", outN, 3);
    check_float_vec("get_value.V", outV, v1, 3);
    check_paquete("get_value.P", outP, p1);

    // pruebo modify_value y vuelvo a hacer get para ver si cambió bien
    float v2[2] = {9.0f, 8.0f};
    struct Paquete p2 = {7,7,7};
    check_int("modify_value k1", modify_value("k1", "valorMOD", 2, v2, p2), 0);

    memset(out_value1, 0, sizeof(out_value1));
    outN = 0;
    memset(outV, 0, sizeof(outV));
    outP = (struct Paquete){0,0,0};

    check_int("get_value k1 (after modify)", get_value("k1", out_value1, &outN, outV, &outP), 0);
    check_str("get_value.value1 (after modify)", out_value1, "valorMOD");
    check_int("get_value.N (after modify)", outN, 2);
    check_float_vec("get_value.V (after modify)", outV, v2, 2);
    check_paquete("get_value.P (after modify)", outP, p2);

    check_int("delete_key k1", delete_key("k1"), 0);
    check_int("exist k1 (after delete)", exist("k1"), 0);
    check_int("destroy (end)", destroy(), 0);

        /* 2) Tests de errores */

    // duplicado
    check_int("set_value k2", set_value("k2", "v", 3, v1, p1), 0);
    check_int("set_value k2 duplicate", set_value("k2", "v", 3, v1, p1), -1);

    // clave inexistente
    check_int("get_value missing", get_value("noExiste", out_value1, &outN, outV, &outP), -1);
    check_int("modify_value missing", modify_value("noExiste", "x", 3, v1, p1), -1);
    check_int("delete_key missing", delete_key("noExiste"), -1);

    // punteros de salida nulos en get_value
    check_int("get_value value1=NULL", get_value("k2", NULL, &outN, outV, &outP), -1);
    check_int("get_value N_value2=NULL", get_value("k2", out_value1, NULL, outV, &outP), -1);
    check_int("get_value V_value2=NULL", get_value("k2", out_value1, &outN, NULL, &outP), -1);
    check_int("get_value value3=NULL", get_value("k2", out_value1, &outN, outV, NULL), -1);

    // punteros / parámetros inválidos en modify_value
    check_int("modify_value value1=NULL", modify_value("k2", NULL, 3, v1, p1), -1);
    check_int("modify_value N=0", modify_value("k2", "x", 0, v1, p1), -1);
    check_int("modify_value N=33", modify_value("k2", "x", 33, bigV, p1), -1);
    check_int("modify_value V=NULL", modify_value("k2", "x", 3, NULL, p1), -1);

    // N fuera de rango
    check_int("set_value N=0", set_value("k3", "v", 0, v1, p1), -1);
    check_int("set_value N=33", set_value("k4", "v", 33, bigV, p1), -1);

    // puntero NULL
    check_int("set_value V=NULL", set_value("k5", "v", 3, NULL, p1), -1);

    // exist / delete con NULL
    check_int("exist NULL", exist(NULL), -1);
    check_int("delete_key NULL", delete_key(NULL), -1);

    // clave vacía (decisión adicional de validación)
    check_int("set_value empty key", set_value("", "v", 3, v1, p1), -1);
    check_int("get_value empty key", get_value("", out_value1, &outN, outV, &outP), -1);
    check_int("modify_value empty key", modify_value("", "x", 3, v1, p1), -1);
    check_int("delete_key empty key", delete_key(""), -1);
    check_int("exist empty key", exist(""), -1);

    // string demasiado largo (más de 255)
    check_int("set_value long key", set_value(longkey, "v", 3, v1, p1), -1);
    check_int("get_value long key", get_value(longkey, out_value1, &outN, outV, &outP), -1);
    check_int("modify_value long key", modify_value(longkey, "x", 3, v1, p1), -1);
    check_int("exist long key", exist(longkey), -1);
    check_int("delete_key long key", delete_key(longkey), -1);
    
    destroy();
    
    /* 3) Test de concurrencia (para comprobar atomicidad) */
    test_concurrency_set_same_key();

    // resumen final
    printf("==== RESUMEN: OK=%d FAIL=%d ====\n", tests_ok, tests_fail);
    return (tests_fail == 0) ? 0 : 1;
}

/*----- TEST PARTE B: error de comunicación -----*/
/*
 * Este test debe ejecutarse con el servidor apagado.
 * No forma parte de la batería principal porque los tests normales
 * requieren que el servidor esté activo.
 * Comprueba que se devuelve -2 si el servidor no responde.
 */
void test_error_comunicacion_sin_servidor() {
    char value1[MAX_STR];
    int n = 0;
    float v[MAX_V2];
    struct Paquete p = {0,0,0};

    int r = get_value("clave_que_no_importa", value1, &n, v, &p);

    check_int("communication error (-2 expected)", r, -2);
}
