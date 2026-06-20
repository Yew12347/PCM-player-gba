/*
 * bn_core.cpp
 * Copyright (c) 2020-2026 Gustavo Valiente gustavo.valiente@protonmail.com
 * zlib License, see LICENSE file.
 * * Patched to completely bypass internal engine audio management for direct hardware PCM control.
 */

#include "bn_core.h"
#include "bn_core_lock.h"

#include "bn_color.h"
#include "bn_timer.h"
#include "bn_keypad.h"
#include "bn_memory.h"
#include "bn_timers.h"
#include "bn_profiler.h"
#include "bn_system_font.h"
#include "bn_bgs_manager.h"
#include "bn_hdma_manager.h"
#include "bn_link_manager.h"
#include "bn_gpio_manager.h"
#include "bn_audio_manager.h"
#include "bn_config_assert.h"
#include "bn_keypad_manager.h"
#include "bn_memory_manager.h"
#include "bn_display_manager.h"
#include "bn_sprites_manager.h"
#include "bn_cameras_manager.h"
#include "bn_palettes_manager.h"
#include "bn_bg_blocks_manager.h"
#include "bn_sprite_tiles_manager.h"
#include "bn_hblank_effects_manager.h"
#include "../hw/include/bn_hw_irq.h"
#include "../hw/include/bn_hw_core.h"
#include "../hw/include/bn_hw_gpio.h"
#include "../hw/include/bn_hw_sram.h"
#include "../hw/include/bn_hw_audio.h"
#include "../hw/include/bn_hw_timer.h"
#include "../hw/include/bn_hw_game_pak.h"
#include "../hw/include/bn_hw_hblank_effects.h"

#if BN_CFG_ASSERT_ENABLED
    #include "bn_assert_callback_type.h"
#endif

#if BN_CFG_ASSERT_ENABLED || BN_CFG_PROFILER_ENABLED
    #include "../hw/include/bn_hw_show.h"
#endif

#ifdef BN_STACKTRACE
    #if BN_CFG_LOG_ENABLED
        #include "../hw/include/bn_hw_stacktrace.h"
    #endif
#endif

#if BN_CFG_PROFILER_ENABLED && BN_CFG_PROFILER_LOG_ENGINE
    #if BN_CFG_PROFILER_LOG_ENGINE_DETAILED
        #define BN_PROFILER_ENGINE_GENERAL_START(id) \
            do \
            { \
            } while(false)

        #define BN_PROFILER_ENGINE_GENERAL_STOP() \
            do \
            { \
            } while(false)

        #define BN_PROFILER_ENGINE_DETAILED_START(id) \
            BN_PROFILER_START(id)

        #define BN_PROFILER_ENGINE_DETAILED_STOP() \
            BN_PROFILER_STOP()
    #else
        #define BN_PROFILER_ENGINE_GENERAL_START(id) \
            BN_PROFILER_START(id)

        #define BN_PROFILER_ENGINE_GENERAL_STOP() \
            BN_PROFILER_STOP()

        #define BN_PROFILER_ENGINE_DETAILED_START(id) \
            do \
            { \
            } while(false)

        #define BN_PROFILER_ENGINE_DETAILED_STOP() \
            do \
            { \
            } while(false)
    #endif
#else
    #define BN_PROFILER_ENGINE_GENERAL_START(id) \
        do \
        { \
        } while(false)

    #define BN_PROFILER_ENGINE_GENERAL_STOP() \
        do \
        { \
        } while(false)

    #define BN_PROFILER_ENGINE_DETAILED_START(id) \
        do \
        { \
        } while(false)

    #define BN_PROFILER_ENGINE_DETAILED_STOP() \
        do \
        { \
        } while(false)
#endif

namespace bn::core
{

namespace
{
    class ticks
    {

    public:
        int cpu_usage_ticks = 0;
        int vblank_usage_ticks = 0;
        int missed_frames = 0;
    };

    class static_data
    {

    public:
        update_callback_type update_callback = nullptr;
        vblank_callback_type vblank_callback = nullptr;
        #if BN_CFG_ASSERT_ENABLED
            assert::callback_type assert_callback = nullptr;
        #endif
        timer cpu_usage_timer;
        ticks last_ticks;
        bn::system_font system_font;
        string_view assert_tag = BN_CFG_ASSERT_TAG;
        int skip_frames = 0;
        int last_update_frames = 1;
        int missed_frames = 0;
        bool dma_enabled = hw::audio::dma_channel_free(3);
        bool slow_game_pak = false;
        volatile bool waiting_for_vblank = false;
        volatile bool vblank_intr_active = false;
    };

