/*
 * Clang emits the four v1.7 operations for these dynamic values. This proves
 * the ordinary C frontend path without depending on a game-specific source.
 */
extern int vircon_rng_get_current_value(void);
extern void vircon_set_background_color(int color);
extern void vircon_spu_set_channel_volume(float volume);

int main(void)
{
    int value = vircon_rng_get_current_value();
    int comparison = vircon_rng_get_current_value();
    int sign_mask = value >> 31;
    int absolute_value = (value ^ sign_mask) - sign_mask;

    vircon_set_background_color(absolute_value);
    vircon_spu_set_channel_volume((float)(unsigned int)value);
    return value <= comparison;
}
