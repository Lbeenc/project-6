# CS 4760 - Assignment 6: Memory Management with LRU

**Author**: Curtis Been  
**Date**: May 15, 2025

## Overview
This project simulates an operating system's memory management behavior, implementing Least Recently Used (LRU) page replacement. It replaces the previous resource management with memory requests, using shared memory and message queues for inter-process communication.

## Files
- `oss.c`: Master process (OSS) that simulates memory, handles user processes, manages the frame table, and simulates paging.
- `user_proc.c`: Child processes that generate memory address requests (read/write) and send them to `oss`.
- `Makefile`: Compiles both `oss` and `user_proc`.
- `oss.log`: Log file generated during simulation execution.

## Compilation
To compile all necessary programs:
```bash
make
```

To remove all binaries and logs:
```bash
make clean
```

## Execution
Run the simulator:
```bash
./oss
```

## Features
- Simulated shared clock.
- Message queues for IPC.
- Basic LRU page tracking logic (to be extended).
- Random address generation biased toward reads.

## Cleanup
Make sure to terminate all processes and remove IPC resources properly. Signal handlers are installed to clean up on `SIGINT`.