    alignas(static_data) BN_DATA_EWRAM_BSS char data_buffer[sizeof(static_data)];

    [[nodiscard]] static_data& data_ref()
    {
        return *reinterpret_cast<static_data*>(data_buffer);
    }

    // FEATURE: Direct Audio Engine State Management Variables
    const uint8_t* base_audio_ptr = nullptr;
    uint32_t audio_buffer_size = 0;
    volatile int audio_current_speed = 1;
    volatile bool audio_is_active = false;
    volatile bool audio_is_paused = false;

    // Stride location parameter references tracked across ticks
    volatile int32_t audio_sample_position = 0;

    void direct_audio_update()
    {
        if (!audio_is_active || audio_is_paused || !base_audio_ptr || audio_buffer_size == 0)
        {
            return;
        }

        // Advance raw address mapping offset by calculations matching current execution speed
        audio_sample_position += (audio_current_speed * 266); // 16000Hz / ~60 FPS ≈ 266 samples per frame

        // Boundary handling check loops
        if (audio_sample_position >= static_cast<int32_t>(audio_buffer_size))
        {
            audio_sample_position = 0; // Loop tracking boundary forwards
        }
        else if (audio_sample_position < 0)
        {
            audio_sample_position = static_cast<int32_t>(audio_buffer_size) - 1; // Loop tracking backwards
        }

        // Adjust running active DMA 1 register target on the fly to new position address block
        *(volatile uint32_t*)0x040000BC = (uint32_t)(base_audio_ptr + audio_sample_position);
    }

    void enable()
    {
        hblank_effects_manager::enable();
        link_manager::enable();

        // PATCH: Bypassed audio manager enable
        // audio_manager::enable();
        hw::irq::enable(hw::irq::id::VBLANK);

        hdma_manager::enable();
    }

    void disable(bool disable_vblank_irq)
    {
        hdma_manager::disable();

        if(disable_vblank_irq)
        {
            hw::irq::disable(hw::irq::id::VBLANK);
            // PATCH: Bypassed audio manager disable
            // audio_manager::disable();
        }

        link_manager::disable();
        hblank_effects_manager::disable();
    }

    void stop(bool disable_vblank_irq)
    {
        hw::core::wait_for_vblank(data_ref().vblank_intr_active);

        // PATCH: Bypassed audio manager stop
        // audio_manager::stop();
        hdma_manager::force_stop();
        hblank_effects_manager::stop();
         palettes_manager::stop();
        bgs_manager::stop();
        display_manager::stop();
        keypad_manager::stop();
        gpio_manager::stop();

        disable(disable_vblank_irq);
    }

