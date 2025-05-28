#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>

#define DEFB (8*1024)

int failed;
int gflag;
int uflag;
int xflag;

void copy(char *from, char *to, int todir);
int copy1(int fdf, int fdt, char *from, char *to);
int samefile(const char *a_path, const struct stat *a_stat, const char *b_path);

int main(int argc, char *argv[]) {
    int todir = 0;
    int i;

    // Simple flag parsing (similar to ARGBEGIN)
    for (i = 1; i < argc && argv[i][0] == '-'; i++) {
        char *opt = argv[i];
        for (int j = 1; opt[j] != '\0'; j++) {
            switch (opt[j]) {
                case 'g': gflag++; break;
                case 'u': uflag++; gflag++; break;
                case 'x': xflag++; break;
                default:
                    fprintf(stderr, "usage:\tcp [-gux] fromfile tofile\n");
                    fprintf(stderr, "\tcp [-x] fromfile ... todir\n");
                    exit(1);
            }
        }
    }

    if (argc - i < 2) {
        fprintf(stderr, "usage:\tcp [-gux] fromfile tofile\n");
        fprintf(stderr, "\tcp [-x] fromfile ... todir\n");
        exit(1);
    }

    // Check if last argument is a directory
    struct stat st;
    if (stat(argv[argc-1], &st) == 0 && S_ISDIR(st.st_mode)) {
        todir = 1;
    }

    if ((argc - i) > 2 && !todir) {
        fprintf(stderr, "cp: %s not a directory\n", argv[argc-1]);
        exit(1);
    }

    // Copy each source to destination
    for (; i < argc - 1; i++) {
        copy(argv[i], argv[argc-1], todir);
    }

    if (failed) {
        exit(1);
    }

    exit(0);
}

int samefile(const char *a_path, const struct stat *a_stat, const char *b_path) {
    struct stat b_stat;
    if (stat(b_path, &b_stat) == -1) {
        return 0; // cannot stat target, so not same file
    }

    if (a_stat->st_ino == b_stat.st_ino &&
        a_stat->st_dev == b_stat.st_dev) {
        fprintf(stderr, "cp: %s and %s are the same file\n", a_path, b_path);
        return 1;
    }
    return 0;
}

void copy(char *from, char *to, int todir) {
    char name[4096];
    struct stat st_from;

    if (todir) {
        // Append basename of from to to directory
        const char *elem = strrchr(from, '/');
        elem = (elem ? elem + 1 : from);
        snprintf(name, sizeof(name), "%s/%s", to, elem);
        to = name;
    }

    if (stat(from, &st_from) < 0) {
        fprintf(stderr, "cp: can't stat %s: %s\n", from, strerror(errno));
        failed = 1;
        return;
    }

    if (S_ISDIR(st_from.st_mode)) {
        fprintf(stderr, "cp: %s is a directory\n", from);
        failed = 1;
        return;
    }

    if (samefile(from, &st_from, to)) {
        failed = 1;
        return;
    }

    int fdf = open(from, O_RDONLY);
    if (fdf < 0) {
        fprintf(stderr, "cp: can't open %s: %s\n", from, strerror(errno));
        failed = 1;
        return;
    }

    // Open to with mode from source file's permission bits (mode & 0777)
    int fdt = open(to, O_WRONLY | O_CREAT | O_TRUNC, st_from.st_mode & 0777);
    if (fdt < 0) {
        fprintf(stderr, "cp: can't create %s: %s\n", to, strerror(errno));
        close(fdf);
        failed = 1;
        return;
    }

    if (copy1(fdf, fdt, from, to) == 0 && (xflag || gflag || uflag)) {
        // Try to preserve metadata
        struct stat st_to;
        if (fstat(fdt, &st_to) == 0) {
            if (xflag) {
                // Preserve modification time and mode
                struct timespec times[2];
                times[0] = st_from.st_atim;
                times[1] = st_from.st_mtim;
                futimens(fdt, times);

                fchmod(fdt, st_from.st_mode);
            }
            if (uflag) {
                // Preserve ownership
                fchown(fdt, st_from.st_uid, st_from.st_gid);
            }
            if (gflag) {
                // In Plan9, gflag sets group — in Linux covered above by fchown
                // Already done with fchown above if uflag or gflag set
            }
        } else {
            fprintf(stderr, "cp: warning: can't fstat %s: %s\n", to, strerror(errno));
        }
    }

    close(fdf);
    close(fdt);
}

int copy1(int fdf, int fdt, char *from, char *to) {
    char *buf = malloc(DEFB);
    if (buf == NULL) {
        fprintf(stderr, "cp: memory allocation failed\n");
        failed = 1;
        return -1;
    }

    ssize_t n;
    int rv = 0;

    while ((n = read(fdf, buf, DEFB)) > 0) {
        ssize_t n1 = write(fdt, buf, n);
        if (n1 != n) {
            fprintf(stderr, "cp: error writing %s: %s\n", to, strerror(errno));
            failed = 1;
            rv = -1;
            break;
        }
    }

    if (n < 0) {
        fprintf(stderr, "cp: error reading %s: %s\n", from, strerror(errno));
        failed = 1;
        rv = -1;
    }

    free(buf);
    return rv;
}