/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#include <stdio.h>
#include <protocol.h>
#include <sifteo/machine.h>

#include "cubecodec.h"
#include "cubeslots.h"
#include "svmmemory.h"

using namespace Sifteo;
using namespace Sifteo::Intrinsic;


void CubeCodec::encodeVRAM(PacketBuffer &buf, _SYSVideoBuffer *vb)
{
    /*
     * Note that we have to sweep that change map as we go. Since
     * we're running in an ISR, at higher priority than user code,
     * it's possible for us to run between a vram modification and a
     * cm update (totally safe) but it isn't possible for any user
     * code to run during this ISR. So there's no need to worry about
     * atomicity in updating the cm here.
     *
     * Also note that we don't need to keep a separate pointer to
     * track our scan location in VRAM, since it's very cheap to use
     * our changemap to locate the first word in VRAM that needs
     * to be sent.
     *
     * Because of this, we effectively do a top-down scan of VRAM for
     * every RF packet. If something earlier on in the framebuffer was
     * changed by userspace, we'll go back and resend it before
     * sending later portions of the buffer. That's good in a way,
     * since it gives us a way to set a kind of QoS priority based on
     * VRAM address. But it means that userspace needs to consciously
     * delay writing to the changemap if it wants to avoid sending
     * redundant or out-of-order updates over the radio while a large
     * or multi-part update is in progress.
     *
     * This loop terminates when all of VRAM has been flushed, or when
     * we fill up the output packet. We assume that this function
     * begins with space available in the packet buffer.
     */

    // Emit buffered bits from the previous packet
    txBits.flush(buf);

    if (vb) {
        do {
            /*
             * This AND is important, it prevents out-of-bounds indexing
             * or infinite looping if userspace gives us a bogus cm32
             * value! This is equivalent to bounds-checking idx32 and addr
             * below, as well as checking for infinite looping. Much more
             * efficient to do it here though :)
             */
            uint32_t cm32 = vb->cm32 & 0xFFFF0000;

            if (!cm32)
                break;

            uint32_t idx32 = CLZ(cm32);
            ASSERT(idx32 < arraysize(vb->cm1));
            uint32_t cm1 = vb->cm1[idx32];

            if (cm1) {
                uint32_t idx1 = CLZ(cm1);
                uint16_t addr = (idx32 << 5) | idx1;

                ASSERT(addr < _SYS_VRAM_WORDS);
                CODEC_DEBUG_LOG(("-encode addr %04x, data %04x\n", addr, vb->vram.words[addr]));

                if (!encodeVRAMAddr(buf, addr) ||
                    !encodeVRAMData(buf, vb, VRAM::peek(*vb, addr))) {

                    /*
                     * We ran out of room to encode. This should be rare,
                     * happening only when we're near the end of the
                     * packet buffer AND we're encoding a very large code,
                     * like a literal 16-bit write plus a literal address
                     * change.
                     */
                    break;
                }

                cm1 &= ROR(0x7FFFFFFF, idx1);
                vb->cm1[idx32] = cm1;
            }

            if (!cm1) {
                cm32 &= ROR(0x7FFFFFFF, idx32);
                vb->cm32 = cm32;
            }
        } while (!buf.isFull());
    }

    /*
     * If we have room in the buffer, and nothing left to render,
     * see if we can flush leftover bits out to the hardware. We may
     * have residual bits in txBits, and we may have to emit a run.
     */

    // Emit buffered bits
    txBits.flush(buf);

    // Flush out an RLE run, if one is buffered
    if (!buf.isFull()) {
        flushDSRuns(true);
        txBits.flush(buf);
    }
}

bool CubeCodec::encodeVRAMAddr(PacketBuffer &buf, uint16_t addr)
{
    ASSERT(addr < _SYS_VRAM_WORDS);
    ASSERT(codePtr < _SYS_VRAM_WORDS);

    if (addr != codePtr) {
        uint16_t delta = (addr - codePtr) & _SYS_VRAM_WORD_MASK;

        flushDSRuns(true);
        txBits.flush(buf);
        if (buf.isFull())
            return false;

        if (delta <= 8) {
            // We can use a short skip code

            CODEC_DEBUG_LOG((" addr delta %d\n", delta));

            delta--;
            txBits.append((delta & 1) | ((delta << 3) & 0x30), 8);
            txBits.flush(buf);

        } else {
            // Too large a delta, use a longer literal code

            CODEC_DEBUG_LOG((" addr literal %04x\n", addr));

            txBits.append(3 | ((addr >> 4) & 0x10) | (addr & 0xFF) << 8, 16);
            txBits.flush(buf);
        }

        codePtr = addr;
    }

    return true;
}

