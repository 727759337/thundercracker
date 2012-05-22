/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#include "xmtrackerplayer.h"
#include "audiomixer.h"
#include <stdint.h>
#include <stdlib.h>
#include "event.h"

// mingw appears to not get this quite right...
#ifndef UINT16_MAX
#define UINT16_MAX 0xffff
#endif
#ifndef UINT32_MAX
#define UINT32_MAX 0xffffffff
#endif

//#define XMTRACKERDEBUG
#define LGPFX "XmTrackerPlayer: "
#define ASSERT_BREAK(_x) if (!(_x)) { ASSERT(_x); break; }
XmTrackerPlayer XmTrackerPlayer::instance;
const uint8_t XmTrackerPlayer::kLinearFrequencies;
const uint8_t XmTrackerPlayer::kAmigaFrequencies;
const uint8_t XmTrackerPlayer::kMaxVolume;
const uint8_t XmTrackerPlayer::kEnvelopeSustain;
const uint8_t XmTrackerPlayer::kEnvelopeLoop;

#include "xmtrackertables.h"

// To be replaced someday by a channel allocator and array
#define CHANNEL_FOR(x) (_SYS_AUDIO_MAX_CHANNELS - ((x) + 1))

bool XmTrackerPlayer::play(const struct _SYSXMSong *pSong)
{
    if (isPlaying()) {
        LOG((LGPFX"Notice: play() called while already playing.\n"));
    }

    // Does the world make any sense? 
    if (!pSong->nPatterns) {
        LOG((LGPFX"Error: Invalid song (no patterns)\n"));
        ASSERT(pSong->nPatterns);
        return false;
    }
    if (pSong->nChannels > _SYS_AUDIO_MAX_CHANNELS) {
        LOG((LGPFX"Warning: Song has %u channels, %u supported\n",
             pSong->nChannels, _SYS_AUDIO_MAX_CHANNELS));
        return false;
    }

    /* Make sure no one is currently using the channels I need.
     * Unfortunately, there's no way to make sure no one else clobbers me
     * during playback, but it won't take long for the perp to notice.
     */
    for (unsigned i = 0; i < pSong->nChannels; i++)
        if (AudioMixer::instance.isPlaying(CHANNEL_FOR(i)))
            return false;

    // Ok, things look (probably) good.
    song = *pSong;
    
    volume = kMaxVolume;
    userVolume = _SYS_AUDIO_DEFAULT_VOLUME;
    bpm = song.bpm;
    ticks = tempo = song.tempo;
    delay = 0;
    phrase = 0;
    memset(&next, 0, sizeof(next));

    pattern.init(&song)->loadPattern(patternOrderTable(phrase));
    if (!isPlaying()) return false;

    memset(channels, 0, sizeof(channels));
    for (unsigned i = 0; i < arraysize(channels); i++) {
        XmTrackerPattern::resetNote(channels[i].note);
        channels[i].userVolume = _SYS_AUDIO_MAX_VOLUME;
    }

    AudioMixer::instance.setTrackerCallbackInterval(2500000 / bpm);
    tick();

    return true;
}

void XmTrackerPlayer::stop()
{
    if (isPlaying()) {
        ASSERT(song.nChannels);
        
        AudioMixer &mixer = AudioMixer::instance;

        for (unsigned i = 0; i < song.nChannels; i++)
            if (mixer.isPlaying(CHANNEL_FOR(i)))
                mixer.stop(CHANNEL_FOR(i));

    } else {
        LOG((LGPFX"Notice: stop() called when no module was playing.\n"));
    }

    song.nPatterns = 0;
    AudioMixer::instance.setTrackerCallbackInterval(0);
}

void XmTrackerPlayer::setVolume(int pVolume, uint8_t ch)
{
    pVolume = clamp(pVolume, 0, _SYS_AUDIO_MAX_VOLUME);
    if (ch > song.nChannels) {
        userVolume = pVolume;
    } else {
        channels[ch].userVolume = pVolume;
    }
}

