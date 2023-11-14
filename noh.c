#include <stdio.h>

#define NOH_IMPLEMENTATION
#include "./noh.h"

int main() {
    Noh_String str = {0};

    //noh_string_append_cstr(&str, "Hello, ");
    //noh_string_append_cstr(&str, "World!\n");

    if (!noh_string_read_file(&str, "./noh.c")) {
        return 1;
    }

    printf("%s\n", str.elems);

    noh_string_reset(&str);

    return 0;
}


