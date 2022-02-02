// parser.h
#pragma once

namespace theta
{
using namespace ast;

struct NameToken : Token
{
    operator Symbol* () const
    {
        return value.symbol;
    }
};

struct Parser
{
    void init(Lexer* lexer)
    {
        _lexer = lexer;
        _nextToken = _lexer->readToken();
    }

    Decl* lookUp(Symbol* name)
    {

    }

    SourceLoc getLoc()
    {
        return SourceLoc();
    }

    Token::Code peekTokenCode()
    {
        return _nextToken.code;
    }

    Token peekToken()
    {
        return _nextToken;
    }

    Token readToken()
    {
        auto token = _nextToken;
        _nextToken = _lexer->readToken();
        return token;
    }

    // TODO: should return a `Token` that is convertible to `bool`
    bool readIf(Token::Code code)
    {
        if( peekTokenCode() != code )
            return false;

        readToken();
        return true;
    }

    void unexpected(char const* expected)
    {
        if(_isRecovering)
            return;

        error(SourceLoc(), "unexpected '%s', expected '%s'", getTokenName(peekTokenCode()), expected);
        _isRecovering = true;
    }

    void unexpected(Token::Code expected)
    {
        unexpected(getTokenName(expected));
    }

    Token expect(Token::Code code)
    {
        if( peekTokenCode() == code )
        {
            _isRecovering = false;
            return readToken();
        }
        else
        {
            unexpected(code);
            return peekToken();
        }
    }

    NameToken readIdentifier()
    {
        NameToken result;
        *static_cast<Token*>(&result) = expect(Token::Code::Identifier);
        return result;
    }

    Expr* parseNameRef(NameToken const& name)
    {
        auto nameRef = new NameExpr(name, name);
        return nameRef;
    }

    Expr* parseLeafExpr()
    {
        switch (peekTokenCode())
        {
        case Token::Code::Identifier:
            return parseNameRef(readIdentifier());

        default:
            unexpected("expression");
            return nullptr;
        }
    }

    Expr* parsePrefixExpr()
    {
        // TODO: handle this stuff
        return parseLeafExpr();
    }

    Expr* parsePostfixExprSuffix(Expr* expr)
    {
        for (;;)
        {
            switch (peekTokenCode())
            {
            case Token::Code::Dot:
                {
                    auto dotToken = readToken();
                    auto name = readIdentifier();

                    expr = new MemberExpr(dotToken, expr, name);
                }
                break;

            default:
                return expr;
            }
        }
    }

    Expr* parsePostfixExpr()
    {
        return parsePostfixExprSuffix(parsePrefixExpr());
    }

    Expr* parseInfixExprSuffix(Expr* expr)
    {
        // TODO: handle this stuff
        return expr;
    }

    Expr* parseInfixExpr()
    {
        return parseInfixExprSuffix(parsePostfixExpr());
    }

    Expr* parseExprSuffix(Expr* expr)
    {
        expr = parsePostfixExprSuffix(expr);
        expr = parseInfixExprSuffix(expr);
        return expr;
    }

    Expr* parseExpr()
    {
        return parseExprSuffix(parsePrefixExpr());
    }

    Expr* parseExpr(NameToken const& name)
    {
        return parseExprSuffix(parseNameRef(name));
    }

    void parseDeclPattern(Decl* decl)
    {
        // Bases, if any
        while( peekTokenCode() == Token::Code::Identifier )
        {
            auto base = parseExpr();
            decl->_bases.push_back(base);
        }

        if(peekTokenCode() == Token::Code::LCurly)
        {
            // Pattern "mainpart" body
            auto openToken = readToken();

            MainPart* mainPart = new MainPart(openToken);
            parseMainPartBody(mainPart);

            expect(Token::Code::RCurly);

            decl->_mainPart = mainPart;
        }
        else
        {
            expect(Token::Code::Semicolon);
        }
    }

    Stmt* parseStmt(NameToken const& name)
    {
        // TODO: need to detect all the keywords for stmts here...

        // Fallback case is to parse an expression
        {
            Expr* expr = parseExpr(name);
            expect(Token::Code::Semicolon);

            return expr;
        }
    }

    Decl* parseDecl(NameToken const& name)
    {
        Syntax::Tag kind = Syntax::Tag::PatternDecl;
        if (readIf(Token::Code::At))
        {
            kind = Syntax::Tag::InlineValueDecl;
        }

        auto decl = new Decl(kind, name, name);

        parseDeclPattern(decl);

        return decl;
    }

    Stmt* parseDeclOrStmt()
    {
        switch( peekTokenCode() )
        {
        case Token::Code::Identifier:
            {
                // The common case is that we see a leading identifier,
                // which either:
                //
                // * Introduces a declaration in the form `name: ...`
                // * Begins a statement with a keyword, like `if ...`
                // * Begins an expression, like `a + b`
                //
                // We will read the identifeir and look at the next token
                // to know if we have a declaration.
                //
                

                // Simple declaration case:

                auto nameToken = readIdentifier();

                if (peekTokenCode() == Token::Code::Colon)
                {
                    readToken();
                    return parseDecl(nameToken);
                }

                return parseStmt(nameToken);

            }
            break;

        default:
            unexpected("a declaration");
            return nullptr;
        }
    }

    void addStmt(MainPart* parent, Stmt* newStmt)
    {
        Stmt* oldStmt = parent->_stmt;
        if (!oldStmt)
        {
            parent->_stmt = newStmt;
        }
        else if (auto oldSeqStmt = as<SeqStmt>(oldStmt))
        {
            oldSeqStmt->stmts.push_back(newStmt);
        }
        else
        {
            auto seqStmt = new SeqStmt(oldStmt->getRangeInfo());
            seqStmt->stmts.push_back(oldStmt);
            seqStmt->stmts.push_back(newStmt);
            parent->_stmt = seqStmt;
        }
    }

    void addDecl(MainPart* parent, Decl* decl)
    {
        if (parent->_stmt != nullptr)
        {
            error(decl->getLoc(), "cannot put declarations after statements");
        }

        parent->_decls.push_back(decl);
    }

    void parseDeclOrStmt(MainPart* parent)
    {
        Stmt* term = parseDeclOrStmt();
        if(!term)
            return;

        if (auto decl = as<Decl>(term))
        {
            addDecl(parent, decl);
        }
        else
        {
            addStmt(parent, term);
        }
    }

    void parseMainPartBody(MainPart* parent)
    {
        for(;;)
        {
            switch( peekTokenCode() )
            {
            default:
                parseDeclOrStmt(parent);
                break;

            case Token::Code::EndOfFile:
            case Token::Code::RCurly:
                return;
            }
        }
    }

    Decl* parseProgram()
    {
        MainPart* body = new MainPart(peekToken());
        parseMainPartBody(body);

        Decl* decl = new Decl(Decl::Tag::PatternDecl, body->getRangeInfo(), nullptr);
        decl->_mainPart = body;

        return decl;
    }

    Lexer* _lexer;
    Token _nextToken;

    bool _isRecovering = false;
};

}
