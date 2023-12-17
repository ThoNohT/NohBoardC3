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
} NB_View;

typedef struct {
    Vector2 screen_size;
    NB_View view;
    bool running;
} NB_State;

typedef enum {
    NB_Align_Left,
    NB_Align_Center,
    NB_Align_Right
} NB_Align;

// The default font to use for menus.
Font nb_font;

#define CONTROL_COLOR CLITERAL(Color){ 58, 60, 74, 255 }
#define CONTROL_COLOR_HL CLITERAL(Color){ 86, 90, 100, 255 }
#define CONTROL_EDGE_COLOR CLITERAL(Color){ 116, 120, 133, 255 }
#define CONTROL_FG_COLOR CLITERAL(Color) { 192, 192, 213, 255 }

Rectangle rec_from_vec2s(Vector2 position, Vector2 size) {
    Rectangle rec = { .x = position.x, .y = position.y, .width = size.x, .height = size.y };
    return rec;
}

// Calculates the position and font size needed to render text inside certain bounds designated by a position and size.
// The provided font size is taken to be the maximum font size. If needed, it will be reduced, but never increased.
// The provided position should point to the top left center of the rectangle into which to render the text. It will
// be updated to the position at which the text should be rendered to fit into the bounds with the specified alignment.
void calculate_text_bounds(char *text, Font font, float *font_size, NB_Align align, Vector2 *position, Vector2 size) {
    Vector2 text_size = MeasureTextEx(nb_font, text, *font_size, 0);

    // Determine the needed font size to ensure the text is smaller than size.
    Vector2 factor = Vector2Divide(size, text_size);
    float scale_factor = fminf(factor.x, factor.y);
    if (scale_factor < 1) {
        *font_size *= scale_factor;
    }

    text_size = MeasureTextEx(font, text, *font_size, 0);

    position->y += size.y / 2.;
    position->y -= text_size.y / 2.;
    switch (align) {
        case NB_Align_Left:
            // Text is already horizontally aligned.
            break;

        case NB_Align_Center:
            position->x += size.x / 2;
            position->x -= text_size.x / 2;
            break;

        case NB_Align_Right:
            position->x += size.x;
            position->x -= text_size.x;
        break;
    }
}

bool render_button(char *text, Vector2 position, Vector2 size) {
    Rectangle rec = rec_from_vec2s(position, size);
    bool hover = CheckCollisionPointRec(GetMousePosition(), rec);
    Color bg_color = CONTROL_COLOR;
    if (hover) bg_color = CONTROL_COLOR_HL;
    DrawRectangleRounded(rec, 0.1, 2, bg_color);
    DrawRectangleRoundedLines(rec, 0.1, 2, 2, CONTROL_EDGE_COLOR);

    float font_size = 32;
    calculate_text_bounds(text, nb_font, &font_size, NB_Align_Center, &position, size);
    DrawTextEx(nb_font, text, position, font_size, 0, CONTROL_FG_COLOR);

    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

void show_keyboard(Noh_Arena *arena, NB_State *state, uint16 **pressed_keys, size_t num_pressed_keys) {
    if (IsKeyPressed(KEY_F10)) {
        state->view = NB_MainMenu;
        return;
    }

    // Change the background if any key is pressed.
    if (num_pressed_keys > 0) {
        static Color background_color = CLITERAL(Color){ 20, 20, 20, 255 };
        ClearBackground(background_color);

        uint16 key = *pressed_keys[0];
        noh_arena_save(arena);
        Noh_String str = {0};
        for (size_t i = 0; i < num_pressed_keys; i++) {
            char *keyStr = noh_arena_sprintf(arena, "%hu", key);
            noh_string_append_cstr(&str, keyStr);

            if (i < num_pressed_keys - 1) noh_string_append_cstr(&str, " - ");
        }
        noh_string_append_null(&str);

        Vector2 text_offset = Vector2Scale(MeasureTextEx(nb_font, str.elems, 120, 0), .5);
        Vector2 pos = Vector2Subtract(Vector2Scale(state->screen_size, .5), text_offset);
        DrawTextEx(nb_font, str.elems, pos, 120, 0, WHITE);
        noh_arena_reset(&arena);
        noh_string_free(&str);
    }
}

void main_menu(Noh_Arena *arena, NB_State *state) {
    (void)arena;
    if (IsKeyPressed(KEY_SCROLL_LOCK)) {
        state->view = NB_ShowKeyboard;
        return;
    }

    Vector2 pos = { .x = 0, .y = 0 };
    DrawTextEx(nb_font, "Main menu", pos, 24, 0, WHITE);

    pos.x = 10;
    pos.y = 100;
    Vector2 size = { .x = 250, .y = 50 };
    if (render_button("Switch to Keyboard", pos, size)) state->view = NB_ShowKeyboard;

    pos.y += 65;
    if (render_button("Quit", pos, size)) state->running = false;
}

int main(void)
{
    Noh_Arena arena = {0};

    // Initial state.
    NB_State state = { .screen_size = { .x = 800, .y = 600 }, .view = NB_MainMenu, .running = true };

    //const char *kb_path = "/dev/input/by-id/usb-Logitech_USB_Receiver-if02-event-kbd";
    const char *kb_path = "/dev/input/by-path/platform-i8042-serio-0-event-kbd";
    //const char *mouse_path = "/dev/input/by-id/usb-Logitech_USB_Receiver-if02-event-mouse";
    const char *mouse_path = "/dev/input/by-path/platform-AMDI0010:00-event-mouse";

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
    while (!WindowShouldClose() && state.running)
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
                main_menu(&arena, &state);
                break;

            case NB_ShowKeyboard:
                show_keyboard(&arena, &state, &pressed_keys, num_pressed_keys);
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
