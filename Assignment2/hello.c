// #include "types.h"
// #include "stat.h"
// #include "user.h"
// int main(void)
// {
//     int pid = getpid(); // Get the process ID
//     printf(1, "Process ID: %d\n", pid);
//     while (1)
//     {
//         printf(1, "Hello");
//         sleep(100);
//     }
// }

#include "types.h"
#include "stat.h"
#include "user.h"

int fib(int n)
{
    if (n <= 0)
        return 0;
    if (n == 1)
        return 1;
    if (n == 2)
        return 1;
    return fib(n - 1) + fib(n - 2);
}

int main()
{
    int pid = fork();
    if (pid != 0)
    {
        while (1)
        {
            printf(1, "Hello , I am parent %d\n", pid);
            fib(35);
        }
    }
    else
    {
        while (1)
        {
            printf(1, "Hi there , I am child %d\n", pid);
            fib(35);
        }
    }
}

// #include "types.h"
// #include "user.h"

// void my_handler()
// {
//     printf(1, "SIGCUSTOM signal received!\n");
// }

// int main()
// {
//     signal(my_handler);
//     while (1)
//         ;
//     exit();
// }


// #include "types.h"
// #include "stat.h"
// #include "user.h"

// int fib(int n) {
//     if (n <= 0) return 0;
//     if (n == 1) return 1;
//     if (n == 2) return 1;
//     return fib(n - 1) + fib(n - 2);
// }

// void sv() {
//     printf(1, "I am Shivam\n");
// }

// void myHandler (){
//     printf(1, "I am inside the handler\n");
//     sv();
// }

// int main() {
//     signal(myHandler); // you need to implement this syscall for registering
//     // signal handlers;
//     while (1) {
//         printf(1, "This is normal code running\n");
//         fib (35); // doing CPU intensive work
//     }
// }