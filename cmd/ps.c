#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>

#define MAXARGS 16

// Simple tokenizer splitting by whitespace, modifies input string
int tokenize(char *str, char **argv, int max) {
    int i = 0;
    char *p = str;
    while (i < max) {
        while (*p && isspace((unsigned char)*p))
            *p++ = 0;
        if (!*p)
            break;
        argv[i++] = p;
        while (*p && !isspace((unsigned char)*p))
            p++;
    }
    return i;
}

void ps(char *pid) {
    char path[256];
    char status[4096];
    char args[256];
    char *argv[MAXARGS+1];
    int argc;
    int fd, n, i;

    unsigned long utime = 0, stime = 0, rtime = 0, rss_pages = 0, basepri = 0, pri = 0, size = 0;
    char comm[256] = {0}, state = '?';
    char rbuf[64] = "";
    char pbuf[32] = "";
    char rbuf1[64] = "";
    long page_size_kb = sysconf(_SC_PAGESIZE) / 1024;

    // Read /proc/[pid]/status for basic info
    snprintf(path, sizeof(path), "/proc/%s/status", pid);
    fd = open(path, O_RDONLY);
    if (fd < 0)
        return;
    n = read(fd, status, sizeof(status)-1);
    close(fd);
    if (n <= 0)
        return;
    status[n] = 0;

    // Parse key values from /proc/[pid]/status
    // We'll look for fields: Name, State, Uid, Gid, VmSize (virtual mem in kB), etc.
    // To keep it simple, parse line-by-line:
    char *line = status;
    char *end = status + n;
    while (line < end) {
        char *next = strchr(line, '\n');
        if (!next)
            break;
        *next = 0;

        if (strncmp(line, "Name:", 5) == 0) {
            sscanf(line+5, " %255s", comm);
        } else if (strncmp(line, "State:", 6) == 0) {
            sscanf(line+6, " %c", &state);
        } else if (strncmp(line, "Uid:", 4) == 0) {
            // We skip for now, could parse owner uid if needed
        } else if (strncmp(line, "VmSize:", 7) == 0) {
            sscanf(line+7, " %lu", &size); // in kB
        }

        line = next + 1;
    }

    // Read /proc/[pid]/stat for CPU times and other info
    snprintf(path, sizeof(path), "/proc/%s/stat", pid);
    fd = open(path, O_RDONLY);
    if (fd < 0)
        return;
    n = read(fd, status, sizeof(status)-1);
    close(fd);
    if (n <= 0)
        return;
    status[n] = 0;

    // Parse fields from /proc/[pid]/stat
    // Field details: https://man7.org/linux/man-pages/man5/proc.5.html
    // comm is 2nd field, can contain spaces inside parentheses
    // so carefully extract comm first, then tokenize remainder

    char *start = strchr(status, '(');
    char *endp = strrchr(status, ')');
    if (!start || !endp || endp < start)
        return;
    *endp = 0; // terminate comm string
    // comm is between '(' and ')'
    strncpy(comm, start+1, sizeof(comm)-1);

    // Tokenize fields after comm
    char *rest = endp + 2; // skip ") "
    // We want fields:
    // 14 utime - 13th after comm
    // 15 stime
    // 22 starttime (rtime calc skipped here)
    // 18 priority (basepri)
    // 19 nice (pri)

    unsigned long utime_ticks, stime_ticks;
    int priority, nice;

    // sscanf the rest fields (space-separated)
    // For simplicity, we can sscanf all needed fields with a big format string:
    // But safer to tokenize rest and pick fields.

    char *fields[64];
    int fcount = 0;
    char *p = rest;
    while (fcount < 64 && *p) {
        fields[fcount++] = p;
        char *space = strchr(p, ' ');
        if (!space)
            break;
        *space = 0;
        p = space + 1;
    }

    if (fcount >= 19) {
        utime_ticks = strtoul(fields[13], NULL, 10);
        stime_ticks = strtoul(fields[14], NULL, 10);
        priority = atoi(fields[17]);
        nice = atoi(fields[18]);
    } else {
        utime_ticks = 0;
        stime_ticks = 0;
        priority = 0;
        nice = 0;
    }

    // Convert ticks to seconds (usually sysconf(_SC_CLK_TCK))
    long ticks_per_sec = sysconf(_SC_CLK_TCK);
    if (ticks_per_sec <= 0) ticks_per_sec = 100; // fallback

    utime = utime_ticks / ticks_per_sec;
    stime = stime_ticks / ticks_per_sec;

    // Read RSS from /proc/[pid]/statm
    snprintf(path, sizeof(path), "/proc/%s/statm", pid);
    fd = open(path, O_RDONLY);
    if (fd >= 0) {
        char statm[64];
        int len = read(fd, statm, sizeof(statm) - 1);
        close(fd);
        if (len > 0) {
            statm[len] = 0;
            unsigned long size_pages = 0;
            if (sscanf(statm, "%lu %lu", &size_pages, &rss_pages) < 2) {
                rss_pages = 0;
            } else {
                rss_pages = rss_pages * page_size_kb;
            }
        }
    }

    // Compose pbuf (priority info)
    snprintf(pbuf, sizeof(pbuf), " %2d %2d", priority, nice);

    // Compose rbuf (runtime) - Linux doesn't provide easy runtime; skipping
    rbuf[0] = 0;
    rbuf1[0] = 0;

    // Print the info - mimic original ps output
    printf("%-10s %8s%s %4lu:%.2lu %3lu:%.2lu %s %7luK %-8.8s ",
           comm,
           pid,
           rbuf1,
           utime / 60, utime % 60,
           stime / 60, stime % 60,
           pbuf,
           rss_pages,
           state ? &state : "?");

    // Now print command line args from /proc/[pid]/cmdline
    snprintf(path, sizeof(path), "/proc/%s/cmdline", pid);
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        // fallback: print just comm
        printf("%s\n", comm);
        return;
    }
    n = read(fd, args, sizeof(args) - 1);
    close(fd);
    if (n <= 0) {
        printf("%s\n", comm);
        return;
    }
    args[n] = 0;
    // cmdline is \0-separated strings, replace \0 with space except last
    for (i = 0; i < n - 1; i++) {
        if (args[i] == 0)
            args[i] = ' ';
    }
    printf("%s\n", args);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <pid>\n", argv[0]);
        return 1;
    }
    ps(argv[1]);
    return 0;
}