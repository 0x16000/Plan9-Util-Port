#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    long n;
    char *p, *q;

    if (argc > 1) {
        n = strtol(argv[1], &p, 0);
        // Sleep n seconds one by one
        while (n > 0) {
            sleep(1);
            n--;
        }

        // If there's a fractional part like ".123"
        if (*p == '.' && *(p+1) != '\0') {
            p++; // skip '.'
            n = strtol(p, &q, 10);
            if (n > 0) {
                // scale n depending on number of digits after decimal
                switch (q - p) {
                    case 0:
                        break;
                    case 1:
                        n *= 100;  // .1 = 100 ms
                        break;
                    case 2:
                        n *= 10;   // .12 = 120 ms
                        break;
                    default:
                        p[3] = '\0'; // truncate to 3 digits
                        n = strtol(p, NULL, 10);
                        break;
                }
                usleep(n * 1000); // sleep takes microseconds
            }
        }
    }
    return 0;
}