#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <time.h>
#include "claves.h"

// nº de operaciones que se van a ejecutar en el benchmark
#define N_OPS 10000

//calcula la diferencia entre dos instantes de tiempo y devuelve el resultado en milisegundos
static double diff_ms(struct timespec a, struct timespec b) {
    return (b.tv_sec - a.tv_sec) * 1000.0 +
           (b.tv_nsec - a.tv_nsec) / 1000000.0;
}

int main() {

    // variables para guardar los tiempos de inicio y fin
    struct timespec t1, t2;

    // datos ejemplo
    float v[1] = {1.0};
    struct Paquete p = {1,2,3};

    //sistema limpio
    destroy();

    set_value("bench", "valor", 1, v, p);

    // guardamos el instante inicial
    clock_gettime(CLOCK_MONOTONIC, &t1);

    // muchos exists() para ver el rendimiento
    for(int i=0;i<N_OPS;i++) {
        exist("bench");
    }

    //guardamos el instante final
    clock_gettime(CLOCK_MONOTONIC, &t2);

    //tiempo total
    double tiempo = diff_ms(t1,t2);

    printf("Tiempo total %d operaciones: %.2f ms\n", N_OPS, tiempo);
    printf("Tiempo medio por operacion: %.5f ms\n", tiempo/N_OPS);

    return 0;
}
