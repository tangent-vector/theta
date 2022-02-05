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

    struct Scope
    {
        PatternDeclBase* _decl = nullptr;
        Scope* _parent = nullptr;
    };
    Scope* _scope = nullptr;

    struct WithScope : Scope
    {
        WithScope(Parser* self, PatternDeclBase* decl)
            : _self(self)
        {
            _decl = decl;

            _parent = _self->_scope;
            _self->_scope = this;
        }

        ~WithScope()
        {
            _self->_scope = _parent;
        }

        Parser* _self;
    };

    void parseParam(PatternDeclBase* decl)
    {
        auto nameToken = readIdentifier();
        expect(Token::Code::Colon);

        auto typeExpr = parseExpr();

        auto paramDecl = new ParamDecl(nameToken, nameToken, typeExpr);

        decl->_members.push_back(paramDecl);
    }

    void parseParams(PatternDeclBase* decl)
    {
        for (;;)
        {
            switch (peekTokenCode())
            {
            case Token::Code::RParen:
            case Token::Code::EndOfFile:
                return;

            default:
                break;
            }

            parseParam(decl);

            switch (peekTokenCode())
            {
            case Token::Code::RParen:
            case Token::Code::EndOfFile:
                return;

            default:
                break;
            }

            expect(Token::Code::Comma);
        }
    }

    void parsePatternDeclBase(PatternDeclBase* decl)
    {
        // Bases, if any
        while( peekTokenCode() == Token::Code::Identifier )
        {
            auto base = parseExpr();
            decl->_bases.push_back(base);

            if (readIf(Token::Code::Comma))
                continue;
            break;
        }

        if (peekTokenCode() == Token::Code::LParen)
        {
            // Parameters...

            auto openToken = readToken();

            parseParams(decl);

            expect(Token::Code::RParen);
        }

        if(peekTokenCode() == Token::Code::LCurly)
        {
            WithScope withScope(this, decl);

            // Pattern "mainpart" body
            auto openToken = readToken();

            parseMainPartBody(decl);

            expect(Token::Code::RCurly);
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

    PatternDecl* parsePatternDecl()
    {
        auto decl = new PatternDecl();
        parsePatternDeclBase(decl);
        return decl;

    }

    ObjectDecl* parseObjectDecl()
    {
        auto decl = new ObjectDecl();
        parsePatternDeclBase(decl);
        return decl;
    }

    Decl* parseDecl(NameToken const& name)
    {
        Syntax::Tag kind = Syntax::Tag::PatternDecl;
        if (readIf(Token::Code::At))
        {
            kind = Syntax::Tag::InlineValueDecl;
        }

        auto decl = new SimpleDecl(kind, name, name);

        parseDeclPattern(decl);

        return decl;
    }

    SyntaxDecl* maybeLookUpSyntax(Symbol* name)
    {
        // TODO: This should be better hooked into the actual semantic checking step,
        // so that syntax can actually be brought in through the statically visible
        // bases, and follow the same scoping rules as everything else.
        //
        for (auto s = _scope; s; s = s->_parent)
        {
            auto decl = s->_decl;

            for (auto member : decl->_members)
            {
                if (member->_name != name)
                    continue;

                return as<SyntaxDecl>(member);
            }
        }

        return nullptr;
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

                auto syntax = maybeLookUpSyntax(nameToken);
                if (syntax)
                {
                    auto result = syntax->_callback(this);
                    auto stmt = as<Stmt>(result);
                    return stmt;
                }



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

    void addStmt(PatternDeclBase* parent, Stmt* newStmt)
    {
        Stmt* oldStmt = parent->_bodyStmt;
        if (!oldStmt)
        {
            parent->_bodyStmt = newStmt;
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
            parent->_bodyStmt = seqStmt;
        }
    }

    void addDecl(PatternDeclBase* parent, Decl* decl)
    {
        if (parent->_bodyStmt != nullptr)
        {
            error(decl->getLoc(), "cannot put declarations after statements");
        }

        parent->_members.push_back(decl);
    }

    void parseDeclOrStmt(PatternDeclBase* parent)
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

    void parseMainPartBody(PatternDeclBase* parent)
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

    Modifier* parseBuiltinModifier()
    {
        return new Modifier(Modifier::Tag::BuiltinModifier, SourceRangeInfo());
    }

    template<typename T, T* (Parser::*callback)()>
    SyntaxDecl::Callback getSyntaxCallback()
    {
        struct Helper
        {
            static Node* _callback(Parser* parser)
            {
                return (parser->*callback)();
            }
        };
        return &Helper::_callback;
    }

    PatternDeclBase* _superGlobalDecl = nullptr;

    void _initSuperGlobalDecl()
    {
        _superGlobalDecl = new PatternDecl();

        addBuiltinSyntax<Modifier, &Parser::parseBuiltinModifier>("__builtin");
    }


    Decl* parseProgram()
    {

        // set up the super-global environment with things like built-in syntax...

        SyntaxDecl* builtinModifierDecl = new SyntaxDecl(getSymbol(StringSpan("__builtin")), getSyntaxCallback<Modifier, &Parser::parseBuiltinModifier>());
        superGlobalBody->_decls.push_back(builtinModifierDecl);

        WithScope withScope(this, superGlobalDecl);

        MainPart* body = new MainPart(peekToken());
        parseMainPartBody(body);

        SimpleDecl* decl = new SimpleDecl(Decl::Tag::PatternDecl, body->getRangeInfo(), getSymbol(StringSpan("theta")));
        decl->_mainPart = body;

        return decl;
    }

    Lexer* _lexer;
    Token _nextToken;

    bool _isRecovering = false;
};

}
