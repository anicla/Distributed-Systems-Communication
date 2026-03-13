#include <stdio.h>
#include "claves.h"

int main(void) {
    float v[1] = {1.0f};
    struct Paquete p = {1, 2, 3};

    // intentamos ejecutar set_value sin tener el servidor levantado
    printf("set_value sin servidor: %d\n",
           set_value("k", "v", 1, v, p));

    return 0;

}
