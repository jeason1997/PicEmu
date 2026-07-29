#include "common/sdl_text.h"

#include <ctype.h>
#include <string.h>

/* 5×7位图字体。只依赖SDL绘图，避免为了少量引脚文字引入SDL_ttf。 */
static const char *glyph(char c)
{
    static const char *const digits[] = {
        "0E11131519110E", "040C040404040E",
        "0E11010204081F", "1E01010601110E",
        "02060A121F0202", "1F101E0101110E",
        "0608101E11110E", "1F010204080808",
        "0E11110E11110E", "0E11110F01020C"
    };
    static const char *const letters[] = {
        "0E11111F111111", "1E11111E11111E",
        "0E11101010110E", "1E11111111111E",
        "1F10101E10101F", "1F10101E101010",
        "0E11101711110E", "1111111F111111",
        "0E04040404040E", "07020202120C",
        "11121418141211", "1010101010101F",
        "111B1515111111", "11191915131311",
        "0E11111111110E", "1E11111E101010",
        "0E11111115120D", "1E11111E141211",
        "0F10100E01011E", "1F040404040404",
        "1111111111110E", "11111111110A04",
        "11111115151B11", "11110A040A1111",
        "1111110A040404", "1F01020408101F"
    };

    c = (char)toupper((unsigned char)c);
    if (c >= '0' && c <= '9') return digits[c - '0'];
    if (c >= 'A' && c <= 'Z') return letters[c - 'A'];
    if (c == '/') return "01010204080810";
    if (c == '-') return "0000001F000000";
    if (c == '_') return "0000000000001F";
    if (c == ':') return "00040000040000";
    if (c == '.') return "00000000000C0C";
    return "00000000000000";
}

void sdl_draw_text(SDL_Renderer *renderer, int x, int y, int scale,
                   const char *text, SDL_Color color)
{
    size_t index;

    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (index = 0; text[index] != '\0'; ++index) {
        const char *rows = glyph(text[index]);
        int row;
        for (row = 0; row < 7; ++row) {
            unsigned bits;
            int column;
            char high = rows[row * 2];
            char low = rows[row * 2 + 1];
            unsigned high_value = high >= 'A'
                ? (unsigned)(high - 'A' + 10) : (unsigned)(high - '0');
            unsigned low_value = low >= 'A'
                ? (unsigned)(low - 'A' + 10) : (unsigned)(low - '0');
            bits = high_value * 16u + low_value;
            for (column = 0; column < 5; ++column) {
                if ((bits & (1u << (4 - column))) != 0) {
                    SDL_Rect pixel = {
                        x + (int)index * 6 * scale + column * scale,
                        y + row * scale, scale, scale
                    };
                    SDL_RenderFillRect(renderer, &pixel);
                }
            }
        }
    }
}

int sdl_text_width(const char *text, int scale)
{
    return (int)strlen(text) * 6 * scale;
}