inline void XmTrackerPlayer::loadNextNotes()
{
    if (ticks) {
        LOG((LGPFX"Error: loading notes off a tick boundary!\n"));
        ASSERT(!ticks);
        ticks = 0;
    }

    // Advance song position on row overflow or effect
    if (next.row >= pattern.nRows() || next.force) {
        if (!next.force) {
            if (loop.i && next.row > loop.end)
                processPatternBreak(phrase, loop.start);
            else
                processPatternBreak(song.patternOrderTableSize, 0);
        }

        if (phrase != next.phrase && next.phrase < song.patternOrderTableSize) {
            pattern.loadPattern(patternOrderTable(next.phrase));
            phrase = next.phrase;
            memset(&loop, 0, sizeof(loop));
        } else if (next.phrase >= song.patternOrderTableSize) {
            LOG((LGPFX"Warning: next phrase (%u) is larger than pattern order table (%u entries)\n",
                 next.phrase, song.patternOrderTableSize));
            stop();
            return;
        }

        if (next.row >= pattern.nRows()) {
            LOG((LGPFX"Warning: next row (%u) is larger than pattern (%u rows)\n",
                 next.row, pattern.nRows()));
            stop();
            return;
        }
        
        next.force = false;

#ifdef XMTRACKERDEBUG
        LOG((LGPFX"Debug: Advancing to phrase %u, row %u\n", phrase, next.row));
#endif
    }

    // Get and process the next row of notes
    struct XmTrackerNote note;
    for (unsigned i = 0; i < song.nChannels; i++) {
        struct XmTrackerChannel &channel = channels[i];

        // ProTracker 2/3 compatibility. FastTracker II maintains final tremolo volume
        channel.volume = channel.tremolo.volume;

        pattern.getNote(next.row, i, note);

#ifdef XMTRACKERDEBUG
        if (i) LOG((" | "));
        else LOG((LGPFX"Debug: "));
        LOG(("%3d %3d x%02x x%02x x%02x",
             note.note, note.instrument,
             note.volumeColumnByte,
             note.effectType, note.effectParam));
        if ((int)i == song.nChannels - 1) LOG(("\n"));
#endif

        channel.valid = false;
        bool recNote = false,
             recInst = false;
        if (note.instrument == XmTrackerPattern::kNoInstrument) {
            recInst = true;
            note.instrument = channel.note.instrument;
        }
        if (note.note == XmTrackerPattern::kNoNote) {
            recNote = true;
            note.note = channel.note.note;
        }

        if (note.effectType != XmTrackerPattern::kNoEffect &&
            note.effectParam == XmTrackerPattern::kNoParam)
        {
            note.effectParam = 0;
        }

        if (channel.note.instrument != note.instrument && note.instrument < song.nInstruments) {
            // Change the instrument.
            if (!SvmMemory::copyROData(channel.instrument, song.instruments + note.instrument * sizeof(_SYSXMInstrument))) {
                LOG((LGPFX"Error: Could not load instrument %u from flash!\n", note.instrument));
                ASSERT(false);
                stop();
                return;
            }
        } else if (note.instrument >= song.nInstruments) {
            channel.instrument.sample.pData = 0;
        }

        if (!recInst && channel.instrument.sample.pData) {
            channel.volume = channel.instrument.sample.volume;
        }
        
        channel.start = !recNote;
        // Don't play with an invalid instrument
        if (channel.start && note.instrument >= song.nInstruments) {
            channel.start = false;
        }
        // Stop playing/don't play when no sample data or note
        if (channel.realNote(note.note) > XmTrackerPattern::kMaxNote ||
            !channel.instrument.sample.pData)
        {
            channel.start = false;
        }

        if (!channel.active && !channel.start) {
            channel.note = note;
            continue;
        }

        // Remember old/current period for portamento slide.
        channel.porta.period = channel.period;
        if (!recNote) {
            channel.period = getPeriod(channel.realNote(note.note), channel.instrument.finetune);
            if (note.instrument != channel.note.instrument || !channel.porta.period) {
                channel.porta.period = channel.period;
            }
        }
        if (channel.period) {
            channel.frequency = getFrequency(channel.period);
        }
        channel.note = note;

        // Volume
        channel.fadeout = UINT16_MAX;
        if (note.volumeColumnByte >= 0x10 && note.volumeColumnByte <= 0x50)
                channel.volume = note.volumeColumnByte - 0x10;

        if (channel.start) {
            // TODO: auto-vibrato.
            channel.vibrato.phase = 0;
            channel.tremolo.phase = 0;

            memset(&channel.envelope, 0, sizeof(channel.envelope));
            channel.envelope.done = !channel.instrument.nVolumeEnvelopePoints;
        }

        channel.active = true;
    }
    pattern.releaseRef();

    // Advance song position
    next.row++;
}

// Volume commands
enum {
    vxSlideDown              = 6,
    vxSlideUp,              // 7
    vxFineVolumeDown,       // 8
    vxFineVolumeUp,         // 9
    vxVibratoSpeed,         // A
    vxVibratoDepth,         // B
    vxTonePortamento        // F. C, D, and E are panning commands
};

void XmTrackerPlayer::incrementVolume(uint16_t &volume, uint8_t inc)
{
    volume = MIN(volume + inc, kMaxVolume);
}

void XmTrackerPlayer::decrementVolume(uint16_t &volume, uint8_t dec)
{
    volume = (uint16_t)MAX((int32_t)volume - dec, 0);
}

void XmTrackerPlayer::processVolumeSlideUp(XmTrackerChannel &channel, uint16_t &volume, uint8_t inc)
{
    if (inc) channel.slideUp = inc;
    incrementVolume(volume, inc);
}

