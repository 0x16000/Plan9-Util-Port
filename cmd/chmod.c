#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

#define U(x) ((x)<<6)
#define G(x) ((x)<<3)
#define O(x) (x)
#define A(x) (U(x)|G(x)|O(x))

#define DMREAD  4
#define DMWRITE 2
#define DMEXEC  1
#define DMAPPEND  8
#define DMEXCL    16
#define DMTMP     32

#define DMRWE (DMREAD|DMWRITE|DMEXEC)

void usage(void) {
    fprintf(stderr, "usage: chmod 0777 file ... or chmod [who]op[rwxalt] file ...\n");
    exit(1);
}

int parsemode(char *spec, unsigned long *pmask, unsigned long *pmode);

int main(int argc, char *argv[]) {
    int i;
    struct stat st;
    unsigned long mode, mask;
    char *p;

    if (argc < 3) {
        usage();
    }

    mode = strtol(argv[1], &p, 8);
    if (*p == '\0') {
        // Numeric mode like 0777
        mask = A(DMRWE);
    } else if (!parsemode(argv[1], &mask, &mode)) {
        fprintf(stderr, "chmod: bad mode: %s\n", argv[1]);
        exit(1);
    }

    for (i = 2; i < argc; i++) {
        if (stat(argv[i], &st) < 0) {
            fprintf(stderr, "chmod: can't stat %s: %s\n", argv[i], strerror(errno));
            continue;
        }

        // Calculate new mode bits
        mode_t newmode = (st.st_mode & ~mask) | (mode & mask);

        if (chmod(argv[i], newmode) < 0) {
            fprintf(stderr, "chmod: can't chmod %s: %s\n", argv[i], strerror(errno));
            continue;
        }
    }

    return 0;
}

int parsemode(char *spec, unsigned long *pmask, unsigned long *pmode) {
    unsigned long mode = 0, mask = DMAPPEND | DMEXCL | DMTMP;
    int done = 0;
    char *s = spec;
    int op;

    while (!done) {
        switch (*s) {
            case 'u': mask |= U(DMRWE); s++; break;
            case 'g': mask |= G(DMRWE); s++; break;
            case 'o': mask |= O(DMRWE); s++; break;
            case 'a': mask |= A(DMRWE); s++; break;
            case '\0': return 0;
            default: done = 1; break;
        }
    }

    if (s == spec) {
        mask |= A(DMRWE);
    }

    op = *s++;
    if (op != '+' && op != '-' && op != '=') {
        return 0;
    }

    while (*s) {
        switch (*s) {
            case 'r': mode |= A(DMREAD); break;
            case 'w': mode |= A(DMWRITE); break;
            case 'x': mode |= A(DMEXEC); break;
            case 'a': mode |= DMAPPEND; break;
            case 'l': mode |= DMEXCL; break;
            case 't': mode |= DMTMP; break;
            default: return 0;
        }
        s++;
    }

    if (op == '+' || op == '-') {
        mask &= mode;
    }
    if (op == '-') {
        mode = ~mode;
    }
    *pmask = mask;
    *pmode = mode;
    return 1;
}