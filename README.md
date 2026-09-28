# Sistemas distribuidos y mecanismos de comunicación

Proyecto desarrollado para la asignatura de Sistemas Distribuidos del Grado en Ingeniería Informática de la Universidad Carlos III de Madrid (UC3M).

El objetivo del proyecto es implementar y comparar diferentes mecanismos de comunicación entre procesos en un sistema distribuido utilizando C.

A lo largo del proyecto se desarrollaron distintas versiones del sistema utilizando colas de mensajes POSIX, sockets TCP y ONC RPC, permitiendo analizar las diferencias entre estos mecanismos de comunicación y su comportamiento en distintos escenarios.

## Implementaciones

El proyecto incluye tres mecanismos principales de comunicación:

### Colas de mensajes POSIX

Implementación de la comunicación entre cliente y servidor mediante colas de mensajes POSIX.

Esta versión permite estudiar un modelo de comunicación basado en intercambio de mensajes entre procesos.

### Sockets TCP

Implementación mediante sockets TCP siguiendo una arquitectura cliente-servidor.

Se desarrollaron tanto clientes como servidores y se trabajó con conexiones concurrentes para permitir la atención de múltiples solicitudes.

### ONC RPC

Implementación mediante ONC RPC (Remote Procedure Call), utilizando interfaces RPC para realizar llamadas a procedimientos remotos.

Esta alternativa permite abstraer parte de la comunicación de red y comparar su funcionamiento con las implementaciones realizadas directamente mediante sockets y colas de mensajes.

## Arquitectura

El proyecto incluye diferentes componentes para implementar y probar los mecanismos de comunicación:

- Clientes y servidores.
- Cliente concurrente.
- Proxies para los distintos mecanismos de comunicación.
- Servicio RPC.
- Pruebas de comunicación.
- Pruebas de carga y estrés.
- Benchmarks para comparar las implementaciones.

## Tecnologías utilizadas

- C
- POSIX Message Queues
- TCP/IP
- Sockets
- ONC RPC
- Concurrencia
- Make
- Shell scripting
- Linux

## Compilación

El proyecto incluye un `Makefile` para facilitar la compilación de los distintos componentes.

```bash
make
```

También se incluye un `Makefile.clavesRPC` específico para los componentes relacionados con ONC RPC.

## Pruebas

Se desarrollaron diferentes pruebas para comprobar el funcionamiento del sistema y evaluar las distintas implementaciones.

Entre ellas se incluyen:

- Pruebas de comunicación.
- Pruebas con múltiples clientes.
- Pruebas de estrés.
- Benchmarks de ejecución local y distribuida.

Los scripts `hybrid_test.sh` y `stress_test.sh` permiten automatizar parte de estas pruebas.

## Autores

Proyecto realizado conjuntamente por:

- Ana Claver Miranda
- David Paz Montoya

Grado en Ingeniería Informática  
Universidad Carlos III de Madrid (UC3M)