void XmTrackerPlayer::processVolumeSlideDown(XmTrackerChannel &channel, uint16_t &volume, uint8_t dec)
{
    if (dec) channel.slideDown = dec;
    decrementVolume(volume, dec);
}

// Half-wave precomputed sine table used by vibrato and tremolo.
static const uint8_t sineTable[] = {0,24,49,74,97,120,141,161,
                                    180,197,212,224,235,244,250,253,
                                    255,253,250,244,235,224,212,197,
                                    180,161,141,120,97,74,49,24};

void XmTrackerPlayer::processVibrato(XmTrackerChannel &channel)
{
    int32_t periodDelta = 0;
    STATIC_ASSERT(arraysize(sineTable) == 32);

    switch (channel.vibrato.type) {
        default:
            // Intentional fall-through
        case 3:   // Random (but not really)
            // Intentional fall-through
        case 0: { // Sine
            periodDelta = sineTable[channel.vibrato.phase % arraysize(sineTable)];
            if (channel.vibrato.phase >= arraysize(sineTable)) periodDelta = -periodDelta;
            break;
        }
        case 1:   // Ramp up
            LOG(("%s:%d: NOT_TESTED: ramp vibrato\n", __FILE__, __LINE__));
            periodDelta = (channel.vibrato.phase % arraysize(sineTable)) * -8;
            if (channel.vibrato.phase >= arraysize(sineTable)) periodDelta += 255;
            break;
        case 2:   // Square
            LOG(("%s:%d: NOT_TESTED: square vibrato\n", __FILE__, __LINE__));
            periodDelta = 255;
            if (channel.vibrato.phase >= arraysize(sineTable)) periodDelta = -periodDelta;
            break;
    }
    periodDelta = (periodDelta * channel.vibrato.depth) / 32;

    channel.frequency = getFrequency(channel.period + periodDelta);

    if (ticks)
        channel.vibrato.phase = (channel.vibrato.speed + channel.vibrato.phase) % (arraysize(sineTable) * 2);
}

void XmTrackerPlayer::processPorta(XmTrackerChannel &channel)
{
    if (!ticks) {
        uint32_t tmp;
        tmp = channel.period;
        channel.period = channel.porta.period;
        channel.porta.period = tmp;
        // Not playing a new note anymore, sustain-shifting to another.
        channel.start = false;
        return;
    }

    if (!channel.porta.period) {
        LOG((LGPFX"Error: Porta period was not set, can not shift!\n"));
        ASSERT(channel.porta.period);
        return;
    }

    int32_t delta=(int32_t)channel.period-(int32_t)channel.porta.period;

    if (!delta) return;

    if (abs(delta) < channel.tonePorta * 4) {
        channel.period = channel.porta.period;
    } else if (delta < 0) {
        channel.period += channel.tonePorta * 4;
    } else {
        channel.period -= channel.tonePorta * 4;
    }
    channel.frequency = getFrequency(channel.period);
}

void XmTrackerPlayer::processVolume(XmTrackerChannel &channel)
{
    uint8_t command = channel.note.volumeColumnByte >> 4;
    uint8_t param = channel.note.volumeColumnByte & 0xF;

    switch (command) {
        case vxSlideDown:
            if (!param) LOG(("%s:%d: NOT_IMPLEMENTED: empty param to vxSlideDown\n", __FILE__, __LINE__));
            decrementVolume(channel.volume, param);
            break;
        case vxSlideUp:
            if (!param) LOG(("%s:%d: NOT_IMPLEMENTED: empty param to vxSlideUp\n", __FILE__, __LINE__));
            incrementVolume(channel.volume, param);
            break;
        case vxFineVolumeDown:
            if (ticks) break;
            if (!param) LOG(("%s:%d: NOT_IMPLEMENTED: empty param to vxFineVolumeDown\n", __FILE__, __LINE__));
            processVolumeSlideDown(channel, channel.volume, param);
            break;
        case vxFineVolumeUp:
            if (ticks) break;
            if (!param) LOG(("%s:%d: NOT_IMPLEMENTED: empty param to vxFineVolumeDown\n", __FILE__, __LINE__));
            processVolumeSlideUp(channel, channel.volume, param);
            break;
        case vxVibratoSpeed:
            if (param) channel.vibrato.speed = param;
            processVibrato(channel);
            break;
        case vxVibratoDepth:
            if (param) channel.vibrato.depth = param;
            processVibrato(channel);
            break;
        case vxTonePortamento:
            if (param) channel.tonePorta = param | (param << 4);
            processPorta(channel);
            break;
        default:
            break;
    }
}

