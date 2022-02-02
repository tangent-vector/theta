// diagnostics.h
#pragma once

#include "source-manager.h"

namespace theta
{

void error(SourceLoc loc, char const* format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);

    throw 99;
}


}