bool CubeCodec::encodeVRAMData(PacketBuffer &buf, _SYSVideoBuffer *vb, uint16_t data)
{
    if (buf.isFull())
        return false;

    /*
     * For debugging, the delta encoder can be disabled, forcing us to
     * use only literal codes. This makes the compression code a lot
     * simpler, but the resulting radio traffic will be extremely
     * inefficient.
     */
#ifndef DISABLE_DELTAS

    /*
     * See if we can encode this word as a delta or copy from one of
     * our four sample points.  If we find a copy, that always wins
     * and we can exit early. Otherwise, see if any of the deltas are
     * small enough to encode.
     */

    unsigned s0 = deltaSample(vb, data, RF_VRAM_SAMPLE_0);
    if (s0 == RF_VRAM_DIFF_BASE) {
        encodeDS(s0, 0);
        txBits.flush(buf); 
        return true;
    }
    
    unsigned s1 = deltaSample(vb, data, RF_VRAM_SAMPLE_1);
    if (s1 == RF_VRAM_DIFF_BASE) {
        encodeDS(s1, 1);
        txBits.flush(buf); 
        return true;
    }

    unsigned s2 = deltaSample(vb, data, RF_VRAM_SAMPLE_2);
    if (s2 == RF_VRAM_DIFF_BASE) {
        encodeDS(s2, 2);
        txBits.flush(buf); 
        return true;
    }

    unsigned s3 = deltaSample(vb, data, RF_VRAM_SAMPLE_3);
    if (s3 == RF_VRAM_DIFF_BASE) {
        encodeDS(s3, 3);
        txBits.flush(buf); 
        return true;
    }

    if (s0 < 0x10) {
        encodeDS(s0, 0);
        txBits.flush(buf); 
        return true;
    }
    if (s1 < 0x10) {
        encodeDS(s1, 1);
        txBits.flush(buf); 
        return true;
    }
    if (s2 < 0x10) {
        encodeDS(s2, 2);
        txBits.flush(buf); 
        return true;
    }
    if (s3 < 0x10) {
        encodeDS(s3, 3);
        txBits.flush(buf); 
        return true;
    }

#endif // !DISABLE_DELTAS

    /*
     * No delta found. Encode as a literal.
     */

    if (data & 0x0101) {
        // 16-bit literal

        flushDSRuns(true);
        txBits.flush(buf);
        if (buf.isFull())
            return false;

        CODEC_DEBUG_LOG((" data literal-16 %04x\n", data));

        txBits.append(0x23 | (data << 8), 24);
        txBits.flush(buf);

        codePtrAdd(1);
        codeS = 0;
        codeD = RF_VRAM_DIFF_BASE;

    } else {
        // 14-bit literal

        /*
         * We're guaranteeing that this flushDSRuns will be
         * immediately followed by the literal code. We can't return
         * false between the two, since that gives up our ability to
         * keep this sequence of codes atomic.
         */
        flushDSRuns(false);

        uint16_t index = ((data & 0xFF) >> 1) | ((data & 0xFF00) >> 2);
        CODEC_DEBUG_LOG((" data literal-14 %04x\n", index));

        txBits.append(0xc | (index >> 12) | ((index & 0xFFF) << 4), 16);
        txBits.flush(buf);

        codePtrAdd(1);
        codeS = 0;
        codeD = RF_VRAM_DIFF_BASE;
    }

    return true;
}

void CubeCodec::encodeDS(uint8_t d, uint8_t s)
{
    ASSERT(codeRuns <= RF_VRAM_MAX_RUN);

    if (d == codeD && s == codeS && codeRuns != RF_VRAM_MAX_RUN) {
        // Extend an existing run
        codeRuns++;

    } else {
        flushDSRuns(false);

        CODEC_DEBUG_LOG((" ds %d %d\n", d, s));
        appendDS(d, s);
        codeD = d;
        codeS = s;
    }

    codePtrAdd(1);
}

void CubeCodec::flushDSRuns(bool rleSafe)
{
    /*
     * Emit a run code.
     *
     * Because we treat doubled run codes as a short form of escape, we
     * have to be very careful about which codes we emit immediately after
     * a flushDSRuns(). It is ONLY safe to emit an RLE code if the next
     * code we emit is a copy, a diff, or a literal index code.
     *
     * If 'rleSafe' is false, the caller is guaranteeing that the next
     * code is already safe. If not, we will perform a simple transformation
     * on the run code. The last run will be converted back to a non-RLE
     * copy or diff code.
     *
     * It is NOT acceptable to call flushDSRuns with rleSafe==FALSE then
     * to return due to an end-of-packet. We can't guarantee that the next
     * packet will resume with the same kind of code, so the caller isn't
     * upholding its side of this contract.
     */

    ASSERT(codeRuns <= RF_VRAM_MAX_RUN);

    if (codeRuns) {
        CODEC_DEBUG_LOG((" flush-ds d=%d s=%d x%d, rs=%d\n", codeD, codeS, codeRuns, rleSafe));

        // Save room for the trailing non-RLE code
        if (rleSafe)
            codeRuns--;

        if (codeRuns) {
            uint8_t r = codeRuns - 1;
            codeRuns = 0;
            
            if (r < 4) {
                // Short run
                txBits.append(r, 4);
            } else {
                // Longer run
                r -= 4;
                txBits.append(0x2 | ((r << 8) & 0xF00) | (r & 0x30), 12);
            }
        }

        // Trailing non-RLE code
        if (rleSafe)
            appendDS(codeD, codeS);

        // Can't emit another run immediately after a flush
        codeD = -1;
    }
}

