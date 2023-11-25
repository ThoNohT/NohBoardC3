#include <stdio.h>

#define NOH_IMPLEMENTATION
#include "src/noh.h"
#include "noh_bld.h"

int main(int argc, char **argv) {
    Noh_Arena arena = noh_arena_init(4*1024);

    while (argc > 0) {
        char *arg = noh_shift_args(&argc, &argv);
        noh_log(NOH_INFO, "%s", arg);
    }

    return 0;


    noh_arena_save(&arena);
    char *s1 = noh_arena_sprintf(&arena, "Hello, World!");
    noh_arena_save(&arena);
    char *s2 = noh_arena_sprintf(&arena, "Hello, Sailor!");
    printf("1: %s\n", s1);
    printf("2: %s\n", s2);
    noh_arena_rewind(&arena);
    char *s3 = noh_arena_sprintf(&arena, "Hello, Something!");
    printf("1: %s\n", s1);
    printf("2: %s\n", s2);
    printf("3: %s\n", s3);

    noh_arena_rewind(&arena);


    // Noh_String str = {0};

    // noh_string_append_cstr(&str, "Hello, ");
    // noh_string_append_cstr(&str, "  W orld!     ");

    // Noh_String_View sv = { .elems = str.elems, .count = str.count };

    // Noh_String_View pre = noh_sv_chop_by_delim(&sv, ',');
    // noh_sv_trim_space(&sv);

    // const char * pre_str = noh_sv_to_arena_cstr(&arena, pre);
    // const char * sv_str = noh_sv_to_arena_cstr(&arena, sv);

    // printf("----\n");
    // printf("'%s'\n", pre_str);
    // printf("----\n");
    // printf("'%s'\n", sv_str);
    // printf("----\n");

    // noh_string_reset(&str);
    noh_arena_free(&arena);

    return 0;
}


