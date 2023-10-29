#include <raylib.h>
#include <stddef.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>

void calculate_speed(bool is_accellerating, unsigned int *speed) {
    if (is_accellerating) {
        *speed += 2;
        if (*speed > 201) *speed = 201;
    } else {
        if (*speed > 0) *speed -= 1;
        if (*speed < 0) *speed = 0;
    }
}

float calculate_angle(unsigned int speed, float deadzone, unsigned int minSpeed, unsigned int maxSpeed, unsigned int doubleAt) {
    assert(minSpeed <= doubleAt && "doubleAt must not be lower than minSpeed.");
    assert(doubleAt <= maxSpeed && "maxSpeed must not be lower than doubleAt.");

    double totalRadians = 2 * PI - 2 * deadzone; // Deadzone in radians around the bottom (left and right).
    double startAngle = -PI - deadzone;

    double singleRange = doubleAt - minSpeed;
    double doubleRange = (maxSpeed - doubleAt) / 2;
    double totalRange = singleRange + doubleRange;

    unsigned int x = speed - minSpeed;
    doubleAt = doubleAt - minSpeed;
    float speedAngle = (x <= doubleAt)
        ? (x / totalRange) * totalRadians
        : (doubleAt / totalRange + ((x - doubleAt) / totalRange / 2)) * totalRadians;

    return startAngle + speedAngle;
}

int main(void)
{
    const int screenWidth = 800;
    const int screenHeight = 600;

    InitWindow(screenWidth, screenHeight, "Speedometer test");

    unsigned int speed = 0;
    Font font = GetFontDefault();

    SetTargetFPS(60);
    while (!WindowShouldClose())
    {
        calculate_speed(IsKeyDown(KEY_A), &speed);

        BeginDrawing();

        ClearBackground(BLACK);
        // Simple display of speed.
        DrawRectangle(10, screenHeight - speed, 20, speed, MAROON);
        DrawText(TextFormat("%i", (int)speed), 10, 40, 20, LIGHTGRAY);

        // Speedometer.
        int radius = 200;
        unsigned int minSpeed = 0;
        unsigned int maxSpeed = 240;
        unsigned int doubleAt = 60;

        int cx = screenWidth / 2;
        int cy = screenHeight / 2;

        // Draw speed steps.
        size_t i = 0;
        for (unsigned int step = minSpeed ; step <= maxSpeed ; step += (step < doubleAt) ? 5 : 10) {
            float angle = calculate_angle(step, PI / 4, minSpeed, maxSpeed, doubleAt);
            float x = cos(angle);
            float y = sin(angle);

            // Draw stripes.
            Color color = WHITE;
            if (step == 30 || step == 50) color = RED;

            int stripeLength = 20;
            if (i % 2 == 1) stripeLength = 10; 

            Vector2 start = { .x = cx + (radius - stripeLength) * x, .y = cy + (radius - stripeLength) * y };
            Vector2 end = { .x = cx + (radius * x), .y = cy + (radius * y) };
            DrawLineEx(start, end, 2, color);

            // Draw speed number.
            if (i % 2 == 0) {
                int fontSize = 24;
                if (i % 4 != 0) fontSize = 18;
                const char *text = TextFormat("%i", step);
                Vector2 textSize = MeasureTextEx(font, text, fontSize, 1);

                int textInset = 45;
                Vector2 textMiddle = { .x = cx + (radius - textInset) * x, .y = cy + (radius - textInset) * y };
                textMiddle.x -= textSize.x / 2;
                textMiddle.y -= textSize.y / 2;

                DrawTextEx(font, text, textMiddle, fontSize, 1, LIGHTGRAY);
            }
            i++;
        }

        // Draw indicator line.
        float angle = calculate_angle(speed, PI / 4, minSpeed, maxSpeed, doubleAt);
        float x = cos(angle);
        float y = sin(angle);
        DrawCircle(cx, cy, 20, DARKGRAY);
        Vector2 start = { .x = cx - 20 * x, .y = cy - 20 * y };
        Vector2 end = { .x = cx + (radius - 30) * x, .y = cy + (radius - 30) * y };
        DrawLineEx(start, end, 2, WHITE);

        EndDrawing();
    }

    CloseWindow();

    return 0;
}
