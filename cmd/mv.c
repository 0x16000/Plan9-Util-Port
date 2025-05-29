#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <utime.h>
#include <libgen.h>

/* Plan 9 compatibility structures and functions */
typedef struct Dir {
    char *name;
    mode_t mode;
    time_t mtime;
    dev_t dev;
    ino_t ino;
    int isdir;
} Dir;

int copy1(int fdf, int fdt, char *from, char *to);
void hardremove(char *);
int mv(char *from, char *todir, char *toelem);
int mv1(char *from, Dir *dirb, char *todir, char *toelem);
int samefile(char *, char *);
void split(char *, char **, char **);
Dir *dirstat(char *path);
void cleanname(char *path);
char *utfrrune(char *s, int c);
void exits(char *msg);

void
main(int argc, char *argv[])
{
    int i, failed;
    Dir *dirto, *dirfrom;
    char *todir, *toelem;

    if(argc<3){
        fprintf(stderr, "usage: mv fromfile tofile\n");
        fprintf(stderr, "	  mv fromfile ... todir\n");
        exits("bad usage");
    }

    /* prepass to canonicalise names before splitting, etc. */
    for(i=1; i < argc; i++)
        cleanname(argv[i]);

    if((dirto = dirstat(argv[argc-1])) != NULL && dirto->isdir){
        dirfrom = NULL;
        if(argc == 3
        && (dirfrom = dirstat(argv[1])) != NULL
        && dirfrom->isdir) 
            split(argv[argc-1], &todir, &toelem); /* mv dir1 dir2 */
        else{                /* mv file... dir */
            todir = argv[argc-1];
            toelem = NULL;        /* toelem will be fromelem */
        }
        free(dirfrom);
    }else
        split(argv[argc-1], &todir, &toelem);    /* mv file1 file2 */
    free(dirto);
    if(argc>3 && toelem != NULL){
        fprintf(stderr, "mv: %s not a directory\n", argv[argc-1]);
        exits("bad usage");
    }

    failed = 0;
    for(i=1; i < argc-1; i++)
        if(mv(argv[i], todir, toelem) < 0)
            failed++;
    if(failed)
        exits("failure");
    exits(NULL);
}

int
mv(char *from, char *todir, char *toelem)
{
    int stat;
    Dir *dirb;

    dirb = dirstat(from);
    if(dirb == NULL){
        fprintf(stderr, "mv: can't stat %s: %s\n", from, strerror(errno));
        return -1;
    }
    stat = mv1(from, dirb, todir, toelem);
    free(dirb);
    return stat;
}

