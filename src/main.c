#include <raylib.h>
#include <raymath.h>
#include <math.h>
#include <assert.h>

#define NOH_IMPLEMENTATION
#include "noh.h"
#include "hooks.h"

//#define NB_DEBUG_KEYPRESSES

typedef enum {
    NB_ShowKeyboard,
    NB_MainMenu
} NohBoard_View;

typedef struct {
    Vector2 screen_size;

    NohBoard_View view;
} NohBoard_State;

// The default font to use for menus.
Font nb_font;

void showKeyboard(Noh_Arena *arena, NohBoard_State *state, uint16 **pressed_keys, size_t num_pressed_keys) {
    if (IsKeyPressed(KEY_SCROLL_LOCK)) {
        state->view = NB_MainMenu;
        return;
    }

    // Change the background if any key is pressed.
    if (num_pressed_keys > 0) {
        static Color background_color = CLITERAL(Color){ 20, 20, 20, 255 };
        ClearBackground(background_color);

        uint16 key = *pressed_keys[0];
        noh_arena_save(arena);
        char *keyStr = noh_arena_sprintf(arena, "%hu", key);

        Vector2 text_offset = Vector2Scale(MeasureTextEx(nb_font, keyStr, 120, 0), 0.5);
        Vector2 pos = Vector2Subtract(Vector2Scale(state->screen_size, 0.5), text_offset);
        DrawTextEx(nb_font, keyStr, pos, 120, 0, WHITE);
    }

    Vector2 pos = { .x = 0, .y = 0 };
    DrawTextEx(nb_font, "Keyboard", pos, 24, 0, WHITE);

    return;
}

void mainMenu(Noh_Arena *arena, NohBoard_State *state) {
    (void)arena;
    if (IsKeyPressed(KEY_SCROLL_LOCK)) {
        state->view = NB_ShowKeyboard;
        return;
    }

    Vector2 pos = { .x = 0, .y = 0 };
    DrawTextEx(nb_font, "Main menu", pos, 24, 0, WHITE);

    return;
}

int main(void)
{
    Noh_Arena arena = {0};


    // Initial state.
    NohBoard_State state = { .screen_size = { .x = 800, .y = 600 }, .view = NB_MainMenu };

    const char *kb_path = "/dev/input/by-id/usb-Logitech_USB_Receiver-if02-event-kbd";
    //const char *kb_path = "/dev/input/by-path/platform-i8042-serio-0-event-kbd";
    const char *mouse_path = "/dev/input/by-id/usb-Logitech_USB_Receiver-if02-event-mouse";

    noh_arena_save(&arena);
    hooks_initialize(&arena, kb_path, mouse_path);
    noh_arena_rewind(&arena);

    SetTraceLogLevel(LOG_WARNING); 
    InitWindow(state.screen_size.x, state.screen_size.y, "NohBoard");
    SetWindowMonitor(GetCurrentMonitor()); // Not sure why Raylib initializes the window on a not current monitor.

    nb_font = LoadFontEx("./assets/Roboto-Regular.ttf", 120, 0, 0);
    SetExitKey(0);

    Noh_String str = {0};

    SetTargetFPS(60);
    while (!WindowShouldClose())
    {
        noh_arena_save(&arena);
        uint16 *pressed_keys;
        size_t num_pressed_keys = hooks_get_pressed_kb_keys(&arena, &pressed_keys);

#ifdef NB_DEBUG_KEYPRESSES
        bool any_pressed = num_pressed_keys > 0;
        if (any_pressed) {
            noh_string_append_cstr(&str, "- ");
            for (size_t i = 0; i < num_pressed_keys; i++) {
                char *line = noh_arena_sprintf(&arena, "%u - ", pressed_keys[i]);
                noh_string_append_cstr(&str, line);
            }
            noh_string_append_null(&str);
            noh_log(NOH_INFO, str.elems);
            noh_string_reset(&str);
        }
#endif

        noh_arena_rewind(&arena);

        BeginDrawing();
        ClearBackground(BLACK);
        switch (state.view) {
            case NB_MainMenu:
                mainMenu(&arena, &state);
                break;

            case NB_ShowKeyboard:
                showKeyboard(&arena, &state, &pressed_keys, num_pressed_keys);
                break;

            default:
                assert(false && "Invalid view.");
                break;
        }
        EndDrawing();
    }

    noh_arena_free(&arena);
    noh_string_free(&str);

    hooks_shutdown();

    CloseWindow();

    return 0;
}
