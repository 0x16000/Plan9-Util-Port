#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

static const char *argv0 = "cat";

void sysfatal(const char *fmt, const char *arg) {
    fprintf(stderr, "%s: ", argv0);
    fprintf(stderr, fmt, arg);
    fprintf(stderr, ": %s\n", strerror(errno));
    exit(1);
}

void cat(int fd, const char *name) {
    char buf[8192];
    ssize_t n;

    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        ssize_t written = 0;
        while (written < n) {
            ssize_t w = write(STDOUT_FILENO, buf + written, n - written);
            if (w < 0)
                sysfatal("write error copying %s", name);
            written += w;
        }
    }
    if (n < 0)
        sysfatal("error reading %s", name);
}

int main(int argc, char *argv[]) {
    int fd, i;

    argv0 = "cat";
    if (argc == 1) {
        cat(STDIN_FILENO, "<stdin>");
    } else {
        for (i = 1; i < argc; i++) {
            fd = open(argv[i], O_RDONLY);
            if (fd < 0)
                sysfatal("can't open %s", argv[i]);
            cat(fd, argv[i]);
            close(fd);
        }
    }
    return 0;
}
