#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>

typedef struct NDir NDir;
struct NDir {
    struct stat *d;
    char *name;
    char *prefix;
};

int errs = 0;
int dflag;
int lflag;
int mflag;
int nflag;
int pflag;
int qflag;
int Qflag;
int rflag;
int sflag;
int tflag;
int Tflag;
int uflag;
int Fflag;
int ndirbuf;
int ndir;
NDir *dirbuf;

int ls(char *, int);
int compar(NDir *, NDir *);
char *asciitime(time_t);
char *darwx(mode_t);
void rwx(mode_t, char *);
void growto(long);
void dowidths(struct stat *, const char *name);
void format(struct stat *db, const char *name, const char *display_name, FILE *out);
void output(void);
char *xcleanname(char *);

time_t clk;
int swidth;         /* max width of -s size */
int qwidth;         /* max width of -q version */
int vwidth;         /* max width of dev */
int uwidth;         /* max width of userid */
int mwidth;         /* max width of muid */
int lwidth;         /* max width of length */
int gwidth;         /* max width of groupid */

void usage(void) {
    fprintf(stderr, "usage: ls [-dlmnpqrstuFQT] [file ...]\n");
    exit(1);
}

int main(int argc, char *argv[]) {
    int i;
    int opt;

    /* Modified getopt handling to work with separate -d -l */
    while ((opt = getopt(argc, argv, "Fd:l:mnpqrstuQT")) != -1) {
        switch (opt) {
            case 'F': Fflag = 1; break;
            case 'd': dflag = 1; break;
            case 'l': lflag = 1; break;
            case 'm': mflag = 1; break;
            case 'n': nflag = 1; break;
            case 'p': pflag = 1; break;
            case 'q': qflag = 1; break;
            case 'Q': Qflag = 1; break;
            case 'r': rflag = 1; break;
            case 's': sflag = 1; break;
            case 't': tflag = 1; break;
            case 'T': Tflag = 1; break;
            case 'u': uflag = 1; break;
            case '?':
            default: usage();
        }
    }
    argc -= optind;
    argv += optind;

    if (lflag)
        clk = time(0);
    if (argc == 0)
        errs = ls(".", 0);
    else
        for (i = 0; i < argc; i++)
            errs |= ls(argv[i], argc > 1);
    output();
    return errs ? 1 : 0;
}

int ls(char *s, int multi) {
    DIR *dir;
    struct dirent *de;
    struct stat st;
    char *p;
    int n = 0;
    char path[4096];

    if (stat(s, &st) < 0) {
    error:
        fprintf(stderr, "ls: %s: %s\n", s, strerror(errno));
        return 1;
    }

    if (S_ISDIR(st.st_mode) && dflag == 0) {
        output();
        dir = opendir(s);
        if (dir == NULL)
            goto error;

        // First count the number of entries
        while ((de = readdir(dir)) != NULL)
            n++;
        rewinddir(dir);

        growto(ndir + n);
        n = 0;
        while ((de = readdir(dir)) != NULL) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
                continue;

            snprintf(path, sizeof(path), "%s/%s", s, de->d_name);
            dirbuf[ndir + n].d = malloc(sizeof(struct stat));
            if (stat(path, dirbuf[ndir + n].d) < 0) {
                free(dirbuf[ndir + n].d);
                continue;
            }
            dirbuf[ndir + n].name = strdup(de->d_name);
            dirbuf[ndir + n].prefix = multi ? strdup(s) : NULL;
            n++;
        }
        ndir += n;
        closedir(dir);
        output();
    } else {
        growto(ndir + 1);
        dirbuf[ndir].d = malloc(sizeof(struct stat));
        memcpy(dirbuf[ndir].d, &st, sizeof(struct stat));
        dirbuf[ndir].name = strdup(s);
        dirbuf[ndir].prefix = NULL;
        xcleanname(s);
        p = strrchr(s, '/');
        if (p) {
            dirbuf[ndir].prefix = strdup(s);
            *p = 0;
        }
        ndir++;
    }
    return 0;
}