    [[nodiscard]] ticks update_impl()
    {
        ticks result;

        BN_PROFILER_ENGINE_GENERAL_START("eng_update");

        BN_PROFILER_ENGINE_DETAILED_START("eng_cameras_update");
        cameras_manager::update();
        BN_PROFILER_ENGINE_DETAILED_STOP();

        BN_PROFILER_ENGINE_DETAILED_START("eng_sprites_update");
        sprites_manager::update();
        BN_PROFILER_ENGINE_DETAILED_STOP();

        BN_PROFILER_ENGINE_DETAILED_START("eng_spr_tiles_update");
        sprite_tiles_manager::update();
        BN_PROFILER_ENGINE_DETAILED_STOP();

        BN_PROFILER_ENGINE_DETAILED_START("eng_bgs_update");
        bgs_manager::update();
        BN_PROFILER_ENGINE_DETAILED_STOP();

        BN_PROFILER_ENGINE_DETAILED_START("eng_bg_blocks_update");
        bg_blocks_manager::update();
        BN_PROFILER_ENGINE_DETAILED_STOP();

        BN_PROFILER_ENGINE_DETAILED_START("eng_palettes_update");
        palettes_manager::update();
        BN_PROFILER_ENGINE_DETAILED_STOP();

        BN_PROFILER_ENGINE_DETAILED_START("eng_display_update");
        display_manager::update();
        BN_PROFILER_ENGINE_DETAILED_STOP();

        BN_PROFILER_ENGINE_DETAILED_START("eng_hblank_fx_update");
        hblank_effects_manager::update();
        BN_PROFILER_ENGINE_DETAILED_STOP();

        static_data& data = data_ref();
        bool use_dma = data.dma_enabled && ! link_manager::active();

        // PATCH: Ensure audio isn't stealing DMA settings contextually
        use_dma = use_dma && hw::audio::dma_channel_free(3);

        BN_PROFILER_ENGINE_GENERAL_STOP();

        BN_BARRIER;
        result.cpu_usage_ticks = data.cpu_usage_timer.elapsed_ticks();

        hw::core::wait_for_vblank(data.waiting_for_vblank);

        BN_BARRIER;
        data.cpu_usage_timer.restart();

        BN_PROFILER_ENGINE_GENERAL_START("eng_commit");

        BN_BARRIER;
        result.missed_frames = data.missed_frames;
        data.missed_frames = 0;

        BN_PROFILER_ENGINE_DETAILED_START("eng_hblank_fx_commit");
        hblank_effects_manager::disable();
        BN_PROFILER_ENGINE_DETAILED_STOP();

        // PATCH: Bypassed high-level hardware audio commands execution loop
        // BN_PROFILER_ENGINE_DETAILED_START("eng_audio_commands");
        // audio_manager::execute_commands();
        // BN_PROFILER_ENGINE_DETAILED_STOP();

        BN_PROFILER_ENGINE_DETAILED_START("eng_display_commit");
        display_manager::commit();
        BN_PROFILER_ENGINE_DETAILED_STOP();

        BN_PROFILER_ENGINE_DETAILED_START("eng_sprites_commit");
        sprites_manager::commit(use_dma);
        BN_PROFILER_ENGINE_DETAILED_STOP();

        BN_PROFILER_ENGINE_DETAILED_START("eng_bgs_commit");
        bgs_manager::commit(use_dma);
        BN_PROFILER_ENGINE_DETAILED_STOP();

        BN_PROFILER_ENGINE_DETAILED_START("eng_palettes_commit");
        palettes_manager::commit(use_dma);
        BN_PROFILER_ENGINE_DETAILED_STOP();

        BN_PROFILER_ENGINE_DETAILED_START("eng_spr_tiles_unc_commit");
        sprite_tiles_manager::commit_uncompressed(use_dma);
        BN_PROFILER_ENGINE_DETAILED_STOP();

        BN_PROFILER_ENGINE_DETAILED_START("eng_hdma_update");
        hdma_manager::update();
        BN_PROFILER_ENGINE_DETAILED_STOP();

        hdma_manager::commit_interrupt_handler();

        bool hdma_running = hdma_manager::commit_entries(use_dma);

        BN_PROFILER_ENGINE_DETAILED_START("eng_hblank_fx_commit");
        bool hblank_effects_running = hblank_effects_manager::commit();
        BN_PROFILER_ENGINE_DETAILED_STOP();

        BN_PROFILER_ENGINE_DETAILED_START("eng_big_maps_commit");
        bgs_manager::commit_big_maps();
        BN_PROFILER_ENGINE_DETAILED_STOP();

        use_dma = use_dma && ! hdma_running && ! hblank_effects_running;

        BN_PROFILER_ENGINE_DETAILED_START("eng_bg_blocks_unc_commit");
        bg_blocks_manager::commit_uncompressed(use_dma);
        BN_PROFILER_ENGINE_DETAILED_STOP();

        BN_PROFILER_ENGINE_DETAILED_START("eng_spr_tiles_cmp_commit");
        sprite_tiles_manager::commit_compressed();
        BN_PROFILER_ENGINE_DETAILED_STOP();

        BN_PROFILER_ENGINE_DETAILED_START("eng_bg_blocks_cmp_commit");
        bg_blocks_manager::commit_compressed();
        BN_PROFILER_ENGINE_DETAILED_STOP();

        BN_PROFILER_ENGINE_DETAILED_START("eng_vblank_callback");
        if(vblank_callback_type vblank_callback = data.vblank_callback)
        {
            vblank_callback();
        }
        BN_PROFILER_ENGINE_DETAILED_STOP();

        result.vblank_usage_ticks = data.cpu_usage_timer.elapsed_ticks();

        // FEATURE PATCH: Safe, integrated Direct Audio Hardware Hook execution loop
        BN_PROFILER_ENGINE_DETAILED_START("eng_custom_direct_audio");
        direct_audio_update();
        BN_PROFILER_ENGINE_DETAILED_STOP();

        BN_PROFILER_ENGINE_GENERAL_STOP();

        return result;
    }