// Note effects
enum {
    fxArpeggio                   = 0x00, // "Appregio" in the file spec *frown*
    fxPortaUp,                  // 0x01
    fxPortaDown,                // 0x02
    fxTonePorta,                // 0x03
    fxVibrato,                  // 0x04
    fxTonePortaAndVolumeSlide,  // 0x05
    fxVibratoAndVolumeSlide,    // 0x06
    fxTremolo,                  // 0x07
    // fx8 is Set Panning, not implemented.
    fxSampleOffset               = 0x09,
    fxVolumeSlide,              // 0x0A
    fxPositionJump,             // 0x0B
    fxSetVolume,                // 0x0C
    fxPatternBreak,             // 0x0D
    fxOverflow,                 // 0x0E, hack inherited from the MOD format
    fxSetTempoAndBPM,           // 0x0F
    fxSetGlobalVolume,          // 0x10 / G
    fxGlobalVolumeSlide,        // 0x11 / H
    // TODO: fxKeyOff (K)
    fxSetEnvelopePos             = 0x15, // L
    fxMultiRetrigNote            = 0x1B, // R
    fxTremor                     = 0x1D, // T
    fxExtraFinePorta             = 0x21, // X
};

enum {
    fxFinePortaUp                = 0x01, // E1
    fxFinePortaDown,            // 0x02
    fxGlissControl,             // 0x03
    fxVibratoControl,           // 0x04
    fxFinetune,                 // 0x05
    fxLoopPattern,              // 0x06, called "Set loop begin/loop" in XM spec
    fxTremoloControl,           // 0x07
    fxRetrigNote                 = 0x09, // 8 is not used
    fxFineVolumeSlideUp,        // 0x0A
    fxFineVolumeSlideDown,      // 0x0B
    fxNoteCut,                  // 0x0C
    fxNoteDelay,                // 0x0D
    fxPatternDelay              // 0x0E
};

void XmTrackerPlayer::processArpeggio(XmTrackerChannel &channel)
{
    uint8_t phase = ticks % 3;
    uint8_t note;

    // ProTracker 2/3 sequence. FastTracker II reverses cases 1 and 2.
    switch (phase) {
        case 1:
            note = channel.note.effectParam >> 4;
            break;
        case 2:
            note = channel.note.effectParam & 0x0F;
            break;
        default:
            // Inconceivable, but the compiler doesn't think so.
            ASSERT(false);
        case 0:
            return;
    }
    note += channel.realNote();

    if (note > XmTrackerPattern::kMaxNote) {
        LOG((LGPFX"Notice: Clipped arpeggio (base note: %d, arpeggio: %02x)\n",
             channel.realNote(), channel.note.effectParam));
        note = XmTrackerPattern::kMaxNote;
    }

    // Apply relative period shift, to avoid disrupting other active effects
    channel.frequency = getFrequency(getPeriod(note, channel.instrument.finetune));
}

void XmTrackerPlayer::processVolumeSlide(uint16_t &pVolume, uint8_t param)
{
    if (!ticks || !param) return;
    uint8_t up = param >> 4;
    uint8_t down = param & 0x0F;

    if (up) {
        incrementVolume(pVolume, up);
    } else if (down) {
        decrementVolume(pVolume, down);
    }
}

void XmTrackerPlayer::processTremolo(XmTrackerChannel &channel)
{
    if (!ticks || !channel.tremolo.depth || !channel.tremolo.speed) return;
    STATIC_ASSERT(arraysize(sineTable) == 32);

    int16_t delta;
    delta = sineTable[channel.tremolo.phase % arraysize(sineTable)] * channel.tremolo.depth / 32;
    if (channel.tremolo.phase >= arraysize(sineTable)) delta = -delta;
    channel.tremolo.phase = (channel.tremolo.speed + channel.tremolo.phase) % (arraysize(sineTable) * 2);

    channel.volume = clamp(channel.tremolo.volume + delta, 0, (int)kMaxVolume);
}

void XmTrackerPlayer::processPatternBreak(uint16_t nextPhrase, uint16_t nextRow)
{
    next.force = true;

    // Adjust phrase as appropriate
    if (nextPhrase >= song.patternOrderTableSize) next.phrase = phrase + 1;
    else next.phrase = nextPhrase;

    // This is bounds-checked later, when the pattern is loaded.
    next.row = nextRow;

    // Reached end of song
    if (next.phrase >= song.patternOrderTableSize) {
        // This is bounds-checked later, when the break is applied.
        next.phrase = song.restartPosition;
    }
}

void XmTrackerPlayer::processRetrigger(XmTrackerChannel &channel, uint8_t interval, uint8_t slide)
{
    if (!channel.retrigger.interval) return;
    if (!ticks) channel.retrigger.phase = 0;

    if (channel.retrigger.phase >= channel.retrigger.interval) {
        channel.retrigger.phase = 0;
        channel.start = true;

        if (slide >= 1 && slide <= 5) {
            decrementVolume(channel.volume, 1 << (slide - 1));
        } else if (slide >= 9 && slide <= 0xD) {
            incrementVolume(channel.volume, 1 << (slide - 9));
        } else if (slide == 6) {
            channel.volume = channel.volume * 2 / 3;
        } else if (slide == 7) {
            channel.volume /= 2;
        } else if (slide == 0xE) {
            incrementVolume(channel.volume, channel.volume / 2);
        } else if (slide == 0xF) {
            incrementVolume(channel.volume, channel.volume);
        }
    }

    channel.retrigger.phase++;
}

