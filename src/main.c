#include <raylib.h>
#include <stddef.h>
#include <stdio.h>
#include <math.h>

void calculate_speed(bool is_accellerating, unsigned int *speed) {
    if (is_accellerating) {
        *speed += 2;
    } else {
        if (*speed > 0) *speed -= 1;
        if (*speed < 0) *speed = 0;
    }
}

float calculate_angle(unsigned int speed, float deadzone, unsigned int minSpeed, unsigned int maxSpeed) {
    float totalRange = 2 * PI - 2 * deadzone; // Deadzone in radians around the bottom (left and right).
    float startAngle = -PI - deadzone;
    float degreesPerSpeed = totalRange / (maxSpeed - minSpeed);
    return startAngle + (speed * degreesPerSpeed);
}

int main(void)
{
    const int screenWidth = 800;
    const int screenHeight = 600;

    InitWindow(screenWidth, screenHeight, "Speedometer test");

    unsigned int speed = 0;
    Font font = GetFontDefault();
    int fontSize = 20;

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

        int cx = screenWidth / 2;
        int cy = screenHeight / 2;

        // Draw speed steps.
        for (unsigned int step = minSpeed ; step <= maxSpeed ; step += 20) {
            float angle = calculate_angle(step, PI / 4, minSpeed, maxSpeed);
            float x = cos(angle);
            float y = sin(angle);

            DrawLine(cx + (radius - 20) * x, cy + (radius - 20) * y, cx + (radius * x), cy + (radius * y), WHITE);

            const char *text = TextFormat("%i", step);
            Vector2 textSize = MeasureTextEx(font, text, fontSize, 1);

            Vector2 textMiddle = {};
            int textOutset = 30;
            textMiddle.x = cx + (radius + textOutset) * x;
            textMiddle.x -= textSize.x / 2;
            textMiddle.y = cy + (radius + textOutset) * y;
            textMiddle.y -= textSize.y / 2;
            
            DrawTextEx(font, text, textMiddle, fontSize, 1, LIGHTGRAY);
        }

        // Draw indicator line.
        float angle = calculate_angle(speed, PI / 4, minSpeed, maxSpeed);
        float x = cos(angle);
        float y = sin(angle);
        DrawLine(cx + 20 * x, cy + 20 * y, cx + (radius - 30) * x, cy + (radius - 30) * y, WHITE);


        EndDrawing();
    }

    CloseWindow();

    return 0;
}
