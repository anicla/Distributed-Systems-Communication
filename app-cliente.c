#include <stdio.h>
#include "claves.h"

int main() {
    struct Paquete p = {1, 2, 3};
    float v2[3] = {1.1, 2.2, 3.3};

    printf("destroy(): %d\n", destroy());

    printf("set_value(): %d\n",
        set_value("clave1", "valor1", 3, v2, p));

    printf("exist(): %d\n", exist("clave1"));

    char value1[256];
    int N;
    float v2_out[32];
    struct Paquete p_out;

    printf("get_value(): %d\n",
        get_value("clave1", value1, &N, v2_out, &p_out));

    printf("value1 = %s\n", value1);
    printf("N = %d\n", N);
    printf("p_out = (%d,%d,%d)\n", p_out.x, p_out.y, p_out.z);

    printf("delete_key(): %d\n", delete_key("clave1"));
    printf("exist(): %d\n", exist("clave1"));

    printf("destroy(): %d\n", destroy());

    return 0;
}