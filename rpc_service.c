#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tirpc/rpc/rpc.h>

#include "claves.h"
#include "clavesRPC.h"

#define MAX_STR 256
#define MAX_V2 32

static char *duplicar_cadena(const char *s) {
    size_t n;
    char *copia;

    if (s == NULL) {
        return NULL;
    }

    n = strlen(s) + 1;
    copia = malloc(n);
    if (copia == NULL) {
        return NULL;
    }

    memcpy(copia, s, n);
    return copia;
}

static struct Paquete rpc_a_local_paquete(Paquete_rpc p) {
    struct Paquete out;
    out.x = p.x;
    out.y = p.y;
    out.z = p.z;
    return out;
}

bool_t destroy_1_svc(int *result, struct svc_req *rqstp) {
    (void)rqstp;

    if (result == NULL) {
        return FALSE;
    }

    *result = destroy();
    return TRUE;
}

bool_t set_value_1_svc(TuplaArg arg1, int *result, struct svc_req *rqstp) {
    struct Paquete p;

    (void)rqstp;

    if (result == NULL) {
        return FALSE;
    }

    if (arg1.key == NULL || arg1.value1 == NULL) {
        *result = -1;
        return TRUE;
    }

    if (arg1.N_value2 < 1 || arg1.N_value2 > MAX_V2) {
        *result = -1;
        return TRUE;
    }

    if (arg1.V_value2.V_value2_val == NULL ||
        arg1.V_value2.V_value2_len != (u_int)arg1.N_value2) {
        *result = -1;
        return TRUE;
    }

    p = rpc_a_local_paquete(arg1.value3);

    *result = set_value(
        arg1.key,
        arg1.value1,
        arg1.N_value2,
        arg1.V_value2.V_value2_val,
        p
    );

    return TRUE;
}

bool_t get_value_1_svc(KeyArg arg1, GetValueResult *result, struct svc_req *rqstp) {
    char value1[MAX_STR];
    int n_value2 = 0;
    float v_value2[MAX_V2];
    struct Paquete value3;
    int i;

    (void)rqstp;

    if (result == NULL) {
        return FALSE;
    }

    memset(result, 0, sizeof(*result));
    memset(value1, 0, sizeof(value1));
    memset(v_value2, 0, sizeof(v_value2));
    value3.x = 0;
    value3.y = 0;
    value3.z = 0;

    if (arg1.key == NULL) {
        result->status = -1;
        result->value1 = duplicar_cadena("");
        if (result->value1 == NULL) {
            return FALSE;
        }
        result->N_value2 = 0;
        result->V_value2.V_value2_len = 0;
        result->V_value2.V_value2_val = NULL;
        result->value3.x = 0;
        result->value3.y = 0;
        result->value3.z = 0;
        return TRUE;
    }

    result->status = get_value(arg1.key, value1, &n_value2, v_value2, &value3);

    if (result->status != 0) {
        result->value1 = duplicar_cadena("");
        if (result->value1 == NULL) {
            return FALSE;
        }

        result->N_value2 = 0;
        result->V_value2.V_value2_len = 0;
        result->V_value2.V_value2_val = NULL;
        result->value3.x = 0;
        result->value3.y = 0;
        result->value3.z = 0;
        return TRUE;
    }

    if (n_value2 < 1 || n_value2 > MAX_V2) {
        result->status = -1;
        result->value1 = duplicar_cadena("");
        if (result->value1 == NULL) {
            return FALSE;
        }

        result->N_value2 = 0;
        result->V_value2.V_value2_len = 0;
        result->V_value2.V_value2_val = NULL;
        result->value3.x = 0;
        result->value3.y = 0;
        result->value3.z = 0;
        return TRUE;
    }

    result->value1 = duplicar_cadena(value1);
    if (result->value1 == NULL) {
        result->status = -1;
        return FALSE;
    }

    result->N_value2 = n_value2;
    result->V_value2.V_value2_len = (u_int)n_value2;
    result->V_value2.V_value2_val = malloc((size_t)n_value2 * sizeof(float));

    if (result->V_value2.V_value2_val == NULL) {
        free(result->value1);
        result->value1 = NULL;
        result->status = -1;
        return FALSE;
    }

    for (i = 0; i < n_value2; i++) {
        result->V_value2.V_value2_val[i] = v_value2[i];
    }

    result->value3.x = value3.x;
    result->value3.y = value3.y;
    result->value3.z = value3.z;

    return TRUE;
}

bool_t modify_value_1_svc(TuplaArg arg1, int *result, struct svc_req *rqstp) {
    struct Paquete p;

    (void)rqstp;

    if (result == NULL) {
        return FALSE;
    }

    if (arg1.key == NULL || arg1.value1 == NULL) {
        *result = -1;
        return TRUE;
    }

    if (arg1.N_value2 < 1 || arg1.N_value2 > MAX_V2) {
        *result = -1;
        return TRUE;
    }

    if (arg1.V_value2.V_value2_val == NULL ||
        arg1.V_value2.V_value2_len != (u_int)arg1.N_value2) {
        *result = -1;
        return TRUE;
    }

    p = rpc_a_local_paquete(arg1.value3);

    *result = modify_value(
        arg1.key,
        arg1.value1,
        arg1.N_value2,
        arg1.V_value2.V_value2_val,
        p
    );

    return TRUE;
}

bool_t delete_key_1_svc(KeyArg arg1, int *result, struct svc_req *rqstp) {
    (void)rqstp;

    if (result == NULL) {
        return FALSE;
    }

    if (arg1.key == NULL) {
        *result = -1;
        return TRUE;
    }

    *result = delete_key(arg1.key);
    return TRUE;
}

bool_t exist_1_svc(KeyArg arg1, int *result, struct svc_req *rqstp) {
    (void)rqstp;

    if (result == NULL) {
        return FALSE;
    }

    if (arg1.key == NULL) {
        *result = -1;
        return TRUE;
    }

    *result = exist(arg1.key);
    return TRUE;
}

int claves_prog_1_freeresult(SVCXPRT *transp, xdrproc_t xdr_result, caddr_t result) {
    (void)transp;
    xdr_free(xdr_result, result);
    return 1;
}