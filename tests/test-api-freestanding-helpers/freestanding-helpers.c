#include <vircon.h>

static char text[64] = "Hello";
static char copied[64];
static char number_text[64];
static unsigned char bytes[8] = {1, 2, 3, 4, 5, 6, 7, 8};
static unsigned char byte_copy[8];

/* Exercises the supported ordinary-C freestanding helper surface. */
int main(void)
{
    int result = 0;

    result += isdigit('4') + isxdigit('F') + isalpha('Z') + isascii(127);
    result += isalphanum('7') + islower(224) + isupper(192) + isspace('\t');
    result += tolower('A') + toupper('z');

    memset(byte_copy, 0, 8);
    memcpy(byte_copy, bytes, 8);
    result += memcmp(bytes, byte_copy, 8);
    strcpy(copied, text);
    strcat(copied, " world");
    strncpy(number_text, copied, 12);
    strncat(number_text, "!", 1);
    result += (int)strlen(number_text) + strcmp(copied, text) + strncmp(copied, text, 5);

    itoa(-42, number_text, 10);
    ftoa(get_drawing_angle(), number_text);
    result += (unsigned char)number_text[0] + (unsigned char)number_text[1];

    exit();
    return result;
}
