#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <ctype.h>
#include <stdarg.h>

/* Plan 9 compatibility */
typedef struct Dir {
    char *name;
    mode_t mode;
    time_t mtime;
    time_t atime;
    off_t length;
    dev_t dev;
    ino_t ino;
    int isdir;
} Dir;

typedef struct String {
    char *s;
    int len;
    int alloc;
} String;

extern long long du(char*, Dir*);
extern void err(char*);
extern long long blkmultiple(long long);
extern int seen(Dir*);
extern int warn(char*);

enum {
    Vkilo = 1024LL,
};

/* rounding up, how many units does amt occupy? */
#define HOWMANY(amt, unit)    (((amt)+(unit)-1) / (unit))
#define ROUNDUP(amt, unit)    (HOWMANY(amt, unit) * (unit))
#define nelem(x) (sizeof(x)/sizeof((x)[0]))

int    aflag;
int    autoscale;
int    fflag;
int    fltflag;
int    qflag;
int    readflg;
int    sflag;
int    tflag;
int    uflag;

char    *fmt = "%llu\t%s\n";
char    *readbuf;
long long    blocksize = Vkilo;    /* actually more likely to be 4K or 8K */
long long    unit;            /* scale factor for output */

static char *pfxes[] = {    /* SI prefixes for units > 1 */
    "",
    "k", "M", "G",
    "T", "P", "E",
    "Z", "Y",
    NULL,
};

/* String functions */
String *s_new(void);
String *s_copy(char *s);
void s_append(String *s, char *append);
char *s_to_c(String *s);
void s_free(String *s);

/* Plan 9 compatibility functions */
Dir *dirstat(char *path);
int cistrcmp(char *a, char *b);
void exits(char *msg);
void sysfatal(char *fmt, ...);
char *needsrcquote(int c);
void quotefmtinstall(void);
long long dirval(Dir *d, long long size);
void readfile(char *name);
long long dufile(char *name, Dir *d);

/* Global for quote handling */
int doquote = 0;
char *(*needsrcquote_func)(int) = NULL;

void
usage(void)
{
    fprintf(stderr, "usage: du [-aefhnqstu] [-b size] [-p si-pfx] [file ...]\n");
    exits("usage");
}

void
printamt(long long amt, char *name)
{
    if (readflg)
        return;
    if (autoscale) {
        int scale = 0;
        double val = (double)amt/unit;

        while (fabs(val) >= 1024 && scale < (int)(nelem(pfxes)-1)) {
            scale++;
            val /= 1024;
        }
        printf("%.6g%s\t%s\n", val, pfxes[scale], name);
    } else if (fltflag)
        printf("%.6g\t%s\n", (double)amt/unit, name);
    else
        printf(fmt, HOWMANY(amt, unit), name);
}

