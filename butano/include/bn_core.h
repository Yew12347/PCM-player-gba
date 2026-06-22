/*
 * bn_core.h
 * Copyright (c) 2020-2026 Gustavo Valiente gustavo.valiente@protonmail.com
 * zlib License, see LICENSE file.
 */

#ifndef BN_CORE_H
#define BN_CORE_H

/**
 * @file
 * bn::core header file.
 *
 * @ingroup core
 */

#include "bn_span.h"
#include "bn_fixed.h"
#include "bn_optional.h"
#include "bn_string_view.h"
#include "bn_vblank_callback_type.h"
#include "bn_core_update_callback_type.h"

namespace bn
{
    class color;
    class system_font;
}

namespace bn::keypad
{
    enum class key_type : uint16_t;
}

/**
 * @brief Core related functions.
 *
 * @ingroup core
 */
namespace bn::core
{
    /**
     * @brief This function must be called before using Butano, and it must be called only once.
     */
    void init();

    /**
     * @brief This function must be called before using Butano, and it must be called only once.
     * @param transparent_color Initial transparent color of the backgrounds.
     */
    void init(const optional<color>& transparent_color);

    /**
     * @brief This function must be called before using Butano, and it must be called only once.
     * @param keypad_commands Keypad commands recorded with the keypad logger.
     *
     * Instead of reading the keypad of the GBA, these keypad commands are replayed.
     */
    void init(const string_view& keypad_commands);

    /**
     * @brief This function must be called before using Butano, and it must be called only once.
     * @param transparent_color Initial transparent color of the backgrounds.
     * @param keypad_commands Keypad commands recorded with the keypad logger.
     *
     * Instead of reading the keypad of the GBA, these keypad commands are replayed.
     */
    void init(const optional<color>& transparent_color, const string_view& keypad_commands);

    /**
     * @brief Returns the number of frames to skip.
     *
     * 0 skip frames means ~60 frames per second, 1 skip frame means ~30 frames per second, and so on.
     */
    [[nodiscard]] int skip_frames();

    /**
     * @brief Sets the number of frames to skip.
     *
     * 0 skip frames means ~60 frames per second, 1 skip frame means ~30 frames per second, and so on.
     */
    void set_skip_frames(int skip_frames);

    /**
     * @brief Updates the screen and all Butano subsystems.
     */
    void update();

    /**
     * @brief Sleeps the GBA until the given keypad key is pressed.
     */
    void sleep(keypad::key_type wake_up_key);

    /**
     * @brief Sleeps the GBA until the given keypad keys are pressed.
     */
    void sleep(const span<const keypad::key_type>& wake_up_keys);

    /**
     * @brief Resets the GBA, going back to the start of main().
     *
     * Keep in mind that it doesn't rewind the stack, so alive objects are not destroyed.
     */
    [[noreturn]] void reset();

    /**
     * @brief Resets the GBA, going back to the start of main() after showing the BIOS intro.
     *
     * Keep in mind that it doesn't rewind the stack, so alive objects are not destroyed.
     */
    [[noreturn]] void hard_reset();

    /**
     * @brief Returns the CPU usage of the current frame.
     *
     * A CPU usage greater than 1 means that at least one screen refresh should have been missed.
     */
    [[nodiscard]] fixed current_cpu_usage();

    /**
     * @brief Returns the CPU timer ticks of the current frame.
     *
     * A CPU tick count greater than timers::ticks_per_frame() means that at least one screen refresh
     * should have been missed.
     */
    [[nodiscard]] int current_cpu_ticks();

    /**
     * @brief Returns the CPU usage of the last elapsed frame.
     *
     * A CPU usage greater than 1 means that at least one screen refresh should have been missed.
     *
     * If you only want to retrieve the number of missed screen refreshes,
     * core::last_missed_frames is more accurate.
     */
    [[nodiscard]] fixed last_cpu_usage();

    /**
     * @brief Returns the CPU timer ticks of the last elapsed frame.
     *
     * A CPU tick count greater than timers::ticks_per_frame() means that at least one screen refresh
     * should have been missed.
     *
     * If you only want to retrieve the number of missed screen refreshes,
     * core::last_missed_frames is more accurate.
     */
    [[nodiscard]] int last_cpu_ticks();

