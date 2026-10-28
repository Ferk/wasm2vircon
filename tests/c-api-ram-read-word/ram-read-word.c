#include <vircon.h>

/* Exercise the public constants and force both physical boundary reads to
 * remain observable through an existing two-argument platform operation. */
void main(void) {
  int dynamic_address = get_frame_counter();
  set_drawing_point(ram_read_word(dynamic_address), ram_read_word(ram_word_count - 1));
  set_drawing_point(ram_read_word(999997), ram_read_word(1000000));
}
