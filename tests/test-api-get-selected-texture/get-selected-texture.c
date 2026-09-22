#include <vircon.h>

/* Proves the public GPU state getter uses the existing hardware import. */
int main(void)
{
    select_texture(3);
    return get_selected_texture();
}
