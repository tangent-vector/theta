// value.h
#pragma once

#include "string.h"

namespace theta
{

struct ValueObj
{
    virtual ~ValueObj() {}
};

struct Value
{
public:
    Value()
    {}

    Value(ValueObj* obj)
        : _obj(obj)
    {}

    bool operator==(Value const& other)
    {
        return _obj == other._obj;
    }

    bool operator!=(Value const& other)
    {
        return _obj != other._obj;
    }

    ValueObj* getPtr() const { return _obj; }

private:
    ValueObj* _obj = nullptr;
};

// Symbol

class Symbol : public ValueObj
{
public:
    StringSpan  text;
    Symbol*     next;
};

Symbol* gSymbols = nullptr;

Symbol* getSymbol(StringSpan const& text)
{
    for( auto s = gSymbols; s; s = s->next )
    {
        if(s->text == text)
            return s;
    }

    size_t textSize = text.getSize();

    size_t totalSize = sizeof(Symbol) + textSize + 1;
    void* memory = malloc(totalSize);

    Symbol* symbol = new(memory) Symbol();

    char* textBegin = (char*) (symbol + 1);
    char* textEnd = textBegin + textSize;
    *textEnd = 0;

    memcpy(textBegin, text.getData(), textSize);

    symbol->text = StringSpan(textBegin, textEnd);

    symbol->next = gSymbols;
    gSymbols = symbol;

    return symbol;
}

}