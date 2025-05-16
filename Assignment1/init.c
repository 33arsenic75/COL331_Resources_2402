// init: The initial user-level program

#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
// #include "login.h"

#define MAX_ATTEMPTS 3
#define MAX_SIZE 32
#define User_Name USERNAME
#define Password PASSWORD 

int login(){
    char user_input[32], pass_input[32];
    int attempts = 0;
    while(attempts < MAX_ATTEMPTS){
        printf(1, "$Enter username: ");
        gets(user_input, sizeof(user_input));
        user_input[strlen(user_input) - 1] = 0;
    if (strcmp(user_input, USERNAME) != 0) {
        attempts++;
        continue;
    }
    printf(1, "$Enter password: ");
    gets(pass_input, sizeof(pass_input));
    pass_input[strlen(pass_input) - 1] = 0;  // Remove newline

    if (strcmp(pass_input, PASSWORD) == 0) {
        printf(1, " Login successful\n");
        return 0;
    } else {
        attempts++;
    }
  }
  if (attempts == MAX_ATTEMPTS) {
        printf(1, "Login failed. System locked.\n");
        return 1;
  }
  return 2;
}

char *argv[] = { "sh", 0 };

int
main(void)
{
  int pid, wpid;

  if(open("console", O_RDWR) < 0){
    mknod("console", 1, 1);
    open("console", O_RDWR);
  }
  dup(0);  // stdout
  dup(0);  // stderr
  int login_value = login();
  if(login_value == 1){
    exit();
  }
  for(;;){
    printf(1, "init: starting sh\n");
    pid = fork();
    if(pid < 0){
      printf(1, "init: fork failed\n");
      exit();
    }
    if(pid == 0){
      exec("sh", argv);
      printf(1, "init: exec sh failed\n");
      exit();
    }
    while((wpid=wait()) >= 0 && wpid != pid)
      printf(1, "zombie!\n");
  }
}
