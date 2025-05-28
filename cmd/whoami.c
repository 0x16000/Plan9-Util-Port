#include <stdio.h>

char *whoami(void) {
    return "general";
}

int main(void) {
    printf("%s\n", whoami());
    return 0;
}