bool XmTrackerPlayer::processTremor(XmTrackerChannel &channel)
{
    if (!ticks) return true;
    // Note this is called from commit(), not processEffects().
    if (channel.note.effectType != fxTremor) return true;

    channel.tremor.phase = (channel.tremor.phase + 1) %
                           (channel.tremor.on + channel.tremor.off);

    // If both parameters are zero, fast tremor.
    if (!channel.tremor.on && !channel.tremor.off) return !(ticks % 2);

    if (channel.tremor.phase > channel.tremor.on) return false;

    return true;
}

void XmTrackerPlayer::processEffects(XmTrackerChannel &channel)
{
    uint8_t &type = channel.note.effectType;
    uint8_t &param = channel.note.effectParam;
    switch (type) {
        case fxArpeggio: {
            processArpeggio(channel);
            break;
        }
        case fxPortaUp: {
            // ProTracker 2/3 does not support memory.
            if (!ticks && param) channel.portaUp = param;
            if (ticks) {
                channel.period -= channel.portaUp * 4;
                channel.frequency = getFrequency(channel.period);
            }
            break;
        }
        case fxPortaDown: {
            // ProTracker 2/3 does not support memory.
            if (!ticks && param) channel.portaDown = param;
            if (ticks) {
                channel.period += channel.portaDown * 4;
                channel.frequency = getFrequency(channel.period);
            }
            break;
        }
        case fxTonePorta: {
            if (!ticks && param) channel.tonePorta = param;
            processPorta(channel);
            break;
        }
        case fxVibrato: {
            if (!ticks) {
                if (param & 0xF0) channel.vibrato.speed = (param & 0xF0) >> 4;
                if (param & 0x0F) channel.vibrato.depth = param & 0x0F;
            }
            processVibrato(channel);
            break;
        }
        case fxTonePortaAndVolumeSlide: {
            /* Strictly speaking, if both nibbles in the parameter are nonzero,
             * this command is illegal.
             */
            if (!ticks && param) channel.slide = param;
            processVolumeSlide(channel.volume, channel.slide);
            processPorta(channel);
            break;
        }
        case fxVibratoAndVolumeSlide: {
            /* Strictly speaking, if both nibbles in the parameter are nonzero,
             * this command is illegal.
             */
            if (param) channel.slide = param;
            processVolumeSlide(channel.volume, channel.slide);
            processVibrato(channel);
            break;
        }
        case fxTremolo: {
            if (!ticks) {
                channel.tremolo.volume = channel.volume;
                if (param & 0xF0) channel.tremolo.speed = (param & 0xF0) >> 4;
                if (param & 0x0F) channel.tremolo.depth = param & 0x0F;
            }
            processTremolo(channel);
            break;
        }
        case fxSampleOffset: {
            if (ticks) break;
            channel.offset = param << 8;
            // Compensate for sample compression (param is in shorts, offset is in samples)
            channel.offset *= channel.instrument.compression;
            break;
        }
        case fxVolumeSlide: {
            /* Unlike the volume slide combination effects, both nibbles in the
             * parameter are allowed to be set.
             */
            if (param) channel.slide = param;
            processVolumeSlide(channel.volume, channel.slide);
            break;
        }
        case fxPositionJump: {
            // Only useful at the start of a note
            ASSERT_BREAK(!ticks);
            type = XmTrackerPattern::kNoEffect;

            processPatternBreak(param, 0);
            break;
        }
        case fxSetVolume: {
            channel.volume = MIN(param, kMaxVolume);
            break;
        }
        case fxPatternBreak: {
            // Only useful at the start of a note
            ASSERT_BREAK(!ticks);
            type = XmTrackerPattern::kNoEffect;

            // Seriously, the spec says the higher order nibble is * 10, not * 16.
            processPatternBreak(-1, (param & 0xF) + (param >> 4) * 10);
            break;
        }
        case fxSetTempoAndBPM: {
            // Only useful at the start of a note
            ASSERT_BREAK(!ticks);
            type = XmTrackerPattern::kNoEffect;

            if (!param) {
                stop();
                return;
            }
            else if (param <= 32)
                tempo = param;
            else
                bpm = param;
            break;
        }
        case fxSetGlobalVolume: {
            volume = param;
            break;
        }
        case fxGlobalVolumeSlide: {
            processVolumeSlide(volume, param);
            break;
        }
        case fxSetEnvelopePos: {
            LOG(("%s:%d: NOT_TESTED: fxSetEnvelopePos fx(0x%02x)\n", __FILE__, __LINE__, type));
            if (!ticks) {
                if (param >= channel.instrument.nVolumeEnvelopePoints) {
                    LOG((LGPFX"Warning: Position %u is out of bounds "
                         "(envelope size: %u), disabling envelope.\n",
                         param, channel.instrument.nVolumeEnvelopePoints));
                    channel.envelope.done = true;
                    channel.envelope.point = 0;
                } else {
                    channel.envelope.point = param;
                }
            }
            break;
        }
        case fxMultiRetrigNote: {
            if (param & 0xF0) channel.retrigger.speed = (param & 0xF0) >> 4;
            if (param & 0x0F) channel.retrigger.interval = param & 0x0F;
            processRetrigger(channel, channel.retrigger.interval, channel.retrigger.speed);
            break;
        }
        case fxTremor: {
            if (ticks) break;
            if (param & 0xF0) channel.tremor.on = (param & 0xF0) >> 4;
            if (param & 0x0F) channel.tremor.off = param & 0x0F;
            LOG(("%s:%d: NOT_TESTED: fxTremor fx(0x%02x)\n", __FILE__, __LINE__, type));
            break;
        }
        case fxExtraFinePorta: {
            if (param >> 4 == 1) {
                if (!ticks) channel.period -= (param & 0x0F);
            } else if (param >> 4 == 2) {
                if (!ticks) channel.period += (param & 0x0F);
            }
            break;
        }
        case fxOverflow: {
            uint8_t nparam = param & 0x0F;
            switch (param >> 4) {
                case fxFinePortaUp: {
                    if (!ticks) channel.period -= nparam * 4;
                    break;
                }
                case fxFinePortaDown: {
                    if (!ticks) channel.period += nparam * 4;
                    break;
                }
                case fxGlissControl: {
                    LOG(("%s:%d: NEVER_IMPLEMENTED: fxGlissControl fx(0x%02x, 0x%02x)\n", __FILE__, __LINE__, type, param));
                    break;
                }
                case fxVibratoControl: {
                    if (ticks) break;
                    LOG(("%s:%d: NOT_TESTED: fxVibratoControl fx(0x%02x, 0x%02x)\n", __FILE__, __LINE__, type, param));
                    channel.vibrato.type = nparam;
                    break;
                }
                case fxFinetune: {
                    LOG(("%s:%d: NOT_TESTED: fxFinetune fx(0x%02x, 0x%02x)\n", __FILE__, __LINE__, type, param));
                    if (param & 0x8) {
                        // Signed nibble
                        channel.instrument.finetune -= 16 * ((~nparam & 0x0F) + 1);
                    } else {
                        channel.instrument.finetune += 16 * nparam;
                    }
                    break;
                }
                case fxLoopPattern: {
                    if (ticks) break;
                    LOG(("%s:%d: NOT_TESTED: fxLoopPattern fx(0x%02x, 0x%02x)\n", __FILE__, __LINE__, type, param));
                    ASSERT_BREAK(!next.force);

                    if (!nparam) {
                        // Remember new boundary
                        loop.start = next.row - 1;
                    } else if (!loop.i) {
                        // Begin looping
                        loop.end = next.row - 1;
                        loop.limit = nparam;
                        loop.i = 1;
                    } else if (++loop.i > loop.limit) {
                        // Stop looping
                        loop.i = 0;
                    }
                    break;
                }
                case fxTremoloControl: {
                    LOG(("%s:%d: NEVER_IMPLEMENTED: fxTremoloControl fx(0x%02x, 0x%02x)\n", __FILE__, __LINE__, type, param));
                    break;
                }
                case fxRetrigNote: {
                    LOG(("%s:%d: NOT_TESTED: fxRetrigNote fx(0x%02x, 0x%02x)\n", __FILE__, __LINE__, type, param));
                    processRetrigger(channel, nparam);
                    break;
                }
                case fxFineVolumeSlideUp: {
                    // Only useful at the start of a note
                    ASSERT_BREAK(!ticks);
                    type = XmTrackerPattern::kNoEffect;

                    // Save parameter for later use, shared with fxFineVolumeSlideDown.
                    if (nparam) channel.fineSlideUp = nparam;
                    incrementVolume(channel.volume, channel.fineSlideUp);
                    break;
                }
                case fxFineVolumeSlideDown: {
                    // Only useful at the start of a note
                    ASSERT_BREAK(!ticks);
                    type = XmTrackerPattern::kNoEffect;

                    // Save parameter for later use, shared with fxFineVolumeSlideUp.
                    if (nparam) channel.fineSlideDown = nparam;
                    decrementVolume(channel.volume, channel.fineSlideDown);
                    break;
                }
                case fxNoteCut: {
                    if (ticks == nparam) channel.volume = 0;
                    break;
                }
                case fxNoteDelay: {
                    channel.start = ticks == nparam;
                    break;
                }
                case fxPatternDelay: {
                    delay = nparam;
                    break;
                }
                default: {
                    Event::setBasePending(_SYS_BASE_TRACKER, (type << 8) | param);
                    type = XmTrackerPattern::kNoEffect;
                    break;
                }
            }
            break;
        }
        default: {
            Event::setBasePending(_SYS_BASE_TRACKER, (type << 8) | param);
            type = XmTrackerPattern::kNoEffect;
            break;
        }
        case XmTrackerPattern::kNoEffect: {
            break;
        }
    }
}

