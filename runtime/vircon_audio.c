#include "vircon.h"
#include "vircon_platform.h"

void select_sound(int sound_id) { vircon_spu_select_sound(sound_id); }
void set_sound_loop(int enabled) { vircon_spu_set_sound_play_with_loop(enabled); }

void play_sound_in_channel(int sound_id, int channel_id)
{
    vircon_spu_select_channel(channel_id);
    vircon_spu_set_channel_assigned_sound(sound_id);
    vircon_spu_play_selected_channel();
}

void select_channel(int channel_id) { vircon_spu_select_channel(channel_id); }
void assign_channel_sound(int channel_id, int sound_id) { vircon_spu_select_channel(channel_id); vircon_spu_set_channel_assigned_sound(sound_id); }

void set_channel_volume(float volume)
{
    vircon_spu_set_channel_volume(volume);
}
void set_channel_speed(float speed) { vircon_spu_set_channel_speed(speed); }
void play_channel(int channel_id) { vircon_spu_select_channel(channel_id); vircon_spu_play_selected_channel(); }

int play_sound(int sound_id)
{
    int channel;
    for (channel = 0; channel < 16; ++channel) {
        vircon_spu_select_channel(channel);
        if (vircon_spu_get_channel_state() == 0x40) {
            vircon_spu_set_channel_assigned_sound(sound_id);
            vircon_spu_play_selected_channel();
            return channel;
        }
    }
    return -1;
}
