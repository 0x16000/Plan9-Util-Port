#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int uflg = 0, nflg = 0;

void usage(const char *prog) {
    fprintf(stderr, "usage: %s [-un] [seconds]\n", prog);
    exit(1);
}

int main(int argc, char *argv[]) {
    unsigned long now;
    int opt;

    // Parse options -u and -n
    while ((opt = getopt(argc, argv, "un")) != -1) {
        switch (opt) {
            case 'u':
                uflg = 1;
                break;
            case 'n':
                nflg = 1;
                break;
            default:
                usage(argv[0]);
        }
    }

    argc -= optind;
    argv += optind;

    if (argc == 1) {
        char *endptr;
        now = strtoul(argv[0], &endptr, 0);
        if (*endptr != '\0') {
            usage(argv[0]);
        }
    } else if (argc == 0) {
        now = (unsigned long)time(NULL);
    } else {
        usage(argv[0]);
    }

    if (nflg) {
        printf("%lu\n", now);
    } else if (uflg) {
        time_t t = (time_t)now;
        struct tm *gmt = gmtime(&t);
        if (!gmt) {
            fprintf(stderr, "%s: gmtime failed\n", argv[0]);
            exit(1);
        }
        // asctime adds a newline at the end
        printf("%s", asctime(gmt));
    } else {
        time_t t = (time_t)now;
        // ctime adds a newline at the end
        printf("%s", ctime(&t));
    }

    return 0;
}