void output(void) {
    int i;
    char buf[4096];
    char *s;

    if (!nflag)
        qsort(dirbuf, ndir, sizeof(dirbuf[0]), (int (*)(const void *, const void *))compar);
    for (i = 0; i < ndir; i++)
        dowidths(dirbuf[i].d, dirbuf[i].name);
    for (i = 0; i < ndir; i++) {
        if (!pflag && (s = dirbuf[i].prefix)) {
            if (strcmp(s, "/") == 0)  /* / is a special case */
                s = "";
            snprintf(buf, sizeof(buf), "%s/%s", s, dirbuf[i].name);
            format(dirbuf[i].d, buf, buf, stdout);
        } else
            format(dirbuf[i].d, dirbuf[i].name, dirbuf[i].name, stdout);
    }
    for (i = 0; i < ndir; i++) {
        free(dirbuf[i].d);
        free(dirbuf[i].name);
        if (dirbuf[i].prefix)
            free(dirbuf[i].prefix);
    }
    ndir = 0;
    fflush(stdout);
}

void dowidths(struct stat *db, const char *name) {
    char buf[256];
    int n;
    struct passwd *pw;
    struct group *gr;

    if (sflag) {
        n = snprintf(buf, sizeof(buf), "%lu", (db->st_size + 1023) / 1024);
        if (n > swidth)
            swidth = n;
    }
    if (qflag) {
        n = snprintf(buf, sizeof(buf), "%lu", (unsigned long)db->st_ino);
        if (n > qwidth)
            qwidth = n;
    }
    if (mflag) {
        pw = getpwuid(db->st_uid);
        n = snprintf(buf, sizeof(buf), "[%s]", pw ? pw->pw_name : "???");
        if (n > mwidth)
            mwidth = n;
    }
    if (lflag) {
        n = snprintf(buf, sizeof(buf), "%lu", (unsigned long)db->st_dev);
        if (n > vwidth)
            vwidth = n;
        pw = getpwuid(db->st_uid);
        n = snprintf(buf, sizeof(buf), "%s", pw ? pw->pw_name : "???");
        if (n > uwidth)
            uwidth = n;
        gr = getgrgid(db->st_gid);
        n = snprintf(buf, sizeof(buf), "%s", gr ? gr->gr_name : "???");
        if (n > gwidth)
            gwidth = n;
        n = snprintf(buf, sizeof(buf), "%lu", (unsigned long)db->st_size);
        if (n > lwidth)
            lwidth = n;
    }
}

