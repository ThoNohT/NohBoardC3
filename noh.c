#include <stdio.h>

#define NOH_IMPLEMENTATION
#include "src/noh.h"
#include "noh_bld.h"

int main() {
    Noh_String str = {0};
    Noh_Arena arena = noh_arena_init(4*1024);

    noh_string_append_cstr(&str, "Hello, ");
    noh_string_append_cstr(&str, "  W orld!     ");

    Noh_String_View sv = { .elems = str.elems, .count = str.count };

    Noh_String_View pre = noh_sv_chop_by_delim(&sv, ',');
    noh_sv_trim_space(&sv);

    const char * pre_str = noh_sv_to_arena_cstr(&arena, pre);
    const char * sv_str = noh_sv_to_arena_cstr(&arena, sv);

    printf("----\n");
    printf("'%s'\n", pre_str);
    printf("----\n");
    printf("'%s'\n", sv_str);
    printf("----\n");

    noh_string_reset(&str);
    noh_arena_free(&arena);

    return 0;
}