void XmTrackerPlayer::processEnvelope(XmTrackerChannel &channel)
{
    if (!channel.instrument.volumeType || channel.envelope.done) {
        return;
    }

    // Save some space in my editor.
    _SYSXMInstrument &instrument = channel.instrument;
    struct XmTrackerEnvelopeMemory &envelope = channel.envelope;

    if (!instrument.nVolumeEnvelopePoints) {
        ASSERT(instrument.nVolumeEnvelopePoints);
        channel.envelope.done = true;
        return;
    }

    if ((instrument.volumeType & kEnvelopeSustain) && channel.note.note != XmTrackerPattern::kNoteOff) {
        // volumeSustainPoint is a maxima, don't exceed it.
        if (envelope.point >= instrument.volumeSustainPoint) {
            envelope.point = instrument.volumeSustainPoint;
            envelope.tick = 0;
        }
    } else if (instrument.volumeType & kEnvelopeLoop) {
        // Loop the loop
        if (envelope.point >= instrument.volumeLoopEndPoint) {
            envelope.point = instrument.volumeLoopStartPoint;
            envelope.tick = 0;
        }
    }

    int16_t pointLength = 0;
    uint16_t envPt0 , envPt1 = 0;
    if (envelope.point == instrument.nVolumeEnvelopePoints - 1) {
        // End of envelope
        envelope.done = true;

        // Load the last envelope point from flash.
        SvmMemory::VirtAddr va = instrument.volumeEnvelopePoints + envelope.point * sizeof(uint16_t);
        if (!SvmMemory::copyROData(envPt0, va)) {
            LOG((LGPFX"Error: Could not copy %p (length %lu)!\n",
                 (void *)va, (long unsigned)sizeof(envPt0)));
            ASSERT(false); stop(); return;
        }
    } else {
        if (envelope.point >= instrument.nVolumeEnvelopePoints) {
            ASSERT(envelope.point < instrument.nVolumeEnvelopePoints);
            envelope.point = instrument.nVolumeEnvelopePoints - 1;
            processEnvelope(channel);
            return;
        }

        // Load current and next envelope points from flash.
        FlashBlockRef ref;
        SvmMemory::VirtAddr va = instrument.volumeEnvelopePoints + envelope.point * sizeof(uint16_t);
        if (!SvmMemory::copyROData(ref, envPt0, va)) {
            LOG((LGPFX"Error: Could not copy %p (length %lu)!\n",
                 (void *)va, (long unsigned)sizeof(envPt0)));
            ASSERT(false); stop(); return;
        }
        if (!SvmMemory::copyROData(ref, envPt1, va + sizeof(uint16_t))) {
            LOG((LGPFX"Error: Could not copy %p (length %lu)!\n",
                 (void *)(va + sizeof(uint16_t)), (long unsigned)sizeof(envPt1)));
            ASSERT(false); stop(); return;
        }

        pointLength = envelopeOffset(envPt1) - envelopeOffset(envPt0);
    }

    if (envelope.point >= instrument.nVolumeEnvelopePoints - 1 || !envelope.tick) {
        // Beginning of node/end of envelope, no interpolation.
        envelope.value = envelopeValue(envPt0);
    } else {
        /* Interpolates unnecessarily with tick == pointLength, but specifically
         * catching that case isn't really worth it.
         */
        int16_t v1 = envelopeValue(envPt0);
        int16_t v2 = envelopeValue(envPt1);
        envelope.value = v1 + envelope.tick * (v2 - v1) / pointLength;
    }

    // Progress!
    if (envelope.point < instrument.nVolumeEnvelopePoints - 1) {
        envelope.tick++;
        if (envelope.tick >= pointLength) {
            envelope.tick = 0;
            envelope.point++;
        }
    }
}