bool CubeCodec::flashReset(PacketBuffer &buf)
{
    // No room in output buffer
    if (!txBits.hasRoomForFlush(buf, 12))
        return false;

    loadBufferAvail = FLS_FIFO_USABLE;

    // Must be the last full byte in the packet to trigger a reset.
    flashEscape(buf);
    
    return true;
}

bool CubeCodec::flashSend(PacketBuffer &buf, _SYSAssetGroup *group,
    _SYSAssetGroupCube *ac, _SYSCubeIDVector cubeBit)
{
    /*
     * Since we're dealing with asset group pointers as well as
     * per-cube state that reside in untrusted memory, this code
     * has to be carefully written to read each user value exactly
     * once, and check it before use.
     *
     * We only do this if we have asset data, obviously, but also
     * if the cube has enough buffer space to accept it, and if
     * there's enough room in the packet for both the escape code
     * and at least one byte of flash data.
     *
     * After this initial check, any further checks exist only as
     * protection against buggy or malicious user code.
     *
     * Returns 'true' if we finish sending an asset group.
     */

    /*
     * We have a state bit in CubeSlots to keep track of whether we
     * need to send an addressing command to the decoder, so that it
     * knows this asset group's load address. Addressing commands are
     * part of the loadstream that we send over the radio, but they're
     * created dynamically rather than coming straight from flash.
     */
     bool flashAddrPending = (CubeSlots::flashAddrPending & cubeBit) != 0;

    /*
     * How much space do we need? The minimum unit of asset data is
     * just a byte, but an addressing command is three bytes. If we don't
     * have enough, we need to be able to determine this before sending a
     * flash escape. Once we send the escape, we're committed.
     */
    const unsigned escapeSizeInBits = 12;
    unsigned dataSizeInBytes = flashAddrPending ? 3 : 1;
    unsigned dataSizeInBits = dataSizeInBytes << 3;
    if (!txBits.hasRoomForFlush(buf, escapeSizeInBits + dataSizeInBits))
        return false;

    // The cube also must have enough buffer space to receive our minimum data
    if (loadBufferAvail < dataSizeInBytes)
        return false;

    // Per-cube asset state pointer is invalid?
    if (!ac)
        return false;

    // Read (cached) asset group header. Must be valid.
    const _SYSAssetGroupHeader *headerVA =
        reinterpret_cast<const _SYSAssetGroupHeader*>(group->pHdr);
    _SYSAssetGroupHeader header;
    if (!SvmMemory::copyROData(header, headerVA))
        return false;

    // Read 'progress' from untrusted memory only once, and validate it.
    uint32_t progress = ac->progress;
    if (progress >= header.dataSize)
        return false;

    /*
     * The escape command indicates that the entire remainder of 'buf' is
     * data for the flash codec. We can figure out how much data to send
     * now. We're limited by the size of the packet buffer, the size of
     * the data left to send, and the amount of space in the cube's FIFO.
     */

    flashEscape(buf);
    uint32_t count = MIN(buf.bytesFree(), loadBufferAvail);
    ASSERT(count >= dataSizeInBytes);

    /*
     * If we need to send an address command, send that first. If we can
     * still cram in some actual loadstream data, awesome, but this is
     * the only part that must succeed.
     */

    if (flashAddrPending) {
        uint16_t addr = ac->baseAddr;
        ASSERT(buf.bytesFree() >= 3);

        // Opcode, lat1, lat2
        buf.append(0xe1);
        buf.append(addr << 1);
        buf.append((addr >> 6) & 0xfe);

        Atomic::And(CubeSlots::flashAddrPending, ~cubeBit);
        ASSERT(count >= 3);
        ASSERT(loadBufferAvail >= 3);
        count -= 3;
        loadBufferAvail -= 3;
        if (!count)
            return false;
    }

    /*
     * We access flash data through the cache, instead of FlashStream, for
     * important reasons. Even though it may seem like asset loading is
     * a purely streaming operation, it actually isn't:
     *
     *   - When loading to multiple cubes concurrently, the same data is often
     *     reused, and the cache can in fact cut down bus traffic quite a bit.
     *
     *   - We consume loadstream data in potentially very tiny chunks, which
     *     would come with significant overhead if we made separate SPI
     *     bus transactions out of each. By utilizing the block cache, we can
     *     amortize this cost over many packets.
     */

    FlashBlockRef ref;
    SvmMemory::VirtAddr dataVA = reinterpret_cast<SvmMemory::VirtAddr>(headerVA);
    dataVA += header.hdrSize;
    dataVA += progress;

    count = MIN(count, header.dataSize - progress);
    progress += count;
    ASSERT(loadBufferAvail >= count);
    loadBufferAvail -= count;

    while (count) {
        SvmMemory::PhysAddr dataPA;
        uint32_t chunk = count;
        if (!SvmMemory::mapROData(ref, dataVA, chunk, dataPA))
            return false;

        buf.append(dataPA, chunk);
        count -= chunk;
        dataVA += chunk;
    }

    ac->progress = progress;
    ASSERT(progress <= header.dataSize);
    return progress >= header.dataSize;
}
