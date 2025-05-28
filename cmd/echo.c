#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

int main(int argc, char *argv[])
{
    int nflag = 0;
    int i;
    size_t len = 1; // for null terminator or newline
    char *buf, *p;

    // Check for -n flag
    if (argc > 1 && strcmp(argv[1], "-n") == 0)
        nflag = 1;

    // Calculate total buffer length needed
    for (i = 1 + nflag; i < argc; i++)
        len += strlen(argv[i]) + 1;  // +1 for space or newline

    buf = malloc(len);
    if (buf == NULL) {
        fprintf(stderr, "echo: no memory\n");
        exit(1);
    }

    p = buf;
    for (i = 1 + nflag; i < argc; i++) {
        size_t arg_len = strlen(argv[i]);
        memcpy(p, argv[i], arg_len);
        p += arg_len;
        if (i < argc - 1)
            *p++ = ' ';
    }

    if (!nflag)
        *p++ = '\n';

    ssize_t w = write(STDOUT_FILENO, buf, p - buf);
    if (w < 0) {
        fprintf(stderr, "echo: write error: %s\n", strerror(errno));
        free(buf);
        exit(1);
    }

    free(buf);
    return 0;
}
