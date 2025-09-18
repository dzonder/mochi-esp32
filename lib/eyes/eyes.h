#ifndef EYES_H
#define EYES_H

class Eyes
{
public:
    /**
     * @brief Generates a 128x64 monochrome image of eyes.
     *
     * @param pupil_y Vertical position of pupils (-1.0 to 1.0, from top to bottom).
     * @param pupil_x Horizontal position of pupils (-1.0 to 1.0, from left to right).
     * @param eyebrows_low How low the eyebrows are (0.0 for normal, 1.0 for fully lowered).
     * @param pupil_size The size of the pupils (0.0 for smallest, 1.0 for largest).
     * @param eyebrow_angle The angle of the eyebrows in degrees (-10 for most angry, 10 for most surprised).
     * @param buffer A pointer to a 1024-byte buffer to store the image data.
     */
    static void draw_open(float pupil_y, float pupil_x, float eyebrows_low, float pupil_size, float eyebrow_angle, unsigned char *buffer);

    /**
     * @brief Generates a 128x64 monochrome image of half-open eyes.
     *
     * @param buffer A pointer to a 1024-byte buffer to store the image data.
     */
    static void draw_half_open(unsigned char *buffer);

    /**
     * @brief Generates a 128x64 monochrome image of closed eyes.
     *
     * @param buffer A pointer to a 1024-byte buffer to store the image data.
     */
    static void draw_closed(unsigned char *buffer);
};

#endif // EYES_H
