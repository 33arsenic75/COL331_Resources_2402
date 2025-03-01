#include "types.h"
#include "user.h"
#include "date.h"
#include "fcntl.h"
int main(int argc, char *argv[])
{
    int fd, n;
    char buf[512];
    fd = open("ls", O_RDWR);
    n = read(fd, buf, sizeof(buf));
    n = write(fd, buf, sizeof(buf));
    exec("ls", argv);
    close(fd);
    exit();
}
