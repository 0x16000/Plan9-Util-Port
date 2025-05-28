#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

char *e = NULL;
mode_t mode = 0777;

void usage(void) {
    fprintf(stderr, "usage: mkdir [-p] [-m mode] dir...\n");
    exit(EXIT_FAILURE);
}

int makedir(const char *s) {
    if (access(s, F_OK) == 0) {
        fprintf(stderr, "mkdir: %s already exists\n", s);
        e = "error";
        return -1;
    }
    if (mkdir(s, mode) < 0) {
        fprintf(stderr, "mkdir: can't create %s: %s\n", s, strerror(errno));
        e = "error";
        return -1;
    }
    return 0;
}

void mkdirp(char *s) {
    char *p;
    // We need a modifiable buffer, so work on a copy of s
    char *path = strdup(s);
    if (!path) {
        fprintf(stderr, "mkdir: memory allocation failed\n");
        e = "error";
        return;
    }

    for (p = strchr(path + 1, '/'); p; p = strchr(p + 1, '/')) {
        *p = 0;
        if (access(path, F_OK) != 0) {
            if (makedir(path) < 0) {
                free(path);
                return;
            }
        }
        *p = '/';
    }
    if (access(path, F_OK) != 0)
        makedir(path);

    free(path);
}

int main(int argc, char *argv[]) {
    int i;
    int pflag = 0;
    char *m = NULL;

    // Simple getopt-like argument parsing
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-p") == 0) {
                pflag = 1;
            } else if (strcmp(argv[i], "-m") == 0) {
                if (i + 1 >= argc)
                    usage();
                m = argv[++i];
                char *endptr;
                mode = strtoul(m, &endptr, 8);
                if (*endptr != '\0' || mode > 0777)
                    usage();
            } else {
                usage();
            }
        } else {
            break; // first non-option argument found
        }
    }

    // Now i points to the first directory argument
    if (i == argc)
        usage();

    for (; i < argc; i++) {
        if (pflag)
            mkdirp(argv[i]);
        else
            makedir(argv[i]);
    }
    exit(e ? EXIT_FAILURE : EXIT_SUCCESS);
}