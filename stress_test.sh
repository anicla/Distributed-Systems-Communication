#!/bin/bash

N=20

echo "Lanzando $N clientes concurrentes..."

for i in $(seq 1 $N); do
    ./cliente_distribuido &
done

wait

echo "Todos los clientes terminaron."