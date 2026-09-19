#include "vircon.h"
#include "vircon_platform.h"

void play_sound_in_channel(int sound_id, int channel_id)
{
    vircon_spu_select_channel(channel_id);
    vircon_spu_set_channel_assigned_sound(sound_id);
    vircon_spu_play_selected_channel();
}
