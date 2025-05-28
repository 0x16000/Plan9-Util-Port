#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/stat.h>

// Helper to parse user or group string or numeric ID to uid/gid
// Returns 0 on success, -1 on failure (invalid user/group)
int parse_user(const char *name, uid_t *uid) {
    char *endptr;
    long val = strtol(name, &endptr, 10);
    if (*endptr == '\0') {
        // Numeric UID
        *uid = (uid_t)val;
        return 0;
    }
    struct passwd *pw = getpwnam(name);
    if (pw == NULL)
        return -1;
    *uid = pw->pw_uid;
    return 0;
}

int parse_group(const char *name, gid_t *gid) {
    char *endptr;
    long val = strtol(name, &endptr, 10);
    if (*endptr == '\0') {
        // Numeric GID
        *gid = (gid_t)val;
        return 0;
    }
    struct group *gr = getgrnam(name);
    if (gr == NULL)
        return -1;
    *gid = gr->gr_gid;
    return 0;
}

// Our chown function similar in spirit to Plan9 version:
// - path: file to change ownership of
// - owner: either numeric uid or string name (in Plan9 it was numeric only, but here we support strings)
// - group: either numeric gid or string name
// Returns 0 on success, -1 on error with errno set.
int my_chown(const char *path, const char *owner_str, const char *group_str) {
    uid_t uid;
    gid_t gid;

    if (parse_user(owner_str, &uid) < 0) {
        fprintf(stderr, "chown: invalid user: %s\n", owner_str);
        errno = EINVAL;
        return -1;
    }

    if (parse_group(group_str, &gid) < 0) {
        fprintf(stderr, "chown: invalid group: %s\n", group_str);
        errno = EINVAL;
        return -1;
    }

    if (chown(path, uid, gid) < 0) {
        perror("chown");
        return -1;
    }
    return 0;
}

// Example usage (can remove main for library usage)
int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "usage: chown user group file\n");
        return 1;
    }
    if (my_chown(argv[3], argv[1], argv[2]) < 0) {
        return 1;
    }
    return 0;
}
