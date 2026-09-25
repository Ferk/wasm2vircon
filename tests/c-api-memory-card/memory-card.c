#include <vircon.h>

static game_signature read_signature;
static const game_signature write_signature = {
    'W', 'A', 'S', 'M', '2', 'V', 'I', 'R', 'C', 'O', 'N'
};
static const unsigned char write_data[8] = {
    0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE
};
static unsigned char read_data[8];

/* Exercises every public memcard.h equivalent with normal C storage. */
int main(void)
{
    int result = card_is_connected();

    card_read_signature(&read_signature);
    card_write_signature(&write_signature);
    result += card_signature_matches(&write_signature);
    result += card_is_empty();

    card_read_data(read_data, game_signature_words, 2);
    card_write_data(write_data, game_signature_words + 2, 2);
    return result + read_signature[0] + read_data[0] + read_data[7];
}
