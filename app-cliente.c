#include <stdio.h>
#include <string.h>
#include "claves.h"

int main() {
    // --- VARIABLES CASO BASE ---
    struct Paquete p = {1, 2, 3};
    float v2[3] = {1.1, 2.2, 3.3};
    char value1[256];
    int N;
    float v2_out[32];
    struct Paquete p_out;

    printf("=== INICIO DE PRUEBAS COMPLETA ===\n\n");

    // ==========================================================
    // 0. COMPROBACIÓN DE PERSISTENCIA (EJECUCIÓN ANTERIOR)
    // ==========================================================
    // Si ejecutas el cliente por segunda vez, esto debería dar 1
    printf("Comprobando persistencia de ejecucion anterior: %d\n", exist("clave_persistente"));
    printf("\n");

    // ==========================================
    // 1. TU CASO BASE (SALIDA ORIGINAL)
    // ==========================================
    printf("destroy(): %d\n", destroy());

    printf("set_value(): %d\n", set_value("clave1", "valor1", 3, v2, p));

    printf("exist(): %d\n", exist("clave1"));

    printf("get_value(): %d\n", get_value("clave1", value1, &N, v2_out, &p_out));

    printf("value1 = %s\n", value1);
    printf("N = %d\n", N);
    printf("p_out = (%d,%d,%d)\n", p_out.x, p_out.y, p_out.z);

    printf("delete_key(): %d\n", delete_key("clave1"));
    printf("exist(): %d\n", exist("clave1"));

    printf("destroy(): %d\n", destroy());

    // ==========================================
    // 2. PRUEBAS DE ROBUSTEZ (LÍMITES)
    // ==========================================
    printf("\n--- PRUEBAS DE ROBUSTEZ (DEBEN DAR -1) ---\n");

    char cadena_300[300];
    memset(cadena_300, 'A', 299);
    cadena_300[299] = '\0';
    printf("set_value (cadena > 255): %d\n", set_value("clave_err", cadena_300, 1, v2, p));

    float v_grande[50];
    printf("set_value (N > 32): %d\n", set_value("clave_err", "valor", 50, v_grande, p));

    // ==========================================================
    // 3. PREPARACIÓN DE PERSISTENCIA (PARA LA PRÓXIMA VEZ)
    // ==========================================================
    // Dejamos esta clave en el servidor para que el próximo cliente la vea
    printf("\nGuardando dato para persistencia futura: %d\n",
            set_value("clave_persistente", "viva!", 1, v2, p));

    return 0;
}