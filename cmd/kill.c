#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

// Use system NSIG
// Make sure signm array size matches NSIG exactly (some Linux systems have 65 signals)

char *signm[] = {
    0,
    "SIGHUP", "SIGINT", "SIGQUIT", "SIGILL", "SIGABRT", "SIGFPE", "SIGKILL",  // 1-7
    "SIGSEGV", "SIGPIPE", "SIGALRM", "SIGTERM", "SIGUSR1", "SIGUSR2",         // 8-13
    "SIGCHLD", "SIGCONT", "SIGSTOP", "SIGTSTP", "SIGTTIN", "SIGTTOU",         // 14-19
    "SIGBUS", "SIGPOLL", "SIGPROF", "SIGSYS", "SIGTRAP", "SIGURG",             // 20-25
    "SIGVTALRM", "SIGXCPU", "SIGXFSZ", "SIGWINCH", "SIGIO", "SIGPWR",         // 26-31
    "SIGSYS"  // 32 (extra, remove if NSIG < 32)
};

// Calculate the number of valid signals in array
#define SIGMAX (sizeof(signm)/sizeof(signm[0]) - 1)

int main(int argc, char **argv)
{
    int signo, pid, res;
    int errlev = 0;

    if (argc <= 1) {
usage:
        fprintf(stderr, "usage: kill [ -sig ] pid ...\n");
        fprintf(stderr, "for a list of signals: kill -l\n");
        exit(2);
    }

    if (argv[1][0] == '-') {
        if (argv[1][1] == 'l' && argv[1][2] == '\0') {
            int i = 0;
            for (signo = 1; signo <= SIGMAX; signo++) {
                if (signm[signo]) {
                    printf("%s ", signm[signo]);
                    if (++i % 8 == 0)
                        printf("\n");
                }
            }
            if (i % 8 != 0)
                printf("\n");
            exit(0);
        } else if (isdigit((unsigned char)argv[1][1])) {
            signo = atoi(argv[1] + 1);
            if (signo <= 0 || signo > SIGMAX) {
                fprintf(stderr, "kill: %s: number out of range\n", argv[1]);
                exit(1);
            }
        } else {
            char *name = argv[1] + 1;
            for (signo = 1; signo <= SIGMAX; signo++) {
                if (signm[signo] && (!strcmp(signm[signo], name) || !strcmp(signm[signo] + 3, name))) {
                    goto foundsig;
                }
            }
            fprintf(stderr, "kill: %s: unknown signal; kill -l lists signals\n", name);
            exit(1);
        }
foundsig:
        argc--;
        argv++;
    } else {
        signo = SIGTERM;
    }

    argv++;
    while (argc > 1) {
        if ((**argv < '0' || **argv > '9') && **argv != '-')
            goto usage;

        pid = atoi(*argv);
        res = kill(pid, signo);
        if (res < 0) {
            perror("kill");
            errlev = 1;
        }

        argc--;
        argv++;
    }

    return errlev;
}