int
mv1(char *from, Dir *dirb, char *todir, char *toelem)
{
    int fdf, fdt, i, j, stat;
    char toname[4096], fromname[4096];
    char *fromdir, *fromelem;
    Dir *dirt;
    struct utimbuf ut;

    strncpy(fromname, from, sizeof fromname);
    fromname[sizeof fromname - 1] = '\0';
    split(from, &fromdir, &fromelem);
    if(toelem == NULL)
        toelem = fromelem;
    i = strlen(toelem);
    if(i==0){
        fprintf(stderr, "mv: null last name element moving %s\n", fromname);
        return -1;
    }
    j = strlen(todir);
    if(i + j + 2 > sizeof toname){
        fprintf(stderr, "mv: path too big (max %d): %s/%s\n",
            (int)sizeof toname, todir, toelem);
        return -1;
    }
    memmove(toname, todir, j);
    toname[j] = '/';
    memmove(toname+j+1, toelem, i);
    toname[i+j+1] = 0;

    if(samefile(fromdir, todir)){
        if(samefile(fromname, toname)){
            fprintf(stderr, "mv: %s and %s are the same\n",
                fromname, toname);
            return -1;
        }

        /* remove target if present */
        dirt = dirstat(toname);
        if(dirt != NULL) {
            hardremove(toname);
            free(dirt);
        }

        /* try rename */
        if(rename(fromname, toname) >= 0)
            return 0;
        if(dirb->isdir){
            fprintf(stderr, "mv: can't rename directory %s: %s\n",
                fromname, strerror(errno));
            return -1;
        }
    }
    /*
     * Renaming won't work --- must copy
     */
    if(dirb->isdir){
        fprintf(stderr, "mv: %s is a directory, not copied to %s\n",
            fromname, toname);
        return -1;
    }
    fdf = open(fromname, O_RDONLY);
    if(fdf < 0){
        fprintf(stderr, "mv: can't open %s: %s\n", fromname, strerror(errno));
        return -1;
    }

    dirt = dirstat(toname);
    if(dirt != NULL)
        hardremove(toname);  /* remove existing file */
    free(dirt);

    fdt = open(toname, O_WRONLY|O_CREAT|O_TRUNC, dirb->mode & 0777);
    if(fdt < 0){
        fprintf(stderr, "mv: can't create %s: %s\n", toname, strerror(errno));
        close(fdf);
        return -1;
    }
    stat = copy1(fdf, fdt, fromname, toname);
    close(fdf);

    if(stat >= 0){
        ut.actime = dirb->mtime;
        ut.modtime = dirb->mtime;
        utime(toname, &ut);    /* ignore errors */
        chmod(toname, dirb->mode);  /* ignore errors */
        if(unlink(fromname) < 0){
            fprintf(stderr, "mv: can't remove %s: %s\n", fromname, strerror(errno));
            stat = -1;
        }
    }
    close(fdt);
    return stat;
}

int
copy1(int fdf, int fdt, char *from, char *to)
{
    char buf[8192];
    ssize_t n, n1;

    while ((n = read(fdf, buf, sizeof buf)) > 0) {
        n1 = write(fdt, buf, n);
        if(n1 != n){
            fprintf(stderr, "mv: error writing %s: %s\n", to, strerror(errno));
            return -1;
        }
    }
    if(n < 0){
        fprintf(stderr, "mv: error reading %s: %s\n", from, strerror(errno));
        return -1;
    }
    return 0;
}

void
split(char *name, char **pdir, char **pelem)
{
    char *s;

    s = utfrrune(name, '/');
    if(s){
        *s = 0;
        *pelem = s+1;
        *pdir = name;
    }else if(strcmp(name, "..") == 0){
        *pdir = "..";
        *pelem = ".";
    }else{
        *pdir = ".";
        *pelem = name;
    }
}

int
samefile(char *a, char *b)
{
    struct stat da, db;
    
    if(strcmp(a, b) == 0)
        return 1;
    if(stat(a, &da) != 0 || stat(b, &db) != 0)
        return 0;
    return (da.st_dev == db.st_dev && da.st_ino == db.st_ino);
}

void
hardremove(char *a)
{
    if(unlink(a) == -1){
        fprintf(stderr, "mv: can't remove %s: %s\n", a, strerror(errno));
        exits("mv");
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
        
    d->name = strdup(basename(path));
    d->mode = st.st_mode;
    d->mtime = st.st_mtime;
    d->dev = st.st_dev;
    d->ino = st.st_ino;
    d->isdir = S_ISDIR(st.st_mode);
    
    return d;
}

void
cleanname(char *path)
{
    char *p, *q;
    int n;
    
    if(path[0] == '\0')
        return;
        
    /* Remove duplicate slashes */
    for(p = q = path; *p; p++) {
        if(*p == '/' && p[1] == '/')
            continue;
        *q++ = *p;
    }
    *q = '\0';
    
    /* Remove trailing slash except for root */
    n = strlen(path);
    if(n > 1 && path[n-1] == '/')
        path[n-1] = '\0';
}

char *
utfrrune(char *s, int c)
{
    return strrchr(s, c);
}

void
exits(char *msg)
{
    if(msg == NULL)
        exit(0);
    else
        exit(1);
}