    void _vblank_intr()
    {
        static_data& data = data_ref();

        if(data.waiting_for_vblank)
        {
            data.waiting_for_vblank = false;
        }
        else
        {
            hdma_manager::commit_entries(false);
            ++data.missed_frames;
        }

        link_manager::commit();
        data.vblank_intr_active = false;
    }
}

void init()
{
    init(nullopt, string_view());
}

void init(const optional<color>& transparent_color)
{
    init(transparent_color, string_view());
}

void init(const string_view& keypad_commands)
{
    init(nullopt, keypad_commands);
}

void init(const optional<color>& transparent_color, const string_view& keypad_commands)
{
    ::new(static_cast<void*>(data_buffer)) static_data();

    // Initial wait:
    hw::core::init();

    // Init heap:
    memory_manager::init();

    // Init H-Blank effects system:
    hblank_effects_manager::init();

    // Init irq system:
    hw::irq::init();
    hw::irq::set_isr(hw::irq::id::HBLANK, hw::hblank_effects::_intr);

    // Init hdma system:
    hdma_manager::init();

    // Init link system:
    link_manager::init();

    // Init V-Blank interrupt:
    hw::irq::set_isr(hw::irq::id::VBLANK, _vblank_intr);
    hw::irq::enable(hw::irq::id::VBLANK);

    // PATCH: Disallowed audio manager initialization framework sequence
    // audio_manager::init();

    // Init storage systems:
    static_data& data = data_ref();
    data.slow_game_pak = hw::game_pak::init();

    [[maybe_unused]] const char* sram_string = hw::sram::init();

    // Init gpio:
    [[maybe_unused]] const char* rtc_string = hw::gpio::init();

    // Init display:
    display_manager::init();

    // Init high level systems:
    cameras_manager::init();
    palettes_manager::init(transparent_color);
    sprite_tiles_manager::init();
    sprites_manager::init();
    bg_blocks_manager::init();
    bgs_manager::init();
    keypad_manager::init(keypad_commands);

    // First update:
    update();

    // Keypad polling fix:
    keypad_manager::update();

    // Init timer system:
    hw::timer::init();
    data.cpu_usage_timer.restart();
    data.last_ticks = ticks();

    // Reset profiler:
    BN_PROFILER_RESET();
}

int skip_frames()
{
    return data_ref().skip_frames;
}

void set_skip_frames(int skip_frames)
{
    BN_ASSERT(skip_frames >= 0, "Invalid skip frames: ", skip_frames);

    data_ref().skip_frames = skip_frames;
}

void update()
{
    static_data& data = data_ref();

    if(update_callback_type update_callback = data.update_callback)
    {
        update_callback();
    }

    int update_frames = data.skip_frames + 1;
    data.last_update_frames = update_frames;

    if(update_frames == 1)
    {
        data.last_ticks = update_impl();
    }
    else
    {
        ticks total_ticks;
        int frame_index = 0;

        while(frame_index < update_frames)
        {
            ticks frame_ticks = update_impl();
            total_ticks.cpu_usage_ticks += frame_ticks.cpu_usage_ticks;
            total_ticks.vblank_usage_ticks = bn::max(total_ticks.vblank_usage_ticks, frame_ticks.vblank_usage_ticks);
            frame_index += frame_ticks.missed_frames + 1;
        }

        total_ticks.missed_frames = frame_index - update_frames;
        data.last_ticks = total_ticks;
    }

    BN_PROFILER_ENGINE_DETAILED_START("eng_keypad");
    keypad_manager::update();
    BN_PROFILER_ENGINE_DETAILED_STOP();
}

void sleep(keypad::key_type wake_up_key)
{
    const keypad::key_type wake_up_keys[] = { wake_up_key };
    sleep(wake_up_keys);
}

void sleep(const span<const keypad::key_type>& wake_up_keys)
{
    BN_BASIC_ASSERT(! wake_up_keys.empty(), "There are no keys");

    // Force at least one update:
    update();

    // Wait until a wake up key is not pressed:
    bool wait = true;

    while(wait)
    {
        for(keypad::key_type wake_up_key : wake_up_keys)
        {
            if(! keypad::held(wake_up_key))
            {
                wait = false;
                break;
            }
        }

        if(wait)
        {
            update();
        }
    }

    // Lock core updates:
    core_lock lock;

    // Sleep display:
    display_manager::sleep();

    // Enable keypad interrupt with the specified wake up keys:
    keypad_manager::set_interrupt(wake_up_keys);
    hw::irq::enable(hw::irq::id::KEYPAD);

    // Enable sleep mode:
    hw::core::sleep();

    // Remove keypad interrupt:
    hw::irq::disable(hw::irq::id::KEYPAD);
}

void reset()
{
    stop(true);
    hw::core::reset();
}

void hard_reset()
{
    stop(true);
    hw::core::hard_reset();
}

fixed current_cpu_usage()
{
    static_data& data = data_ref();
    int current_cpu_usage_ticks = data.cpu_usage_timer.elapsed_ticks();
    int current_update_frames = data.skip_frames + 1;
    return fixed(current_cpu_usage_ticks) / (timers::ticks_per_frame() * current_update_frames);
}

int current_cpu_ticks()
{
    return data_ref().cpu_usage_timer.elapsed_ticks();
}

fixed last_cpu_usage()
{
    static_data& data = data_ref();
    return fixed(data.last_ticks.cpu_usage_ticks) / (timers::ticks_per_frame() * data.last_update_frames);
}

int last_cpu_ticks()
{
    return data_ref().last_ticks.cpu_usage_ticks;
}

fixed last_vblank_usage()
{
    static_data& data = data_ref();
    return fixed(data.last_ticks.vblank_usage_ticks) / timers::ticks_per_vblank();
}

int last_vblank_ticks()
{
    return data_ref().last_ticks.vblank_usage_ticks;
}

int last_missed_frames()
{
    return data_ref().last_ticks.missed_frames;
}

update_callback_type update_callback()
{
    return data_ref().update_callback;
}

void set_update_callback(update_callback_type update_callback)
{
    data_ref().update_callback = update_callback;
}

vblank_callback_type vblank_callback()
{
    return data_ref().vblank_callback;
}

void set_vblank_callback(vblank_callback_type vblank_callback)
{
    data_ref().vblank_callback = vblank_callback;
}

bool slow_game_pak()
{
    return data_ref().slow_game_pak;
}

const bn::system_font& system_font()
{
    return data_ref().system_font;
}

void set_system_font(const bn::system_font& font)
{
    data_ref().system_font = font;
}

const string_view& assert_tag()
{
    return data_ref().assert_tag;
}

void set_assert_tag(const string_view& assert_tag)
{
    data_ref().assert_tag = assert_tag;
}

void log_stacktrace()
{
    #if BN_CFG_LOG_ENABLED
        #ifdef BN_STACKTRACE
            bn::hw::stacktrace::log(3);
        #else
            BN_ERROR("Stack trace logging is disabled");
        #endif
    #endif
}

// FEATURE: Public API Implementations for Direct Audio Hardware Feature Hook
void direct_audio_play(const void* data, uint32_t size_bytes)
{
    base_audio_ptr = reinterpret_cast<const uint8_t*>(data);
    audio_buffer_size = size_bytes;
    audio_sample_position = 0;
    audio_is_paused = false;
    audio_is_active = true;

    // Reset targeted timing and playback registers
    *(volatile uint16_t*)0x04000102 = 0; // REG_TM0CNT_H
    *(volatile uint32_t*)0x04000104 = 0; // REG_DMA1CNT
    
    // Clear and configure Direct Sound hardware targets
    *(volatile uint16_t*)0x04000084 = 0x80; // REG_SOUNDCNT_X
    *(volatile uint16_t*)0x04000082 = (1 << 2) | (1 << 8) | (1 << 9) | (1 << 11); // REG_SOUNDCNT_H

    // Setup DMA1 addresses
    *(volatile uint32_t*)0x040000BC = (uint32_t)data;      // DMA1 Source
    *(volatile uint32_t*)0x040000C0 = 0x040000A0;          // DMA1 Destination (FIFO A)
    
    // Configure DMA Channel 1
    *(volatile uint32_t*)0x040000C4 = (1u << 31) | (1u << 25) | (1u << 26) | (1u << 23) | (3u << 28);

    // Boot up hardware Timer 0 at a standard sample rate target (16000Hz)
    *(volatile uint16_t*)0x04000100 = 65536 - 1048; // REG_TM0CNT_L
    *(volatile uint16_t*)0x04000102 = (1u << 7);    // REG_TM0CNT_H (Enabled)
}

void direct_audio_stop()
{
    audio_is_active = false;
    audio_is_paused = false;
    base_audio_ptr = nullptr;
    audio_buffer_size = 0;
    audio_sample_position = 0;
    *(volatile uint16_t*)0x04000102 = 0; // Turn off Timer 0
    *(volatile uint32_t*)0x040000C4 = 0; // Turn off DMA 1
}

void direct_audio_pause_toggle()
{
    if (!audio_is_active)
    {
        return;
    }

    audio_is_paused = !audio_is_paused;

    if (audio_is_paused)
    {
        *(volatile uint16_t*)0x04000102 = 0; // Disable timer to freeze playback sample intake
        *(volatile uint32_t*)0x040000C4 = 0; // Turn off DMA channel 1
    }
    else
    {
        // Re-align stream block address relative to the frame-calculated sample pointer
        *(volatile uint32_t*)0x040000BC = (uint32_t)(base_audio_ptr + audio_sample_position);
        *(volatile uint32_t*)0x040000C4 = (1u << 31) | (1u << 25) | (1u << 26) | (1u << 23) | (3u << 28);
        *(volatile uint16_t*)0x04000102 = (1u << 7); // Restart Timer 0
    }
}

void direct_audio_set_speed(int speed)
{
    audio_current_speed = speed;
}

int direct_audio_get_speed()
{
    return audio_current_speed;
}

bool direct_audio_get_paused()
{
    return audio_is_paused;
}

uint32_t direct_audio_get_offset()
{
    return static_cast<uint32_t>(audio_sample_position);
}

void direct_audio_set_offset(uint32_t offset)
{
    if (offset >= audio_buffer_size)
    {
        audio_sample_position = audio_buffer_size > 0 ? audio_buffer_size - 1 : 0;
    }
    else
    {
        audio_sample_position = static_cast<int32_t>(offset);
    }
    
    if (audio_is_active && !audio_is_paused && base_audio_ptr)
    {
        *(volatile uint32_t*)0x040000BC = (uint32_t)(base_audio_ptr + audio_sample_position);
    }
}

}

