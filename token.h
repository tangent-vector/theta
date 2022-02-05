// token.h
#pragma once

namespace theta
{


#define FOREACH_TOKEN_CODE(X) \
    X(EndOfFile)            \
    X(Whitespace)           \
    X(Newline)              \
    X(LineComment)          \
    X(BlockComment)         \
    X(Identifier)           \
    X(InvalidChar)          \
    X(LCurly)               \
    X(RCurly)               \
    X(LParen)               \
    X(RParen)               \
    X(Semicolon)            \
    X(Colon)                \
    X(At)                   \
    X(Dot)                  \
    X(InfixOperator)        \
    X(Hash)                 \
    X(Comma)                \
    /* end */

struct Token
{
    enum class Code
    {
#define DECLARE_TOKEN_CODE(NAME) NAME,
        FOREACH_TOKEN_CODE(DECLARE_TOKEN_CODE)
#undef DECLARE_TOKEN_CODE
    };

    union Value
    {
        Symbol* symbol;
    };

    Code        code;
    StringSpan  text;
    Value       value;

    SourceRange range;
};

static const char* kTokenCodeNames[] =
{
#define TOKEN_CODE_NAME(NAME) #NAME,
    FOREACH_TOKEN_CODE(TOKEN_CODE_NAME)
#undef TOKEN_CODE_NAME
};

const char* getTokenName(Token::Code code)
{
    return kTokenCodeNames[int(code)];
}

}