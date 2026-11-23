#include <stdio.h>
#include <stdlib.h>

void glue_abortar(const char* msg) {
    fprintf(stderr, "Atención: %s\n", (msg && msg[0] != '\0') ? msg : "Unknown error");
    fflush(stderr);
    exit(1);
}

void glue_check_bounds(long long index, long long length) {
    if (index < 0 || index >= length) {
        char buf[200];
        snprintf(buf, sizeof(buf), "Index %lld out of bounds for length %lld", index, length);
        glue_abortar(buf);
    }
}

void glue_check_division_by_zero(int32_t divisor) {
    if (divisor == 0) {
        glue_abortar("Division by zero");
    }
}