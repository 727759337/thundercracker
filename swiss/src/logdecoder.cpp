/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * swiss - your Sifteo utility knife
 *
 * Copyright <c> 2012 Sifteo, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <string.h>
#include "macros.h"
#include "logdecoder.h"

/*
 * XXX: This is copied from mc_logdecoder.cpp/.h in siftulator.
 *      Factor out and share.
 */

void LogDecoder::init(bool aFlushLogs)
{
    flushLogs = aFlushLogs;
    scriptType = _SYS_SCRIPT_NONE;
    scriptBuffer.clear();
    handlers.clear();
}

void LogDecoder::formatLog(ELFDebugInfo &DI,
    char *out, size_t outSize, char *fmt, const uint32_t *args, size_t argCount)
{
    // This is a simple printf()-like formatter, used to format log entries.
    // It differs from printf() in two important ways:
    //
    //   - We enforce a subset of functionality which is both safe and
    //     does not require runtime memory access. This means only int, float,
    //     and character arguments are permitted.
    //
    //   - Unlike C's printf(), our floating point arguments are single
    //     precision, and always stored in a single 32-bit argument slot.
    //
    // We don't worry too much about nice error reporting here, since the
    // format string should have already been validated by slinky. We only
    // perform the minimum checks necessary to ensure runtime safety.

    char *outEnd = out + outSize - 1;
    *outEnd = '\0';

    while (out < outEnd) {
        char *segment = fmt;
        char c = *(fmt++);

        if (c != '%') {
            *(out++) = c;
            if (c)
                continue;
            break;
        }

        bool done = false;
        bool dot = false;
        bool leadingZero = false;
        int width = 0;
        do {
            char spec = *(fmt++);
            char saved = *fmt;
            *fmt = '\0';  // NUL terminate the current segment

            switch (spec) {

                // Prefix characters
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                    if (width == 0 && spec == '0')
                        leadingZero = true;
                    width = (width * 10) + (spec - '0');
                    break;

                case ' ':
                case '-':
                case '+':
                    break;

                case '.':
                    dot = true;
                    break;

                // Literal '%'
                case '%':
                    *(out++) = '%';
                    done = true;
                    break;

                // Pointers, formatted as 32-bit hex values
                case 'p':
                    done = true;
                    ASSERT(argCount && "Too few arguments in format string");
                    out += snprintf(out, outEnd - out, "0x%08x", *args);
                    argCount--;
                    args++;
                    break;

                // Pointer, formatted as a resolved symbol
                case 'P':
                    done = true;
                    ASSERT(argCount && "Too few arguments in format string");
                    out += snprintf(out, outEnd - out, "%s", DI.formatAddress(*args).c_str());
                    argCount--;
                    args++;
                    break;

                // Binary integers
                case 'b':
                    done = true;
                    ASSERT(argCount && "Too few arguments in format string");
                    for (int i = 31; i >= 0 && out != outEnd; --i) {
                        unsigned bits = (*args) >> i;

                        if (bits != 0) {
                            // There's a '1' at or to the left of the current bit
                            out += snprintf(out, outEnd - out, "%d", bits & 1);
                        } else if (i < width) {
                            // Pad with blanks or zeroes
                            *(out++) = leadingZero ? '0' : ' ';
                        }
                    }
                    argCount--;
                    args++;
                    break;

                // Integer parameters, passed to printf()
                case 'd':
                case 'i':
                case 'o':
                case 'u':
                case 'X':
                case 'x':
                case 'c':
                    done = true;
                    ASSERT(argCount && "Too few arguments in format string");
                    out += snprintf(out, outEnd - out, segment, *args);
                    argCount--;
                    args++;
                    break;

                // 32-bit float parameters, passed to printf()
                case 'f':
                case 'F':
                case 'e':
                case 'E':
                case 'g':
                case 'G':
                    done = true;
                    ASSERT(argCount && "Too few arguments in format string");
                    out += snprintf(out, outEnd - out, segment,
                        (double) reinterpret_cast<const float&>(*args));
                    argCount--;
                    args++;
                    break;

                // 'C' specifier: Four characters packed into a 32-bit int.
                // Stops when it hits a NUL.
                case 'C':
                    done = true;
                    for (unsigned i = 0; outEnd != out && i < 4; i++) {
                        char c = (*args) >> (i * 8);
                        if (!c)
                            break;
                        *(out++) = c;
                    }
                    argCount--;
                    args++;
                    break;

                case 0:
                    ASSERT(0 && "Premature end of format string");
                    break;

                default:
                    ASSERT(0 && "Unsupported character in format string");
                    return;
            }

            *fmt = saved;   // Restore next character
        } while (!done);
    }
}

