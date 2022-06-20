#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int parseargv(char line[], char *argv[], int k) {
    int i = 0, s = k;
    while (1) {
        while (line[i] == ' ')
            i++;
        if (line[i] == 0)
            break;
        if (k == MAXARG) {
            fprintf(2, "Too much args\n");
            exit(1);
        }
        argv[k++] = line + i;
        while (line[i] && line[i] != ' ')
            i++;
        if (line[i])
            line[i++] = 0;
        else
            break;
    }
    argv[k] = 0;
    return k - s;
}

int main(int argc, char *argv[]) {
    char buf[100];
    char *newargv[MAXARG];
    int k = 0;
    for (; argv[k+1]; k++)
        newargv[k] = argv[k+1];
    int i = 0;
    while (read(0, buf + i, 1)) {
        if (buf[i] == '\n') {
            buf[i] = 0;
            if (parseargv(buf, newargv, k)){
                if (fork() == 0)
                    exec(newargv[0], newargv);
                wait(0);
            }
            i = 0;
        }
        else i++;
    }
    buf[i] = 0;
    if (parseargv(buf, newargv, k)){
        if (fork() == 0)
            exec(newargv[0], newargv);
        wait(0);
    }
    exit(0);
}