#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <fcntl.h>
#include <mntent.h>
#include <pwd.h>

#define MAX_LINE 1024
#define MAX_OUTPUT 4096

// Function to read a line from a file
char* read_file_line(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) return NULL;
    
    static char buffer[MAX_LINE];
    if (fgets(buffer, sizeof(buffer), file)) {
        // Remove newline
        char* newline = strchr(buffer, '\n');
        if (newline) *newline = '\0';
        fclose(file);
        return buffer;
    }
    fclose(file);
    return NULL;
}

// Function to execute command and get output
char* execute_command(const char* cmd) {
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return NULL;
    
    static char buffer[MAX_OUTPUT];
    buffer[0] = '\0';
    
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), pipe)) {
        strcat(buffer, line);
    }
    
    pclose(pipe);
    
    // Remove trailing newline
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len-1] == '\n') {
        buffer[len-1] = '\0';
    }
    
    return buffer[0] ? buffer : NULL;
}

// Get OS information
void get_os_info(char* os_name, char* arch) {
    struct utsname sys_info;
    if (uname(&sys_info) == 0) {
        // Check for specific distributions
        char* os_release = read_file_line("/etc/os-release");
        if (os_release && strstr(os_release, "NAME=")) {
            char* start = strstr(os_release, "NAME=\"");
            if (start) {
                start += 6; // Skip 'NAME="'
                char* end = strchr(start, '"');
                if (end) {
                    *end = '\0';
                    strcpy(os_name, start);
                } else {
                    strcpy(os_name, sys_info.sysname);
                }
            } else {
                strcpy(os_name, sys_info.sysname);
            }
        } else {
            strcpy(os_name, sys_info.sysname);
        }
        strcpy(arch, sys_info.machine);
    } else {
        strcpy(os_name, "Unknown");
        strcpy(arch, "Unknown");
    }
}

// Get CPU information
void get_cpu_info(char* cpu_info) {
    FILE* file = fopen("/proc/cpuinfo", "r");
    if (!file) {
        strcpy(cpu_info, "Unknown CPU");
        return;
    }
    
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "model name", 10) == 0) {
            char* colon = strchr(line, ':');
            if (colon) {
                colon += 2; // Skip ": "
                char* newline = strchr(colon, '\n');
                if (newline) *newline = '\0';
                strcpy(cpu_info, colon);
                fclose(file);
                return;
            }
        }
    }
    fclose(file);
    strcpy(cpu_info, "Unknown CPU");
}

// Get memory information
void get_memory_info(long* total_mb, long* used_mb, long* free_mb) {
    struct sysinfo sys_info;
    if (sysinfo(&sys_info) == 0) {
        *total_mb = sys_info.totalram / (1024 * 1024);
        *free_mb = sys_info.freeram / (1024 * 1024);
        *used_mb = *total_mb - *free_mb;
    } else {
        *total_mb = *used_mb = *free_mb = 0;
    }
}

// Get uptime
void get_uptime(char* uptime_str) {
    struct sysinfo sys_info;
    if (sysinfo(&sys_info) == 0) {
        long uptime = sys_info.uptime;
        long days = uptime / 86400;
        long hours = (uptime % 86400) / 3600;
        long minutes = (uptime % 3600) / 60;
        
        if (days > 0) {
            sprintf(uptime_str, "%ld days, %ld hours, %ld minutes", days, hours, minutes);
        } else if (hours > 0) {
            sprintf(uptime_str, "%ld hours, %ld minutes", hours, minutes);
        } else {
            sprintf(uptime_str, "%ld minutes", minutes);
        }
    } else {
        strcpy(uptime_str, "Unknown");
    }
}

// Get shell information
void get_shell_info(char* shell_info) {
    char* shell_env = getenv("SHELL");
    if (shell_env) {
        // Extract just the shell name
        char* shell_name = strrchr(shell_env, '/');
        if (shell_name) {
            strcpy(shell_info, shell_name + 1);
        } else {
            strcpy(shell_info, shell_env);
        }
    } else {
        strcpy(shell_info, "Unknown");
    }
}

// Get screen resolution
void get_resolution(char* resolution) {
    char* xdpyinfo_output = execute_command("xdpyinfo 2>/dev/null | grep dimensions");
    if (xdpyinfo_output && strstr(xdpyinfo_output, "dimensions:")) {
        char* start = strstr(xdpyinfo_output, "dimensions:");
        if (start) {
            start += 11; // Skip "dimensions:"
            while (*start == ' ') start++; // Skip spaces
            char* end = strchr(start, ' ');
            if (end) {
                *end = '\0';
                strcpy(resolution, start);
                return;
            }
        }
    }
    
    // Fallback: try to get from environment variables
    char* display = getenv("DISPLAY");
    if (display) {
        strcpy(resolution, "Unknown (X11 running)");
    } else {
        strcpy(resolution, "No display");
    }
}

