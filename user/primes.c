#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


void main(int argc, char *argv[]) {
    int p[2];
    int wp, n, prime, rp = -1;
    pipe(p);
    while (fork() == 0) {
        close(p[1]);
        rp = p[0];
        if (read(rp, &prime, 4) == 0)
            exit(0);
        printf("prime %d\n", prime);
        pipe(p);
        continue;
    }
    close(p[0]);
    wp = p[1];
    if (rp != -1) {
        while (read(rp, &n, 4) != 0) {
            if (n % prime != 0)
                write(wp, &n, 4);
        }
        close(rp);
    }
    else {
        for (int i = 2; i <= 35 ; i++)
            write(wp, &i, 4);
    }
    close(wp);
    wait(0);
    exit(0);
}