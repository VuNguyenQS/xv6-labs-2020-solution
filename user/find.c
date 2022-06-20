#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"


void find(char *path, char *fname) {
    //Read path find name
    //if see another path find again
    char buf[512], *p;
    int fd;
    struct stat st; 
    struct dirent de;

    if((fd = open(path, 0)) < 0){
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    if(fstat(fd, &st) < 0){
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("path too long\n");
      return;
    }

    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    //int i = 0;
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      //printf("%d \n", i++);
      if(de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      //printf("%s\n", de.name);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        printf("ls: cannot stat %s\n", buf);
        continue;
      }
      switch(st.type) {
        case T_FILE:
            if (strcmp(fname, de.name) == 0) {
                write(1, buf, strlen(buf));
                write(1, "\n", 1);
            }
            break;
        case T_DIR:
            find(buf, fname);
            break;
      }
    }
    close(fd);
}

int main(int argc, char *argv[]) {
  if (argc < 2 || argc > 3) {
    fprintf(2, "Usage: find dir filename or find filename...\n");
    exit(1);
  }

  char *path, *fname;
  if (argc == 2) {
    path =".";
    fname = argv[1];
  }
  else {
    path = argv[1];
    fname = argv[2];
  }

  struct stat st;
  if(stat(path, &st) < 0) {
    fprintf(2, "cannot stat %s\n", path);
    exit(1);
  }
  if (st.type != T_DIR) {
    fprintf(2, "%s is not a dir\n", path);
    exit(1);
  }
  find(path, fname);
  exit(0);
}