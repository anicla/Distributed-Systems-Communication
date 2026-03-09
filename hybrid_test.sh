#!/bin/bash

echo "Lanzando carga híbrida: 100 clientes locales + 50 distribuidos"

# lanzar clientes locales
for i in $(seq 1 100); do
    ./cliente_local > /dev/null &
done

# lanzar clientes distribuidos
for i in $(seq 1 50); do
    ./cliente_distribuido > /dev/null &
done

# esperar a que terminen
wait

echo "Todos los clientes terminaron"