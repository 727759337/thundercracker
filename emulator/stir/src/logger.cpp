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

Logger::~Logger() {}
ConsoleLogger::~ConsoleLogger() {}

void ConsoleLogger::taskBegin(const char *name)
{
    fprintf(stderr, "%s...\n", name);
}

void ConsoleLogger::taskProgress(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    fprintf(stderr, "\r\t");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "    ");
    va_end(ap);
}

void ConsoleLogger::taskEnd()
{
    fprintf(stderr, "\n");
}
