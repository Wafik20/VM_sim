# VM Simulation

## Description
VM Simulation is a C program to simulate a virtual memory system using various page replacement algorithms, including OPT, NRU, and CLOCK. The program handles memory management tasks such as page allocation, replacement, and tracking access statistics, providing a platform for understanding and analyzing the efficiency of different algorithms in handling page faults.

## Features
- Simulates virtual memory management.
- Supports OPT (Optimal), NRU (Not Recently Used), and CLOCK page replacement algorithms.
- Configurable number of memory frames and refresh rates.
- Detailed statistics reporting including total accesses, page faults, and writes to disk.

## Installation
To compile the program, use the following GCC command:
```bash
gcc -o vmsim vmsim.c
