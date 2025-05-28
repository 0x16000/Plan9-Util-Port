#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

int main(int argc, char *argv[]) {
    char pathname[512];

    (void)argc;  // UNUSED
    (void)argv;

    if (getcwd(pathname, sizeof(pathname)) == NULL) {
        fprintf(stderr, "pwd: %s\n", strerror(errno));
        exit(1);
    }

    printf("%s\n", pathname);
    return 0;
}
