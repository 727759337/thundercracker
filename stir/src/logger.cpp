/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * STIR -- Sifteo Tiled Image Reducer
 * M. Elizabeth Scott <beth@sifteo.com>
 *
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include <stdio.h>
#include <stdarg.h>

#include "logger.h"

namespace Stir {

Logger::~Logger() {}
ConsoleLogger::~ConsoleLogger() {}

void ConsoleLogger::setVerbose(bool verbose)
{
    mVerbose = verbose;
}

void ConsoleLogger::taskBegin(const char *name)
{
    if (mVerbose)
	fprintf(stderr, "%s...\n", name);
}

void ConsoleLogger::taskProgress(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);

    if (mVerbose) {
	fprintf(stderr, "\r\t");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "    ");
    }

    va_end(ap);
}

void ConsoleLogger::taskEnd()
{
    if (mVerbose)
	fprintf(stderr, "\n");
}

void ConsoleLogger::infoBegin(const char *name)
{
    if (mVerbose)
	fprintf(stderr, "\n%s:\n", name);
}

void ConsoleLogger::infoLine(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);

    if (mVerbose) {
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
    }

    va_end(ap);
}

void ConsoleLogger::infoEnd()
{}

void ConsoleLogger::error(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    fprintf(stderr, "-!- ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

};  // namespace Stir
