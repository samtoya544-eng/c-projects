#include <stdio.h>

int main(void) {
    FILE *f = fopen("index.html", "r");
    if (f == NULL) {
        perror("fopen");
        return 1;
    }

    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);

    printf("Read %zu bytes:\n%s", n, buf);
    return 0;
}
