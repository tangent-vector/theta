// lexer.h
#pragma once

namespace theta
{

enum
{
    kEndOfFile = -1,
};

bool isIdentifierStartChar(int c)
{
    return (('a' <= c) && (c <= 'z'))
        || (('A' <= c) && (c <= 'Z'))
        || (c == '_');
}

bool isIdentifierChar(int c)
{
    return isIdentifierStartChar(c);
}

struct Lexer
{
    void init(StringSpan const& text)
    {
        _cursor = text._begin;
        _end = text._end;
    }

    SourceLoc getLoc() { return _loc; }
    bool isAtEnd() { return _cursor == _end; }

    Token readToken();

private:

    Token::Code readTokenImpl(Token::Value& outValue);

    Token::Code readLineComment();

    int peekChar()
    {
        if (isAtEnd()) return kEndOfFile;
        return *_cursor;
    }
    int readChar()
    {
        if (isAtEnd()) return kEndOfFile;
        return *_cursor++;
    }

    SourceLoc _loc;
    char const* _cursor;
    char const* _end;
};

Token Lexer::readToken()
{
    for (;;)
    {
        Token token;
        token.text._begin = _cursor;
        token.code = readTokenImpl(token.value);
        token.text._end = _cursor;

        switch (token.code)
        {
        default:
            return token;

        case Token::Code::Whitespace:
        case Token::Code::Newline:
        case Token::Code::LineComment:
        case Token::Code::BlockComment:
            continue;
        }
    }
}

Token::Code Lexer::readLineComment()
{
    for (;;)
    {
        int c = peekChar();
        switch (c)
        {
        case kEndOfFile:
        case '\r':
        case '\n':
            return Token::Code::LineComment;

        default:
            readChar();
            continue;
        }
    }
}

Token::Code Lexer::readTokenImpl(Token::Value& outValue)
{
    char const* textStart = _cursor;
    int c = readChar();
    switch (c)
    {
    case kEndOfFile: return Token::Code::EndOfFile;

    case '/':
        {
            switch(peekChar())
            {
            case '/':
                readChar();
                return readLineComment();

            default:
                break;
            }

            outValue.symbol = getSymbol(StringSpan(textStart, _cursor));
            return Token::Code::InfixOperator;
        }
        break;

    case '#': return Token::Code::Hash;
    case '(': return Token::Code::LParen;
    case ')': return Token::Code::RParen;
    case '{': return Token::Code::LCurly;
    case '}': return Token::Code::RCurly;
    case ';': return Token::Code::Semicolon;
    case ':': return Token::Code::Colon;
    case '@': return Token::Code::At;
    case '.': return Token::Code::Dot;

    case '\n': return Token::Code::Newline;
    case '\r':
    {
        if (peekChar() == '\n')
            readChar();
        return Token::Code::Newline;
    }

#define CASE_WHITESPACE \
        case ' ': case '\t'

    CASE_WHITESPACE:
    {
        for (;;)
        {
            switch (peekChar())
            {
            CASE_WHITESPACE:
                readChar();
                continue;

            default:
                break;
            }

            return Token::Code::Whitespace;
        }
    }

    default:
        break;
    }

    if (isIdentifierStartChar(c))
    {
        while (isIdentifierChar(peekChar()))
            readChar();

        outValue.symbol = getSymbol(StringSpan(textStart, _cursor));

        return Token::Code::Identifier;
    }

    error(getLoc(), "unexpected character 'c'", c);
    return Token::Code::InvalidChar;
}

}