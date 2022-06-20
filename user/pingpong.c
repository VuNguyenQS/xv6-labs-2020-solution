#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[]) {
    char buf;
    int p[2];
    pipe(p);
    if(fork() == 0){
        if(read(p[0], &buf, 1) == 1) {
            printf("%d: received ping\n", getpid());
            write(p[1], "c", 1);
            exit(0);
        }
        exit(1);
    }
    else {
        write(p[1], "p", 1);
        wait(0);
        if (read(p[0], &buf, 1) == 1) {
            printf("%d: received pong\n", getpid());
            exit(0);
        }
        exit(1);
    }
}