#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "../../punto_h/claves.h"

int main(void) {
    pid_t pid = getpid(); //sacar el PID del proceso para generar claves únicas en cada ejecución del cliente y evitar que distintos clientesconcurrentes trabajen sobre las mismas claves.

    // buffers para guardar las distintas claves que se usan
    char key1[64];
    char key2[64];
    char key_err[64];
    char key_err2[64];
    char key_noexist[64];

    //incluir el PID en la clave para que cada cliente tenga clves distitntas
    snprintf(key1, sizeof(key1), "clave1_%d", pid);
    snprintf(key2, sizeof(key2), "clave2_%d", pid);
    snprintf(key_err, sizeof(key_err), "clave_err_%d", pid);
    snprintf(key_err2, sizeof(key_err2), "clave_err2_%d", pid);
    snprintf(key_noexist, sizeof(key_noexist), "noExiste_%d", pid);

    //estructuras ejemplo para insertar y modificar valores 
    struct Paquete p = {1, 2, 3};
    struct Paquete p_mod = {7, 7, 7};

    // vectores de floats para set_value y modify_value.
    float v2[3] = {1.1f, 2.2f, 3.3f};

    // sirve para comprobar que la modificación funciona
    float v2_mod[2] = {9.0f, 8.0f};

    char value1[256];
    int N = 0;
    float v2_out[32];
    struct Paquete p_out = {0, 0, 0};

    // variables para generar algunos casos de error
    char cadena_300[300];
    float v_grande[50];

    // para probar la validación: rellenamos la cadena larga con 'A'  de longitud de strings 
    memset(cadena_300, 'A', 299);
    cadena_300[299] = '\0';

    printf("=== DEMO APP-CLIENTE ===\n\n");

    // estado limpio
    printf("destroy(): %d\n", destroy());

    // insertar un valor
    printf("set_value(clave1): %d\n", set_value(key1, "valor1", 3, v2, p));

    // comprobar que existe la clave
    printf("exist(clave1): %d\n", exist(key1));

    // leerla
    printf("get_value(clave1): %d\n", get_value(key1, value1, &N, v2_out, &p_out));
    printf("value1 = %s\n", value1);
    printf("N = %d\n", N);
    printf("p_out = (%d,%d,%d)\n", p_out.x, p_out.y, p_out.z);

    // modificarla 
    printf("modify_value(clave1): %d\n", modify_value(key1, "valorMOD", 2, v2_mod, p_mod));

    // relectura tras la modificación 
    printf("get_value(clave1) tras modify: %d\n", get_value(key1, value1, &N, v2_out, &p_out));
    printf("value1 = %s\n", value1);
    printf("N = %d\n", N);
    printf("p_out = (%d,%d,%d)\n", p_out.x, p_out.y, p_out.z);

    // borrar la clave
    printf("delete_key(clave1): %d\n", delete_key(key1));
    printf("exist(clave1) tras delete: %d\n", exist(key1));

    // errores esperados
    printf("\n--- CASOS DE ERROR ESPERADOS ---\n");
    printf("set_value duplicada: %d\n", set_value(key2, "v", 3, v2, p));
    printf("set_value duplicada otra vez: %d\n", set_value(key2, "v", 3, v2, p));
    printf("get_value(noExiste): %d\n", get_value(key_noexist, value1, &N, v2_out, &p_out));
    printf("set_value(value1 > 255): %d\n", set_value(key_err, cadena_300, 1, v2, p));
    printf("set_value(N > 32): %d\n", set_value(key_err2, "valor", 50, v_grande, p));

    // limpieza final
    printf("\ndestroy(): %d\n", destroy());

    return 0;
}
