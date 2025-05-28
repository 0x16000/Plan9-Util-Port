#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

int rmdir_custom(const char *path) {
    int ret = 0;
    if (rmdir(path) < 0) {
        fprintf(stderr, "rmdir: %s: %s\n", path, strerror(errno));
        ret = -1;
    }
    return ret;
}

int main(int argc, char *argv[]) {
    int status = 0;

    if (argc < 2) {
        fprintf(stderr, "usage: rmdir directory...\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (rmdir_custom(argv[i]) < 0)
            status = 1;
    }

    return status;
}
