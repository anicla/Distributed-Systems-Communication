/* Definición del protocolo de tuplas basado en claves.h */

struct Paquete_rpc {
    int x;
    int y;
    int z;
};

struct Tupla_rpc {
    string key<256>;        /* Clave */
    string value1<256>;     /* Valor 1 */
    int n_value2;           /* Dimensión de v_value2 */
    float v_value2[32];     /* Vector de floats (tamaño fijo) */
    struct Paquete_rpc value3;
};

/* Estructura para devolver los múltiples valores de get_value */
struct get_res {
    int status;             /* 0 éxito, -1 error */
    char value1[256];
    int n_value2;
    float v_value2[32];
    struct Paquete_rpc value3;
};

program CLAVES_PROG {
    version CLAVES_VERS {
        int DESTROY(void) = 1;
        int SET_VALUE(struct Tupla_rpc) = 2;
        struct get_res GET_VALUE(string) = 3;
        int MODIFY_VALUE(struct Tupla_rpc) = 4;
        int DELETE_KEY(string) = 5;
        int EXIST(string) = 6;
    } = 1;
} = 0x20000001;