// Get filesystem information
void get_filesystem_info(char* fs_info) {
    FILE* file = fopen("/proc/mounts", "r");
    if (!file) {
        strcpy(fs_info, "Unknown");
        return;
    }
    
    char filesystems[MAX_OUTPUT] = "";
    char line[MAX_LINE];
    char seen_fs[256][32];
    int fs_count = 0;
    
    while (fgets(line, sizeof(line), file)) {
        char device[256], mountpoint[256], fstype[64];
        if (sscanf(line, "%s %s %s", device, mountpoint, fstype) == 3) {
            // Skip common virtual filesystems
            if (strcmp(fstype, "proc") == 0 || strcmp(fstype, "sysfs") == 0 ||
                strcmp(fstype, "devtmpfs") == 0 || strcmp(fstype, "tmpfs") == 0 ||
                strcmp(fstype, "devpts") == 0 || strcmp(fstype, "securityfs") == 0 ||
                strcmp(fstype, "cgroup") == 0 || strcmp(fstype, "pstore") == 0 ||
                strcmp(fstype, "efivarfs") == 0 || strcmp(fstype, "bpf") == 0 ||
                strcmp(fstype, "cgroup2") == 0 || strcmp(fstype, "hugetlbfs") == 0 ||
                strcmp(fstype, "mqueue") == 0 || strcmp(fstype, "debugfs") == 0 ||
                strcmp(fstype, "tracefs") == 0 || strcmp(fstype, "fusectl") == 0 ||
                strcmp(fstype, "configfs") == 0) {
                continue;
            }
            
            // Check if we've already seen this filesystem type
            int already_seen = 0;
            for (int i = 0; i < fs_count; i++) {
                if (strcmp(seen_fs[i], fstype) == 0) {
                    already_seen = 1;
                    break;
                }
            }
            
            if (!already_seen && fs_count < 256) {
                strcpy(seen_fs[fs_count], fstype);
                fs_count++;
                
                if (strlen(filesystems) > 0) {
                    strcat(filesystems, ", ");
                }
                strcat(filesystems, fstype);
            }
        }
    }
    fclose(file);
    
    if (strlen(filesystems) > 0) {
        strcpy(fs_info, filesystems);
    } else {
        strcpy(fs_info, "Unknown");
    }
}

// Get storage information
void get_storage_info(char* storage_info) {
    FILE* file = fopen("/proc/partitions", "r");
    if (!file) {
        strcpy(storage_info, "Unknown storage");
        return;
    }
    
    char storage_output[MAX_OUTPUT] = "";
    char line[MAX_LINE];
    
    // Skip header lines
    fgets(line, sizeof(line), file);
    fgets(line, sizeof(line), file);
    
    while (fgets(line, sizeof(line), file)) {
        int major, minor;
        unsigned long blocks;
        char name[64];
        
        if (sscanf(line, "%d %d %lu %s", &major, &minor, &blocks, name) == 4) {
            // Only show main disks (not partitions)
            if (strstr(name, "sd") == name && strlen(name) == 3) { // sda, sdb, etc.
                double size_gb = (double)blocks * 1024.0 / (1024.0 * 1024.0 * 1024.0);
                
                if (strlen(storage_output) > 0) {
                    strcat(storage_output, "\n             ");
                }
                
                char disk_info[128];
                sprintf(disk_info, "%s: %.1fGB", name, size_gb);
                strcat(storage_output, disk_info);
            }
        }
    }
    fclose(file);
    
    if (strlen(storage_output) > 0) {
        strcpy(storage_info, storage_output);
    } else {
        strcpy(storage_info, "No disks found");
    }
}

int main() {
    char os_name[256], arch[64];
    char cpu_info[512];
    char shell_info[64];
    char uptime_str[128];
    char resolution[64];
    char fs_info[512];
    char storage_info[MAX_OUTPUT];
    long total_mb, used_mb, free_mb;
    
    // Get system hostname
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strcpy(hostname, "unknown");
    }
    
    // Get username
    char* username = getenv("USER");
    if (!username) {
        struct passwd* pw = getpwuid(getuid());
        username = pw ? pw->pw_name : "unknown";
    }
    
    // Gather system information
    get_os_info(os_name, arch);
    get_cpu_info(cpu_info);
    get_memory_info(&total_mb, &used_mb, &free_mb);
    get_uptime(uptime_str);
    get_shell_info(shell_info);
    get_resolution(resolution);
    get_filesystem_info(fs_info);
    get_storage_info(storage_info);
    
    // Print the system information with ASCII art
    printf("\n");
    printf("\n");
    printf("             %s@%s\n", username, hostname);
    printf("    (\\(\\     -----------\n");
    printf("   j\". ..    os: %s/%s\n", os_name, arch);
    printf("   (  . .)   shell: %s\n", shell_info);
    printf("   |   ° ¡   uptime: %s\n", uptime_str);
    printf("   ¿     ;   ram: %ld/%ld MiB\n", used_mb, total_mb);
    printf("   c?\".UJ    cpu: %s\n", cpu_info);
    printf("             resolution: %s\n", resolution);
    printf("             fs: %s\n", fs_info);
    printf("             %s\n", storage_info);
    
    return 0;
}