int
main(int argc, char *argv[])
{
    int i, scale;
    char *s, *ss, *name;
    int arg_index = 1;

    doquote = 1;  /* Enable quoting by default */

    /* Simple argument parsing */
    while (arg_index < argc && argv[arg_index][0] == '-') {
        char *arg = argv[arg_index];
        if (strcmp(arg, "--") == 0) {
            arg_index++;
            break;
        }
        
        for (i = 1; arg[i]; i++) {
            switch (arg[i]) {
            case 'a':    /* all files */
                aflag = 1;
                break;
            case 'b':    /* block size */
                if (arg[i+1]) {
                    s = &arg[i+1];
                    i = strlen(arg) - 1;  /* skip to end */
                } else if (arg_index + 1 < argc) {
                    s = argv[++arg_index];
                } else {
                    usage();
                }
                if(s) {
                    blocksize = strtoul(s, &ss, 0);
                    if(s == ss)
                        blocksize = 1;
                    while(*ss++ == 'k')
                        blocksize *= 1024;
                }
                break;
            case 'e':    /* print in %g notation */
                fltflag = 1;
                break;
            case 'f':    /* don't print warnings */
                fflag = 1;
                break;
            case 'h':    /* similar to -h in bsd but more precise */
                autoscale = 1;
                break;
            case 'n':    /* all files, number of bytes */
                aflag = 1;
                blocksize = 1;
                unit = 1;
                break;
            case 'p':
                if (arg[i+1]) {
                    s = &arg[i+1];
                    i = strlen(arg) - 1;  /* skip to end */
                } else if (arg_index + 1 < argc) {
                    s = argv[++arg_index];
                } else {
                    usage();
                }
                if(s) {
                    for (scale = 0; pfxes[scale] != NULL; scale++)
                        if (cistrcmp(s, pfxes[scale]) == 0)
                            break;
                    if (pfxes[scale] == NULL)
                        sysfatal("unknown suffix %s", s);
                    unit = 1;
                    while (scale-- > 0)
                        unit *= Vkilo;
                }
                break;
            case 'q':    /* qid */
                fmt = "%llx\t%s\n";
                qflag = 1;
                break;
            case 'r':
                /* undocumented: just read & ignore every block of every file */
                readflg = 1;
                break;
            case 's':    /* only top level */
                sflag = 1;
                break;
            case 't':    /* return modified/accessed time */
                tflag = 1;
                break;
            case 'u':    /* accessed time */
                uflag = 1;
                break;
            default:
                usage();
            }
        }
        arg_index++;
    }

    if (unit == 0) {
        if (qflag || tflag || uflag || autoscale)
            unit = 1;
        else
            unit = Vkilo;
    }
    if (blocksize < 1)
        blocksize = 1;

    if (readflg) {
        readbuf = malloc(blocksize);
        if (readbuf == NULL)
            sysfatal("out of memory");
    }
    
    if(arg_index >= argc)
        printamt(du(".", dirstat(".")), ".");
    else
        for(i = arg_index; i < argc; i++) {
            name = argv[i];
            printamt(du(name, dirstat(name)), name);
        }
    exits(NULL);
}

long long
dirval(Dir *d, long long size)
{
    if(qflag)
        return d->ino;  /* Use inode as qid equivalent */
    else if(tflag) {
        if(uflag)
            return d->atime;
        return d->mtime;
    } else
        return size;
}

void
readfile(char *name)
{
    int n, fd = open(name, O_RDONLY);

    if(fd < 0) {
        warn(name);
        return;
    }
    while ((n = read(fd, readbuf, blocksize)) > 0)
        continue;
    if (n < 0)
        warn(name);
    close(fd);
}

long long
dufile(char *name, Dir *d)
{
    long long t = blkmultiple(d->length);

    if(aflag || readflg) {
        String *file = s_copy(name);

        s_append(file, "/");
        s_append(file, d->name);
        if (readflg)
            readfile(s_to_c(file));
        t = dirval(d, t);
        printamt(t, s_to_c(file));
        s_free(file);
    }
    return t;
}

long long
du(char *name, Dir *dir)
{
    DIR *dirp;
    struct dirent *entry;
    String *file;
    long long nk, t;
    Dir *d;

    if(dir == NULL)
        return warn(name);

    if(!dir->isdir)
        return dirval(dir, blkmultiple(dir->length));

    dirp = opendir(name);
    if(dirp == NULL)
        return warn(name);
    
    nk = 0;
    while((entry = readdir(dirp)) != NULL) {
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        file = s_copy(name);
        s_append(file, "/");
        s_append(file, entry->d_name);
        
        d = dirstat(s_to_c(file));
        if(d == NULL) {
            s_free(file);
            continue;
        }

        if(!d->isdir) {
            nk += dufile(name, d);
            free(d);
            s_free(file);
            continue;
        }

        if(seen(d)) {
            free(d);
            s_free(file);
            continue;    /* don't get stuck */
        }

        t = du(s_to_c(file), d);

        nk += t;
        t = dirval(d, t);
        if(!sflag)
            printamt(t, s_to_c(file));
        
        free(d);
        s_free(file);
    }
    closedir(dirp);
    return dirval(dir, nk);
}

