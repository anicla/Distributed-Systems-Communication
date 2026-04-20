struct Paquete_rpc {
    int x;
    int y;
    int z;
};

struct KeyArg {
    string key<256>;
};

struct TuplaArg {
    string key<256>;
    string value1<256>;
    int N_value2;
    float V_value2<32>;
    Paquete_rpc value3;
};

struct GetValueResult {
    int status;
    string value1<256>;
    int N_value2;
    float V_value2<32>;
    Paquete_rpc value3;
};

program CLAVES_PROG {
    version CLAVES_VERS {
        int DESTROY(void) = 1;
        int SET_VALUE(TuplaArg) = 2;
        GetValueResult GET_VALUE(KeyArg) = 3;
        int MODIFY_VALUE(TuplaArg) = 4;
        int DELETE_KEY(KeyArg) = 5;
        int EXIST(KeyArg) = 6;
    } = 1;
} = 0x20000001;