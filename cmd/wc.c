#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <locale.h>
#include <wctype.h>
#include <string.h>
#include <errno.h>

typedef unsigned long long uvlong;

static int pline, pword, prune, pbadr, pchar;
static uvlong nline, nword, nrune, nbadr, nchar;
static uvlong tnline, tnword, tnrune, tnbadr, tnchar;

enum { Space, Word };

static void
wc(FILE *f)
{
    int where = Space;
    wint_t r;

    nline = nword = nrune = nbadr = 0;

    while ((r = fgetwc(f)) != WEOF) {
        nrune++;
        if (r == L'\n')
            nline++;
        if (where == Word) {
            if (iswspace(r))
                where = Space;
        } else {
            if (!iswspace(r)) {
                where = Word;
                nword++;
            }
        }
    }

    if (ferror(f)) {
        perror("read");
        // Count bytes only if read succeeded fully
    }

    // Get byte offset: seek to end, ftell
    if (fseek(f, 0, SEEK_END) == 0) {
        long off = ftell(f);
        if (off >= 0)
            nchar = (uvlong)off;
        else
            nchar = 0;
    } else {
        nchar = 0;
    }

    tnline += nline;
    tnword += nword;
    tnrune += nrune;
    tnbadr += nbadr;
    tnchar += nchar;
}

static void
report(uvlong nline, uvlong nword, uvlong nrune, uvlong nbadr, uvlong nchar, const char *fname)
{
    char line[1024];
    char *s = line;
    size_t e = sizeof line;
    int len;

    s[0] = 0;

    if (pline) {
        len = snprintf(s, e, " %7llu", nline);
        s += len; e -= len;
    }
    if (pword) {
        len = snprintf(s, e, " %7llu", nword);
        s += len; e -= len;
    }
    if (prune) {
        len = snprintf(s, e, " %7llu", nrune);
        s += len; e -= len;
    }
    if (pbadr) {
        len = snprintf(s, e, " %7llu", nbadr);
        s += len; e -= len;
    }
    if (pchar) {
        len = snprintf(s, e, " %7llu", nchar);
        s += len; e -= len;
    }
    if (fname != NULL) {
        len = snprintf(s, e, " %s", fname);
        s += len; e -= len;
    }
    printf("%s\n", line + 1);  // skip leading space
}

int
main(int argc, char *argv[])
{
    char *sts = NULL;
    int i;

    setlocale(LC_ALL, "");

    // Parse flags -lwrbc
    for (i = 1; i < argc && argv[i][0] == '-' && argv[i][1] != '\0'; i++) {
        char *p = &argv[i][1];
        while (*p) {
            switch (*p) {
                case 'l': pline++; break;
                case 'w': pword++; break;
                case 'r': prune++; break;
                case 'b': pbadr++; break;
                case 'c': pchar++; break;
                default:
                    fprintf(stderr, "Usage: %s [-lwrbc] [file ...]\n", argv[0]);
                    exit(1);
            }
            p++;
        }
    }

    if (pline + pword + prune + pbadr + pchar == 0) {
        pline = 1;
        pword = 1;
        pchar = 1;
    }

    if (i == argc) {
        // No file arguments, read stdin
        wc(stdin);
        report(nline, nword, nrune, nbadr, nchar, NULL);
    } else {
        for (; i < argc; i++) {
            FILE *f = fopen(argv[i], "r");
            if (f == NULL) {
                perror(argv[i]);
                sts = "can't open";
                continue;
            }
            wc(f);
            report(nline, nword, nrune, nbadr, nchar, argv[i]);
            fclose(f);
        }
        if (argc - i + 1 > 1)  // If multiple files, print total
            report(tnline, tnword, tnrune, tnbadr, tnchar, "total");
    }

    return sts ? 1 : 0;
}