char *fileflag(struct stat *db) {
    if (Fflag == 0)
        return "";
    if (S_ISDIR(db->st_mode))
        return "/";
    if (db->st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
        return "*";
    return "";
}

 void format(struct stat *db, const char *name, const char *display_name, FILE *out) {
    int i;
    struct passwd *pw;
    struct group *gr;
    char modestr[11];
    const char *type = "?";
    char uidbuf[32], gidbuf[32];

    if (S_ISREG(db->st_mode)) type = "-";
    if (S_ISDIR(db->st_mode)) type = "d";
    if (S_ISCHR(db->st_mode)) type = "c";
    if (S_ISBLK(db->st_mode)) type = "b";
    if (S_ISFIFO(db->st_mode)) type = "p";
    if (S_ISLNK(db->st_mode)) type = "l";
    if (S_ISSOCK(db->st_mode)) type = "s";

    rwx(db->st_mode, modestr);

    if (sflag)
        fprintf(out, "%*lu ", swidth, (db->st_size + 1023) / 1024);
    if (mflag) {
        pw = getpwuid(db->st_uid);
        fprintf(out, "[%s] ", pw ? pw->pw_name : "???");
        for (i = 2 + (pw ? strlen(pw->pw_name) : 3); i < mwidth; i++)
            fprintf(out, " ");
    }
    if (qflag)
        fprintf(out, "(%.16lx %*lu %.2x) ",
               (unsigned long)db->st_ino,
               qwidth, (unsigned long)db->st_ino,
               (unsigned)db->st_mode & S_IFMT);
    if (Tflag)
        fprintf(out, "%c ", (db->st_mode & S_ISVTX) ? 't' : '-');

    if (lflag) {
        pw = getpwuid(db->st_uid);
        gr = getgrgid(db->st_gid);
        snprintf(uidbuf, sizeof(uidbuf), "%s", pw ? pw->pw_name : "???");
        snprintf(gidbuf, sizeof(gidbuf), "%s", gr ? gr->gr_name : "???");
        fprintf(out, "%s%s %c %*lu %*s %*s %*lu %s ",
               type, modestr + 1, type[0],
               vwidth, (unsigned long)db->st_dev,
               -uwidth, uidbuf,
               -gwidth, gidbuf,
               lwidth, (unsigned long)db->st_size,
               asciitime(uflag ? db->st_atime : db->st_mtime));
    }
    fprintf(out, Qflag ? "%s%s\n" : "%s%s\n", display_name, fileflag(db));
}

void growto(long n) {
    if (n <= ndirbuf)
        return;
    ndirbuf = n;
    dirbuf = realloc(dirbuf, ndirbuf * sizeof(NDir));
    if (dirbuf == NULL) {
        fprintf(stderr, "ls: malloc fail\n");
        exit(1);
    }
}

int compar(NDir *a, NDir *b) {
    long i;
    struct stat *ad, *bd;

    ad = a->d;
    bd = b->d;

    if (tflag) {
        if (uflag)
            i = bd->st_atime - ad->st_atime;
        else
            i = bd->st_mtime - ad->st_mtime;
    } else {
        if (a->prefix && b->prefix) {
            i = strcmp(a->prefix, b->prefix);
            if (i == 0)
                i = strcmp(a->name, b->name);
        } else if (a->prefix) {
            i = strcmp(a->prefix, b->name);
            if (i == 0)
                i = 1;  /* a is longer than b */
        } else if (b->prefix) {
            i = strcmp(a->name, b->prefix);
            if (i == 0)
                i = -1; /* b is longer than a */
        } else
            i = strcmp(a->name, b->name);
    }
    if (i == 0)
        i = (a < b ? -1 : 1);
    if (rflag)
        i = -i;
    return i;
}

char *asciitime(time_t l) {
    static char buf[32];
    char *t;

    t = ctime(&l);
    /* 6 months in the past or a day in the future */
    if (l < clk - 180L * 24 * 60 * 60 || clk + 24L * 60 * 60 < l) {
        memmove(buf, t + 4, 7);      /* month and day */
        memmove(buf + 7, t + 20, 4); /* year */
    } else
        memmove(buf, t + 4, 12);     /* skip day of week */
    buf[12] = 0;
    return buf;
}

void rwx(mode_t mode, char *buf) {
    buf[0] = mode & S_IRUSR ? 'r' : '-';
    buf[1] = mode & S_IWUSR ? 'w' : '-';
    buf[2] = mode & S_IXUSR ? 'x' : '-';
    buf[3] = mode & S_IRGRP ? 'r' : '-';
    buf[4] = mode & S_IWGRP ? 'w' : '-';
    buf[5] = mode & S_IXGRP ? 'x' : '-';
    buf[6] = mode & S_IROTH ? 'r' : '-';
    buf[7] = mode & S_IWOTH ? 'w' : '-';
    buf[8] = mode & S_IXOTH ? 'x' : '-';
    buf[9] = ' ';  /* For Plan9 compatibility */
    buf[10] = '\0';
}

char *xcleanname(char *name) {
    char *r, *w;

    for (r = w = name; *r; r++) {
        if (*r == '/' && r > name && *(r - 1) == '/')
            continue;
        if (w != r)
            *w = *r;
        w++;
    }
    *w = 0;
    while (w - 1 > name && *(w - 1) == '/')
        *--w = 0;
    return name;
}