namespace bn
{

core_lock::core_lock()
{
    core::update();
    gpio_manager::sleep();
    core::disable(true);
}

core_lock::~core_lock()
{
    core::enable();
    keypad_manager::update();

    core::static_data& data = core::data_ref();
    hw::core::wait_for_vblank(data.vblank_intr_active);

    data.cpu_usage_timer.restart();
    display_manager::wake_up();
    gpio_manager::wake_up();
}

}

namespace bn::memory
{

bool dma_enabled()
{
    return core::data_ref().dma_enabled;
}

void set_dma_enabled(bool dma_enabled)
{
    core::data_ref().dma_enabled = dma_enabled;
}

}

#if BN_CFG_ASSERT_ENABLED
    namespace bn::assert
    {
        callback_type callback()
        {
            return core::data_ref().assert_callback;
        }

        void set_callback(callback_type callback)
        {
            core::data_ref().assert_callback = callback;
        }
    }

    namespace _bn::assert
    {
        namespace
        {
            [[noreturn]] void _show_impl(
                    const char* condition, const char* file_name, const char* function, int line,
                    const bn::string_view& message)
            {
                bn::core::static_data& data = bn::core::data_ref();

                if(bn::assert::callback_type assert_callback = data.assert_callback)
                {
                    assert_callback();
                }

                bn::core::stop(false);
                bn::hw::show::error(bn::core::system_font(), condition, file_name, function, line, message,
                                    bn::core::assert_tag());

                #ifdef BN_STACKTRACE
                    #if BN_CFG_LOG_ENABLED
                        bn::hw::core::wait_for_vblank(data.vblank_intr_active);
                        bn::hw::stacktrace::log(5);
                    #endif
                #endif

                while(true)
                {
                    bn::hw::core::wait_for_vblank(data.vblank_intr_active);
                }
            }
        }

        void show(const char* file_name, int line)
        {
            _show_impl("", file_name, "", line, "");
        }

        void show(const char* condition, const char* file_name, const char* function, int line)
        {
            _show_impl(condition, file_name, function, line, "");
        }

        void show(const char* condition, const char* file_name, const char* function, int line, const char* message)
        {
            _show_impl(condition, file_name, function, line, message);
        }

        void show(const char* condition, const char* file_name, const char* function, int line,
                  const bn::istring_base& message)
        {
            _show_impl(condition, file_name, function, line, message);
        }
    }
#endif

#if BN_CFG_PROFILER_ENABLED
    namespace bn::profiler
    {
        void show()
        {
            core::stop(false);
            hw::show::profiler_results(core::system_font());
        }
    }
#endif