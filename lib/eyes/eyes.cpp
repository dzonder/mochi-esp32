#include "eyes.h"
#include <cmath>
#include <cstring>

// Screen and buffer properties
const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 64;
const int BUFFER_SIZE = SCREEN_WIDTH * SCREEN_HEIGHT / 8;

// Eye properties
const int EYE_CENTER_Y = 32;
const int LEFT_EYE_CENTER_X = 32;
const int EYE_SEPARATION = 64;
const int EYE_R = 28;
const int IRIS_R = 9;
const float PUPIL_R_MIN = 3.0f;
const float PUPIL_R_MAX = 7.0f;
const float IRIS_SHIFT_X = 10.0f;
const float IRIS_SHIFT_Y = 10.0f;

// Eyebrow properties
const int EYEBROW_Y_BASE = 12;
const int EYEBROW_Y_RANGE = 12;
const float EYEBROW_ANGLE_LIMIT = 10.0f;

// Closed eye properties
const int CLOSED_EYE_Y = 32;
const int CLOSED_EYE_LENGTH = 40;
const int CLOSED_EYE_THICKNESS = 2;

// Half-open eyelid properties
const int UPPER_EYELID_Y = 24;
const int LOWER_EYELID_Y = 40;

namespace
{
    unsigned char sclera_mask[BUFFER_SIZE];
    bool sclera_mask_generated = false;

    void generate_sclera_mask()
    {
        if (sclera_mask_generated)
            return;

        memset(sclera_mask, 0, BUFFER_SIZE);
        for (int i = 0; i < 2; i++)
        {
            int eye_center_x = LEFT_EYE_CENTER_X + i * EYE_SEPARATION;
            for (int y = 0; y < SCREEN_HEIGHT; y++)
            {
                for (int x = eye_center_x - EYE_R; x < eye_center_x + EYE_R; x++)
                {
                    float dx_eye = (float)(x - eye_center_x) / EYE_R;
                    float dy_eye = (float)(y - EYE_CENTER_Y) / EYE_R;
                    if (dx_eye * dx_eye + dy_eye * dy_eye <= 1.0f)
                    {
                        int pixel = y * SCREEN_WIDTH + x;
                        sclera_mask[pixel / 8] |= (1 << (7 - (pixel % 8)));
                    }
                }
            }
        }
        sclera_mask_generated = true;
    }

    void set_pixel(int x, int y, unsigned char *buffer)
    {
        int pixel = y * SCREEN_WIDTH + x;
        buffer[pixel / 8] |= (1 << (7 - (pixel % 8)));
    }

    void clear_pixel(int x, int y, unsigned char *buffer)
    {
        int pixel = y * SCREEN_WIDTH + x;
        buffer[pixel / 8] &= ~(1 << (7 - (pixel % 8)));
    }

    void clear_buffer(unsigned char *buffer)
    {
        memset(buffer, 0, BUFFER_SIZE);
    }
}

