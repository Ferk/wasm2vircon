/* Exercise the official matrix signature and printing at the current point. */
#include <vircon.h>

int main(void) {
  int x, y;
  select_texture(-1);
  define_region_matrix(300, 10, 20, 19, 39, 12, 23, 2, 2, 1);
  get_region_minimum(&x, &y);
  if (get_selected_region() != 303 || x != 21 || y != 41)
    return 1;
  set_drawing_point(42, 65);
  print("A");
  get_drawing_point(&x, &y);
  return x != 42 || y != 65 || get_selected_region() != 'A';
}