size_t LogDecoder::decode(FILE *f, ELFDebugInfo &DI, SvmLogTag tag, const uint32_t *buffer)
{
    char outBuffer[1024];

    switch (tag.getType()) {

        // Stow all arguments, plus the log tag. The post-processor
        // will do some printf()-like formatting on the stored arguments.
        case _SYS_LOGTYPE_FMT: {
            std::string fmt = DI.readString(".debug_logstr", tag.getParam());
            if (fmt.empty()) {
                LOG(("SVMLOG: No symbol table found. Raw data:\n"
                     "\t[%08x] %08x %08x %08x %08x %08x %08x %08x\n",
                     tag.getValue(), buffer[0], buffer[1], buffer[2],
                     buffer[3], buffer[4], buffer[5], buffer[6]));
            } else {
                formatLog(DI, outBuffer, sizeof outBuffer, (char*) fmt.c_str(),
                    buffer, tag.getArity());
                writeLog(f, outBuffer);
            }
            return tag.getArity() * sizeof(uint32_t);
        }

        // Print a string from the log buffer
        case _SYS_LOGTYPE_STRING: {
            uint32_t bytes = MIN(tag.getParam(), sizeof outBuffer);
            ASSERT(bytes <= SvmDebugPipe::LOG_BUFFER_BYTES);
            memcpy(outBuffer, buffer, bytes);
            outBuffer[bytes] = '\0';
            writeLog(f, outBuffer);
            return bytes;
        }

        // Fixed width hex-dump
        case _SYS_LOGTYPE_HEXDUMP: {
            uint32_t bytes = tag.getParam();
            ASSERT(bytes <= SvmDebugPipe::LOG_BUFFER_BYTES);
            for (unsigned i = 0; i != bytes; i++) {
                snprintf(outBuffer, sizeof outBuffer,
                    "%02x", ((uint8_t*)buffer)[i]);
                writeLog(f, outBuffer);
            }
            return bytes;
        }

        // Begin/end a script block
        case _SYS_LOGTYPE_SCRIPT: {
            if (scriptType != _SYS_SCRIPT_NONE)
                runScript();
            scriptType = tag.getParam();
            scriptBuffer.clear();
            return 0;
        }

        default:
            ASSERT(0 && "Decoding unknown log type. (syscall layer should have caught this!)");
            return 0;
    }
}

void LogDecoder::writeLog(FILE *f, const char *str)
{
    if (scriptType == _SYS_SCRIPT_NONE) {
        fprintf(f, "%s", str);
        if (flushLogs) {
            fflush(f);
        }
    } else {
        scriptBuffer += str;
    }
}

void LogDecoder::runScript()
{
    std::map<unsigned, ScriptHandler>::iterator I = handlers.find(scriptType);
    if (I == handlers.end()) {
        LOG(("LOG: Unsupported script type %d\n", scriptType));
    } else {
        I->second.func(scriptBuffer.c_str(), I->second.context);
    }
}

void LogDecoder::setScriptHandler(unsigned type, ScriptHandler handler)
{
    handlers[type] = handler;
}