    /**
     * @brief Returns the maximum V-Blank usage of the last elapsed frame.
     *
     * A V-Blank usage greater than 1 means that the screen is being redrawn
     * before all of GBA display components are updated.
     */
    [[nodiscard]] fixed last_vblank_usage();

    /**
     * @brief Returns the maximum V-Blank timer ticks of the last elapsed frame.
     *
     * A V-Blank tick count greater than timers::ticks_per_vblank() means that the screen is being redrawn
     * before all of GBA display components are updated.
     */
    [[nodiscard]] int last_vblank_ticks();

    /**
     * @brief Returns the number of screen refreshes that were missed in the last core::update call.
     */
    [[nodiscard]] int last_missed_frames();

    /**
     * @brief Returns the user function called in core::update, before V-Blank.
     */
    [[nodiscard]] update_callback_type update_callback();

    /**
     * @brief Sets the user function called in core::update, before V-Blank.
     */
    void set_update_callback(update_callback_type update_callback);

    /**
     * @brief Returns the user function called in core::update, during V-Blank.
     */
    [[nodiscard]] vblank_callback_type vblank_callback();

    /**
     * @brief Sets the user function called in core::update, during V-Blank.
     */
    void set_vblank_callback(vblank_callback_type vblank_callback);

    /**
     * @brief Indicates if a slow game pak like the SuperCard SD has been detected or not.
     */
    [[nodiscard]] bool slow_game_pak();

    /**
     * @brief Returns the font used to show assert and profiling messages.
     */
    [[nodiscard]] const system_font& system_font();

    /**
     * @brief Sets the font used to show assert and profiling messages.
     */
    void set_system_font(const bn::system_font& font);

    /**
     * @brief Returns the tag displayed in assert messages.
     */
    [[nodiscard]] const string_view& assert_tag();

    /**
     * @brief Sets the tag displayed in assert messages.
     */
    void set_assert_tag(const string_view& assert_tag);

    /**
     * @brief Logs the current stack trace.
     */
    void log_stacktrace();

    // =========================================================================
    // CUSTOM BYPASS PATCH: Direct Hardware Audio Subsystem Hooks
    // =========================================================================

    /**
     * @brief Boots up DMA Channel 1 and Timer 0 to instantly stream raw unsigned
     * 8-bit linear mono PCM audio directly into hardware FIFO A with boundary checks.
     * @param data Pointer to raw, 4-byte-aligned linear PCM audio byte array data.
     * @param size_bytes Total size of data sample array buffer block.
     */
    void direct_audio_play(const void* data, uint32_t size_bytes);

    /**
     * @brief Alternative CPU-driven FIFO playback streaming engine that completely
     * drops hardware DMA channel dependencies. Updates sample clusters inside the main loop.
     * @param data Pointer to raw, 4-byte-aligned linear PCM audio byte array data.
     * @param size_bytes Total size of data sample array buffer block.
     */
    void direct_audio_play_cpu(const void* data, uint32_t size_bytes);

    /**
     * @brief Alternative Hardware Interrupt-driven driver variant that feeds raw
     * PCM audio clusters byte-by-byte into the FIFO queue inside an active Timer 0 ISR.
     * @param data Pointer to raw, 4-byte-aligned linear PCM audio byte array data.
     * @param size_bytes Total size of data sample array buffer block.
     */
    void direct_audio_play_irq(const void* data, uint32_t size_bytes);

    /**
     * @brief Tears down active audio hardware timers and DMA controller channels 
     * associated with the bypassed direct-sound pipeline to halt playback.
     */
    void direct_audio_stop();

    /**
     * @brief Suspends or resumes the running direct sound playback pipeline.
     */
    void direct_audio_pause_toggle();

    /**
     * @brief Changes the current playback rate multiplier stride factor (e.g. 1, 2, -2).
     */
    void direct_audio_set_speed(int speed);

    /**
     * @brief Returns the current active playback speed stride context flag.
     */
    [[nodiscard]] int direct_audio_get_speed();

    /**
     * @brief Returns whether the current sound stream pipeline layout is paused.
     */
    [[nodiscard]] bool direct_audio_get_paused();

    /**
     * @brief Returns the current tracking stream position byte offset.
     */
    [[nodiscard]] uint32_t direct_audio_get_offset();

    /**
     * @brief Manually rewrites the tracking stream position byte offset.
     */
    void direct_audio_set_offset(uint32_t offset);
}

#endif