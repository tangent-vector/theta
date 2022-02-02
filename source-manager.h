// source-manager.h
#pragma once

#include "string.h"

namespace theta
{

struct SourceLoc
{};

struct SourceRange
{
    SourceLoc begin;
    SourceLoc end;
};

struct SourceFile
{
    char const* _path;
    StringSpan _text;
};

SourceFile* loadSourceFile(char const* path)
{
    FILE* f = fopen(path, "rb");
    if(!f) return nullptr;

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buffer = (char*) malloc(size + 1);

    if( fread(buffer, size, 1, f) != 1 )
    {
        free(buffer);
        return nullptr;
    }
    buffer[size] = 0;

    SourceFile* sourceFile = new SourceFile();
    sourceFile->_path = path;
    sourceFile->_text = StringSpan(buffer, buffer+size);

    return sourceFile;

}

}
