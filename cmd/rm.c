#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>

char errbuf[512];
int ignerr = 0;

void err(const char *f) {
    if (!ignerr) {
        snprintf(errbuf, sizeof(errbuf), "%s: %s", f, strerror(errno));
        fprintf(stderr, "rm: %s\n", errbuf);
    }
}

/*
 * Recursively remove directory contents and then the directory itself
 */
void rmdir_recursive(const char *path) {
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    char *name;
    size_t len = strlen(path) + 1 + 256 + 1; // path + '/' + filename + null
    name = malloc(len);
    if (!name) {
        err("memory allocation");
        return;
    }

    dir = opendir(path);
    if (!dir) {
        err(path);
        free(name);
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(name, len, "%s/%s", path, entry->d_name);

        if (lstat(name, &st) == -1) {
            err(name);
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            // Recursively remove directory
            rmdir_recursive(name);
        } else {
            // Remove file or symlink
            if (remove(name) == -1)
                err(name);
        }
    }

    closedir(dir);

    // Remove the directory itself
    if (rmdir(path) == -1)
        err(path);

    free(name);
}

int main(int argc, char *argv[]) {
    int recurse = 0;
    int i;
    int opt;

    ignerr = 0;

    while ((opt = getopt(argc, argv, "rf")) != -1) {
        switch (opt) {
        case 'r':
            recurse = 1;
            break;
        case 'f':
            ignerr = 1;
            break;
        default:
            fprintf(stderr, "usage: rm [-fr] file ...\n");
            exit(1);
        }
    }

    if (optind == argc) {
        fprintf(stderr, "usage: rm [-fr] file ...\n");
        exit(1);
    }

    for (i = optind; i < argc; i++) {
        char *f = argv[i];
        struct stat st;

        if (remove(f) == 0)
            continue;

        if (recurse) {
            if (lstat(f, &st) == 0 && S_ISDIR(st.st_mode)) {
                rmdir_recursive(f);
                continue;
            }
        }

        err(f);
    }

    return ignerr ? 0 : (errbuf[0] == '\0' ? 0 : 1);
}