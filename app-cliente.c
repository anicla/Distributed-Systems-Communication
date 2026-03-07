#include <stdio.h>
#include <string.h>
#include "claves.h"

int main(void) {
    struct Paquete p = {1, 2, 3};
    struct Paquete p_mod = {7, 7, 7};

    float v2[3] = {1.1f, 2.2f, 3.3f};
    float v2_mod[2] = {9.0f, 8.0f};

    char value1[256];
    int N = 0;
    float v2_out[32];
    struct Paquete p_out = {0, 0, 0};

    char cadena_300[300];
    float v_grande[50];

    memset(cadena_300, 'A', 299);
    cadena_300[299] = '\0';

    printf("=== DEMO APP-CLIENTE ===\n\n");

    // 1) Empezamos con estado limpio
    printf("destroy(): %d\n", destroy());

    // 2) Inserción
    printf("set_value(clave1): %d\n", set_value("clave1", "valor1", 3, v2, p));

    // 3) Comprobación de existencia
    printf("exist(clave1): %d\n", exist("clave1"));

    // 4) Lectura
    printf("get_value(clave1): %d\n", get_value("clave1", value1, &N, v2_out, &p_out));
    printf("value1 = %s\n", value1);
    printf("N = %d\n", N);
    printf("p_out = (%d,%d,%d)\n", p_out.x, p_out.y, p_out.z);

    // 5) Modificación
    printf("modify_value(clave1): %d\n", modify_value("clave1", "valorMOD", 2, v2_mod, p_mod));

    // 6) Nueva lectura tras modificar
    printf("get_value(clave1) tras modify: %d\n", get_value("clave1", value1, &N, v2_out, &p_out));
    printf("value1 = %s\n", value1);
    printf("N = %d\n", N);
    printf("p_out = (%d,%d,%d)\n", p_out.x, p_out.y, p_out.z);

    // 7) Borrado
    printf("delete_key(clave1): %d\n", delete_key("clave1"));
    printf("exist(clave1) tras delete: %d\n", exist("clave1"));

    // 8) Algunos errores esperados
    printf("\n--- CASOS DE ERROR ESPERADOS ---\n");
    printf("set_value duplicada: %d\n", set_value("clave2", "v", 3, v2, p));
    printf("set_value duplicada otra vez: %d\n", set_value("clave2", "v", 3, v2, p));
    printf("get_value(noExiste): %d\n", get_value("noExiste", value1, &N, v2_out, &p_out));
    printf("set_value(value1 > 255): %d\n", set_value("clave_err", cadena_300, 1, v2, p));
    printf("set_value(N > 32): %d\n", set_value("clave_err2", "valor", 50, v_grande, p));

    // 9) Limpieza final
    printf("\ndestroy(): %d\n", destroy());

    return 0;
}
