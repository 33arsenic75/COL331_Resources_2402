// #include "kernel/types.h"
// #include "kernel/stat.h"
// #include "user/user.h"

#include "types.h"
#include "stat.h"
#include "user.h"

#ifndef ALPHA
#define ALPHA 1
#endif

#ifndef BETA
#define BETA 13
#endif

long IO_LIM = 500000000;
long CPU_LIM = 500000000;

void io_bound_process()
{
    printf(1, "IO Process PID %d\n", getpid()); // Use getpid() instead of myproc()->pid
    for (int i = 0; i < IO_LIM; i++)
    {
        printf(1, "");
        // sleep(1);  // Simulate I/O wait
    }
    printf(1, "IO-bound process finished.\n");
    exit();
}

void cpu_bound_process()
{
    printf(1, "CPU Process PID %d\n", getpid()); // Use getpid()
    volatile unsigned long counter = 0;
    for (unsigned long i = 0; i < CPU_LIM; i++)
    {
        counter++; // Simulate CPU work
    }
    printf(1, "CPU-bound process finished.\n");
    exit();
}

int main()
{
    printf(1, "ALPHA = %d\nBETA = %d\n", ALPHA, BETA);
    int pid1 = fork();

    if (pid1 == 0)
    {
        // Child 1: I/O-bound process
        io_bound_process();
    }

    int pid2 = fork();

    if (pid2 == 0)
    {
        // Child 2: CPU-bound process
        cpu_bound_process();
    }

    // Parent waits for both children
    wait();
    wait();

    printf(1, "Parent process exiting.\n");
    exit();
}