#define    NCACHE    256    /* must be power of two */

typedef struct
{
    Dir*    cache;
    int    n;
    int    max;
} Cache;
Cache cache[NCACHE];

int
seen(Dir *dir)
{
    Dir *dp;
    int i;
    Cache *c;

    c = &cache[dir->ino&(NCACHE-1)];
    dp = c->cache;
    for(i=0; i<c->n; i++, dp++)
        if(dir->ino == dp->ino && dir->dev == dp->dev)
            return 1;
    if(c->n == c->max){
        if (c->max == 0)
            c->max = 8;
        else
            c->max += c->max/2;
        c->cache = realloc(c->cache, c->max*sizeof(Dir));
        if(c->cache == NULL)
            err("malloc failure");
    }
    c->cache[c->n++] = *dir;
    return 0;
}

void
err(char *s)
{
    fprintf(stderr, "du: %s: %s\n", s, strerror(errno));
    exits(s);
}

int
warn(char *s)
{
    if(fflag == 0)
        fprintf(stderr, "du: %s: %s\n", s, strerror(errno));
    return 0;
}

/* round up n to nearest block */
long long
blkmultiple(long long n)
{
    if(blocksize == 1)        /* no quantization */
        return n;
    return ROUNDUP(n, blocksize);
}

/* String functions */
String *
s_new(void)
{
    String *s = malloc(sizeof(String));
    if (!s) return NULL;
    s->s = malloc(1);
    if (!s->s) { free(s); return NULL; }
    s->s[0] = '\0';
    s->len = 0;
    s->alloc = 1;
    return s;
}

String *
s_copy(char *str)
{
    String *s = malloc(sizeof(String));
    if (!s) return NULL;
    s->len = strlen(str);
    s->alloc = s->len + 1;
    s->s = malloc(s->alloc);
    if (!s->s) { free(s); return NULL; }
    strcpy(s->s, str);
    return s;
}

void
s_append(String *s, char *append)
{
    int appendlen = strlen(append);
    int newlen = s->len + appendlen;
    
    if (newlen + 1 > s->alloc) {
        s->alloc = (newlen + 1) * 2;
        s->s = realloc(s->s, s->alloc);
        if (!s->s) sysfatal("out of memory");
    }
    strcpy(s->s + s->len, append);
    s->len = newlen;
}

char *
s_to_c(String *s)
{
    return s->s;
}

void
s_free(String *s)
{
    if (s) {
        free(s->s);
        free(s);
    }
}

/* Plan 9 compatibility functions */
Dir *
dirstat(char *path)
{
    struct stat st;
    Dir *d;
    
    if(stat(path, &st) != 0)
        return NULL;
        
    d = malloc(sizeof(Dir));
    if(d == NULL)
        return NULL;
        
    d->name = strdup(path);
    d->mode = st.st_mode;
    d->mtime = st.st_mtime;
    d->atime = st.st_atime;
    d->length = st.st_size;
    d->dev = st.st_dev;
    d->ino = st.st_ino;
    d->isdir = S_ISDIR(st.st_mode);
    
    return d;
}

int
cistrcmp(char *a, char *b)
{
    while (*a && *b) {
        int ca = tolower(*a);
        int cb = tolower(*b);
        if (ca != cb)
            return ca - cb;
        a++;
        b++;
    }
    return tolower(*a) - tolower(*b);
}

void
exits(char *msg)
{
    if(msg == NULL)
        exit(0);
    else
        exit(1);
}

void
sysfatal(char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "du: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);
}

char *
needsrcquote(int c)
{
    /* Simple implementation - just return the character as string if printable */
    static char buf[2];
    if (isprint(c) && c != ' ' && c != '\t') {
        buf[0] = c;
        buf[1] = '\0';
        return buf;
    }
    return NULL;
}

void
quotefmtinstall(void)
{
    /* Enable quote formatting */
    needsrcquote_func = needsrcquote;
}