void XmTrackerPlayer::process()
{
    for (unsigned i = 0; i < song.nChannels; i++) {
        struct XmTrackerChannel &channel = channels[i];

        processVolume(channel);
        if (!isPlaying()) return;
        processEffects(channel);
        if (!isPlaying()) return;
        processEnvelope(channel);
        if (!isPlaying()) return;
    }
}

void XmTrackerPlayer::commit()
{
    AudioMixer &mixer = AudioMixer::instance;

    for (unsigned i = 0; i < song.nChannels; i++) {
        struct XmTrackerChannel &channel = channels[i];

        if (!channel.active && mixer.isPlaying(CHANNEL_FOR(i))) {
            mixer.stop(CHANNEL_FOR(i));
            continue;
        }

        /* This code relies on the single-threaded nature of the audio
         * stack--the sample is loaded first and volume and sampling rate
         * adjusted shortly after.
         */
        if (channel.start) {
            channel.start = false;
            if (mixer.isPlaying(CHANNEL_FOR(i))) mixer.stop(CHANNEL_FOR(i));
            if (!mixer.play(&channel.instrument.sample,
                            CHANNEL_FOR(i),
                            (_SYSAudioLoopType)channel.instrument.sample.loopType)) {
                channel.active = false;
                continue;
            }
        }

        channel.active = mixer.isPlaying(CHANNEL_FOR(i));

        /* Final volume is computed from the current channel volume, the
         * current state of the instrument's volume envelope, sample fadeout,
         * global volume, user-assigned channel volume, and user-assigned
         * global volume.
         */
        int32_t finalVolume = 0;
        if (processTremor(channel)) {
            finalVolume = channel.volume;

            // ProTracker 2/3 compatibility. FastTracker II maintains final tremolo volume
            if (channel.note.effectType != fxTremolo) channel.tremolo.volume = finalVolume;

            // Apply envelope
            if (channel.active && channel.instrument.volumeType) {
                finalVolume = (finalVolume * channel.envelope.value) >> 6;
            }

            // Apply fadeout
            if (channel.active && channel.note.note == XmTrackerPattern::kNoteOff) {
                if (channel.instrument.volumeFadeout > 0xFFF ||
                    channel.instrument.volumeFadeout > channel.fadeout ||
                    !channel.instrument.volumeFadeout)
                {
                    channel.fadeout = 0;
                } else {
                    channel.fadeout -= channel.instrument.volumeFadeout;
                }

                // Completely faded -> stop playing sample.
                if (!channel.fadeout) {
                    channel.active = false;
                    if (mixer.isPlaying(CHANNEL_FOR(i))) mixer.stop(CHANNEL_FOR(i));
                }

                // Apply fadeout
                finalVolume = finalVolume * channel.fadeout / UINT16_MAX;
            }
        }

        // Global volume
        finalVolume = finalVolume * volume / kMaxVolume;
        // User-assigned channel volume
        finalVolume = finalVolume * channel.userVolume / _SYS_AUDIO_MAX_VOLUME;
        // User-assigned global volume
        finalVolume = finalVolume * userVolume / _SYS_AUDIO_MAX_VOLUME;

        mixer.setVolume(CHANNEL_FOR(i), clamp(finalVolume * 4, (int32_t)0, (int32_t)_SYS_AUDIO_MAX_VOLUME));

        // Sampling rate
        if (channel.frequency > 0) {
            mixer.setSpeed(CHANNEL_FOR(i), channel.frequency);
        } else if (mixer.isPlaying(CHANNEL_FOR(i))) {
            mixer.stop(CHANNEL_FOR(i));
        }

        // Offset
        if (channel.offset < UINT32_MAX) {
            mixer.setPos(CHANNEL_FOR(i), channel.offset);
            channel.offset = UINT32_MAX;
        }
    }

    // Call back at (24 / 60 * bpm) Hz
    mixer.setTrackerCallbackInterval(2500000 / bpm);
}

uint8_t XmTrackerPlayer::patternOrderTable(uint16_t order)
{
    if (order >= song.patternOrderTableSize) {
        LOG((LGPFX"Error: Order %u is larger than pattern order table (%u entries)\n",
             order, song.patternOrderTableSize));
        ASSERT(order < song.patternOrderTableSize);
        stop();
        return song.restartPosition < song.patternOrderTableSize
               ? song.restartPosition
               : 0;
    }

    uint8_t buf;
    SvmMemory::VirtAddr va = song.patternOrderTable + order;
    if(!SvmMemory::copyROData(buf, va)) {
        LOG((LGPFX"Error: Could not copy %p (length %lu)!\n",
             (void *)va, (long unsigned)sizeof(buf)));
        ASSERT(false);
        stop();
        return song.restartPosition < song.patternOrderTableSize
               ? song.restartPosition
               : 0;
    }
    return buf;
}

void XmTrackerPlayer::tick()
{
    if (++ticks >= tempo * (delay + 1)) {
        ticks = delay = 0;
        // load next notes into the process channels
        loadNextNotes();
        if (!isPlaying()) return;
    }

    // process effects and envelopes
    process();
    if (!isPlaying()) return;

    // update mixer
    commit();
    //if (!isPlaying()) return;
}
