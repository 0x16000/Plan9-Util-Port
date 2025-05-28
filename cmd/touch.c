#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

static unsigned long now;

void usage(void) {
    fprintf(stderr, "usage: touch [-c] [-t time] files\n");
    exit(1);
}

// Update file times to 'now'. If nocreate is set, don't create files.
int touch_file(int nocreate, const char *name) {
    struct stat st;
    struct timespec times[2];
    int fd;

    // Try to get current file info
    if (stat(name, &st) == 0) {
        // File exists — update times to 'now'
        times[0].tv_sec = now;     // access time
        times[0].tv_nsec = 0;
        times[1].tv_sec = now;     // modification time
        times[1].tv_nsec = 0;

        if (utimensat(AT_FDCWD, name, times, 0) < 0) {
            fprintf(stderr, "touch: %s: cannot update times: %s\n", name, strerror(errno));
            return 1;
        }
        return 0;
    }

    // File does not exist
    if (nocreate) {
        fprintf(stderr, "touch: %s: cannot wstat: %s\n", name, strerror(errno));
        return 1;
    }

    // Create the file with mode 0666
    fd = open(name, O_CREAT | O_EXCL | O_RDONLY, 0666);
    if (fd < 0) {
        fprintf(stderr, "touch: %s: cannot create: %s\n", name, strerror(errno));
        return 1;
    }

    // Update times on newly created file
    times[0].tv_sec = now;
    times[0].tv_nsec = 0;
    times[1].tv_sec = now;
    times[1].tv_nsec = 0;
    if (futimens(fd, times) < 0) {
        fprintf(stderr, "touch: %s: cannot update times: %s\n", name, strerror(errno));
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}

int main(int argc, char **argv) {
    int nocreate = 0;
    int status = 0;
    char *t = NULL;
    char *endptr;

    now = time(NULL);

    // Simple argument parsing like ARGBEGIN/ARGEND
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            char *opt = argv[i];
            for (int j = 1; opt[j]; j++) {
                switch (opt[j]) {
                    case 'c':
                        nocreate = 1;
                        break;
                    case 't':
                        if (opt[j+1] != '\0') {
                            // Time is attached to -t (like -t123456)
                            t = &opt[j+1];
                            j = strlen(opt) - 1;  // end inner loop
                        } else if (i + 1 < argc) {
                            // Time is next argument
                            t = argv[++i];
                        } else {
                            usage();
                        }
                        now = strtoul(t, &endptr, 0);
                        if (endptr == t || *endptr != '\0') {
                            usage();
                        }
                        j = strlen(opt) - 1; // end inner loop
                        break;
                    default:
                        usage();
                }
            }
        } else {
            // Found first filename argument
            // Process remaining args as filenames below
            break;
        }
    }

    // Find first filename argument
    int first_file_arg = 1;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            first_file_arg = i;
            break;
        }
    }

    if (first_file_arg >= argc) {
        usage();
    }

    // Process each file argument
    for (int i = first_file_arg; i < argc; i++) {
        status += touch_file(nocreate, argv[i]);
    }

    if (status) {
        exit(1);
    }
    return 0;
}