void Eyes::draw_open(float pupil_y, float pupil_x, float eyebrows_low, float pupil_size, float eyebrow_angle, unsigned char *buffer)
{
    generate_sclera_mask();
    memcpy(buffer, sclera_mask, BUFFER_SIZE);

    float pupil_r = PUPIL_R_MIN + (PUPIL_R_MAX - PUPIL_R_MIN) * pupil_size;

    for (int i = 0; i < 2; i++)
    { // 0 for left eye, 1 for right eye
        int eye_center_x = LEFT_EYE_CENTER_X + i * EYE_SEPARATION;

        float iris_cx = eye_center_x + pupil_x * IRIS_SHIFT_X;
        float iris_cy = EYE_CENTER_Y + pupil_y * IRIS_SHIFT_Y;

        int eyebrow_y_base_pos = EYEBROW_Y_BASE + (int)(eyebrows_low * EYEBROW_Y_RANGE);
        float current_eyebrow_angle = (i == 1) ? -eyebrow_angle : eyebrow_angle;
        current_eyebrow_angle = fmaxf(-EYEBROW_ANGLE_LIMIT, fminf(EYEBROW_ANGLE_LIMIT, current_eyebrow_angle));
        float tan_angle = tanf(current_eyebrow_angle * M_PI / 180.0f);

        // Clear iris area and draw pupil
        for (int y = roundf(iris_cy) - IRIS_R; y <= roundf(iris_cy) + IRIS_R; y++)
        {
            for (int x = roundf(iris_cx) - IRIS_R; x <= roundf(iris_cx) + IRIS_R; x++)
            {
                float dx_iris = x - iris_cx;
                float dy_iris = y - iris_cy;
                if (dx_iris * dx_iris + dy_iris * dy_iris <= IRIS_R * IRIS_R)
                {
                    clear_pixel(x, y, buffer);
                }
            }
        }

        // Draw pupil
        for (int y = roundf(iris_cy) - pupil_r; y <= roundf(iris_cy) + pupil_r; y++)
        {
            for (int x = roundf(iris_cx) - pupil_r; x <= roundf(iris_cx) + pupil_r; x++)
            {
                float dx_pupil = x - iris_cx;
                float dy_pupil = y - iris_cy;
                if (dx_pupil * dx_pupil + dy_pupil * dy_pupil <= pupil_r * pupil_r)
                {
                    set_pixel(x, y, buffer);
                }
            }
        }

        // Draw eyebrows
        for (int x_offset = -EYE_R; x_offset <= EYE_R; x_offset++)
        {
            int x = eye_center_x + x_offset;
            int eyebrow_y_at_x = eyebrow_y_base_pos + roundf(tan_angle * x_offset);
            for (int y = 0; y < eyebrow_y_at_x; y++)
            {
                clear_pixel(x, y, buffer);
            }
        }
    }
}

void Eyes::draw_half_open(unsigned char *buffer)
{
    const float pupil_y = 0.0f;
    const float pupil_x = 0.0f;
    const float pupil_size = 0.3f;

    clear_buffer(buffer);

    float pupil_r = PUPIL_R_MIN + (PUPIL_R_MAX - PUPIL_R_MIN) * pupil_size;

    for (int i = 0; i < 2; i++)
    { // 0 for left eye, 1 for right eye
        int eye_center_x = LEFT_EYE_CENTER_X + i * EYE_SEPARATION;

        float iris_cx = eye_center_x + pupil_x * IRIS_SHIFT_X;
        float iris_cy = EYE_CENTER_Y + pupil_y * IRIS_SHIFT_Y;

        for (int y = 0; y < SCREEN_HEIGHT; y++)
        {
            for (int x = eye_center_x - EYE_R; x < eye_center_x + EYE_R; x++)
            {
                float dx_eye = (float)(x - eye_center_x) / EYE_R;
                float dy_eye = (float)(y - EYE_CENTER_Y) / EYE_R;

                if (dx_eye * dx_eye + dy_eye * dy_eye <= 1.0f)
                {
                    if (y < UPPER_EYELID_Y || y > LOWER_EYELID_Y)
                    {
                        continue;
                    }

                    float dx_iris = x - iris_cx;
                    float dy_iris = y - iris_cy;

                    if (dx_iris * dx_iris + dy_iris * dy_iris > IRIS_R * IRIS_R ||
                        (x - iris_cx) * (x - iris_cx) + (y - iris_cy) * (y - iris_cy) <= pupil_r * pupil_r)
                    {
                        set_pixel(x, y, buffer);
                    }
                }
            }
        }
    }
}

void Eyes::draw_closed(unsigned char *buffer)
{
    clear_buffer(buffer);

    for (int i = 0; i < 2; i++)
    { // 0 for left eye, 1 for right eye
        int eye_center_x = LEFT_EYE_CENTER_X + i * EYE_SEPARATION;
        int eye_start_x = eye_center_x - CLOSED_EYE_LENGTH / 2;
        int eye_end_x = eye_center_x + CLOSED_EYE_LENGTH / 2;

        for (int y = CLOSED_EYE_Y; y < CLOSED_EYE_Y + CLOSED_EYE_THICKNESS; y++)
        {
            for (int x = eye_start_x; x < eye_end_x; x++)
            {
                set_pixel(x, y, buffer);
            }
        }
    }
}
