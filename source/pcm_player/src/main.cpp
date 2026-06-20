#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_vector.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_text_generator.h"
#include "bn_string.h"

#include "gbfs.h"
#include "unifont_sprite_font.h"

#include <stdint.h>

// ---------------- STATE ----------------
static const GBFS_FILE* gbfs = nullptr;
static const uint8_t* src = nullptr;
static const uint8_t* src_end = nullptr;
static uint32_t src_len = 0;

static int cur_song = 0;
static int gbfs_total = 0;

static char current_song_name[64] = {0};

static bool select_locked = false;
static bool paused = false;

// ---------------- UI ----------------
bn::sprite_text_generator text_gen(unifont_sprite_font);

bn::vector<bn::sprite_ptr, 32> static_ui_sprites;
bn::vector<bn::sprite_ptr, 32> track_sprites;
bn::vector<bn::sprite_ptr, 16> dynamic_ui_sprites;

// ---------------- SCROLL ----------------
bn::vector<bn::fixed, 32> track_base_xs;
bn::fixed track_scroll_x = 0;
bn::fixed track_scroll_speed = 0.6;

// ---------------- AUDIO FORMAT ----------------
// PCM signed 8-bit mono @ 16000 Hz
static constexpr uint32_t SAMPLE_RATE = 16000;

// ---------------- TIME (FIXED) ----------------
static void decimal_time(char* dst, uint32_t byte_offset)
{
    // 1 byte = 1 sample (8-bit PCM mono)
    uint32_t total_seconds = byte_offset / SAMPLE_RATE;

    if(total_seconds > 5999)
        total_seconds = 5999;

    uint32_t m = total_seconds / 60;
    uint32_t s = total_seconds % 60;

    *dst++ = '0' + (m / 10);
    *dst++ = '0' + (m % 10);
    *dst++ = '0' + (s / 10);
    *dst++ = '0' + (s % 10);
}

// ---------------- HUD STATIC ----------------
static void init_hud_static_text()
{
    static_ui_sprites.clear();
    text_gen.set_left_alignment();

    text_gen.generate(-110, -65, "PCM Player for GBA", static_ui_sprites);
    text_gen.generate(-110, -50, "Copr. 2026, 2026", static_ui_sprites);
    text_gen.generate(-110, -35, "by yewgamer", static_ui_sprites);
    text_gen.generate(-110, 15, "Playing", static_ui_sprites);
}

// ---------------- SONG TITLE ----------------
static void hud_new_song(const char* name)
{
    init_hud_static_text();

    track_sprites.clear();
    track_base_xs.clear();
    track_scroll_x = 0;

    text_gen.set_left_alignment();
    text_gen.generate(-110, 30, name, track_sprites);

    for(int i = 0, size = track_sprites.size(); i < size; ++i)
    {
        track_base_xs.push_back(track_sprites[i].x());
    }
}

// ---------------- STATUS LINE ----------------
static char status_buffer[32];

static void hud_update_frame(bool locked, bool is_paused, int track_index, uint32_t byte_offset)
{
    dynamic_ui_sprites.clear();
    text_gen.set_left_alignment();

    char time_bcd[4];
    decimal_time(time_bcd, byte_offset);

    char* p = status_buffer;

    *p++ = locked ? '*' : ' ';
    *p++ = is_paused ? '|' : ' ';
    *p++ = ' ';
    *p++ = ' ';

    int v = track_index + 1;
    *p++ = '0' + (v / 10);
    *p++ = '0' + (v % 10);

    *p++ = ' ';
    *p++ = ' ';

    *p++ = time_bcd[0];
    *p++ = time_bcd[1];
    *p++ = ':';
    *p++ = time_bcd[2];
    *p++ = time_bcd[3];

    *p = 0;

    text_gen.generate(-110, 65, status_buffer, dynamic_ui_sprites);
}

// ---------------- SONG START ----------------
static void start_song()
{
    src = (const uint8_t*)gbfs_get_nth_obj(gbfs, cur_song, current_song_name, &src_len);
    src_end = src + src_len;

    hud_new_song(current_song_name);

    if(src && src_len > 0)
    {
        bn::core::direct_audio_play(src, src_len);
    }
}

// ---------------- MAIN ----------------
int main()
{
    bn::core::init();

    gbfs = find_first_gbfs_file((void*)0x08000000);

    if(!gbfs)
    {
        text_gen.set_left_alignment();
        text_gen.generate(-110, 10, "Please append gbfs file", static_ui_sprites);
        while(true) bn::core::update();
    }

    gbfs_total = gbfs_count_objs(gbfs);

    for(int i = 0; i < 60; ++i)
    {
        bn::core::update();
    }

    init_hud_static_text();
    start_song();

    while(true)
    {
        uint32_t current_offset = bn::core::direct_audio_get_offset();

        // ---------------- INPUT ----------------
        if(bn::keypad::select_pressed())
        {
            select_locked = !select_locked;
        }

        if(!select_locked)
        {
            if(bn::keypad::start_pressed())
            {
                paused = !paused;
                bn::core::direct_audio_pause_toggle();
            }

            if(!paused)
            {
                if(bn::keypad::b_pressed())
                {
                    int32_t target = (int32_t)current_offset - (33 * 50);
                    if(target < 0)
                    {
                        cur_song = (cur_song == 0) ? gbfs_total - 1 : cur_song - 1;
                        start_song();
                    }
                    else
                    {
                        bn::core::direct_audio_set_offset(target);
                    }
                }

                if(bn::keypad::a_pressed())
                {
                    uint32_t target = current_offset + (33 * 50);
                    if(target >= src_len)
                    {
                        cur_song = (cur_song + 1) % gbfs_total;
                        start_song();
                    }
                    else
                    {
                        bn::core::direct_audio_set_offset(target);
                    }
                }

                if(bn::keypad::right_pressed())
                {
                    cur_song = (cur_song + 1) % gbfs_total;
                    start_song();
                }

                if(bn::keypad::left_pressed())
                {
                    cur_song = (cur_song == 0) ? gbfs_total - 1 : cur_song - 1;
                    start_song();
                }
            }
        }

        // ---------------- AUTO ADVANCE ----------------
        if(!paused && src_len > 256 && current_offset >= (src_len - 256))
        {
            cur_song = (cur_song + 1) % gbfs_total;
            start_song();
        }

        // ---------------- SCROLL ----------------
        if(!paused)
        {
            track_scroll_x -= track_scroll_speed;

            if(track_scroll_x < -240)
            {
                track_scroll_x += 360;
            }

            for(int i = 0, size = track_sprites.size(); i < size; ++i)
            {
                track_sprites[i].set_x(track_base_xs[i] + track_scroll_x);
            }
        }

        // ---------------- HUD ----------------
        hud_update_frame(select_locked, paused, cur_song, current_offset);

        bn::core::update();
    }
}