// emit.h
#pragma once

#include "syntax.h"

namespace theta
{

    using namespace ast;

namespace bytecode
{

struct Emitter
{
    void emitConstantIndex(Value value)
    {
        auto constantIndex = addConstant(value);
        emitUInt(constantIndex);
    }

    void emitOpcode(Opcode opcode)
    {
        emitByte(Byte(opcode));
    }

    void emitUInt(unsigned int value)
    {
        emitByte(Byte(value));
    }

    void emitByte(Byte code)
    {
        getChunk()->_bytes.push_back(code);
    }

    int addConstant(Value value)
    {
        int index = (int) getChunk()->_constants.size();
        getChunk()->_constants.push_back(value);
        return index;
    }

    struct Scope
    {
        Decl* _astDecl = nullptr;
        BCDecl* _bcDecl = nullptr;

        Scope* _parent = nullptr;
    };
    Scope* _scope = nullptr;

    struct WithScope : Scope
    {
        WithScope(
            Emitter*    self,
            Decl*       astDecl,
            BCDecl*     bcDecl)
            : _self(self)
        {
            _parent = self->_scope;
            self->_scope = this;

            _astDecl = astDecl;
            _bcDecl = bcDecl;
        }

        ~WithScope()
        {
            _self->_scope = _parent;
        }

        Emitter* _self;
    };

    struct ChunkBinding
    {
        CodeChunk* _chunk = nullptr;
        ChunkBinding* _parent = nullptr;
    };
    ChunkBinding* _chunkStack;

    struct WithChunk : ChunkBinding
    {
        WithChunk(
            Emitter* self,
            CodeChunk* chunk)
            : _self(self)
        {
            _parent = self->_chunkStack;
            self->_chunkStack = this;

            _chunk = chunk;
        }

        ~WithChunk()
        {
            _self->_chunkStack = _parent;
        }

        Emitter* _self;
    };


    BCDecl* getBCDecl()
    {
        return _scope ? _scope->_bcDecl : nullptr;
    }

    CodeChunk* getChunk()
    {
        assert(_chunkStack);
        return _chunkStack->_chunk;
    }

#if 0
    BCMainPart* beginMainPart(BCName* name)
    {
        BCMainPart* mainPart = new BCMainPart();
        mainPart->name = name;

        MainPartStackEntry* entry = new MainPartStackEntry();
        entry->_mainPart = mainPart;
        entry->_parent = _mainPartStack;
        _mainPartStack = entry;

        return mainPart;
    }

    void endMainPart(BCMainPart* mainPart)
    {
        assert( _mainPartStack );
        assert( mainPart == _mainPartStack->_mainPart );
        _mainPartStack = _mainPartStack->_parent;
    }
#endif

#if 0
    BCMember* beginMember(BCName* name)
    {
        BCMember* member = new BCMember();
        member->name = name;

        auto mainPart = getMainPart();
        mainPart->_members.push_back(member);

        // TODO: need to update `slotCount` here!!!

        return member;
    }

    void endMember(BCMember* member)
    {}
#endif

    void emitExpr(Expr* expr)
    {
        switch(expr->getTag())
        {
        default:
            error(expr->getLoc(), "unhandled expr in emit");
            break;

        case Expr::Tag::SlotPath:
            {
                auto path = (SlotExpr*) expr;
                emitExpr(path->_base);
                emitOpcode(Opcode::GetPartSlot);
                emitUInt(path->_decl->_slotIndex);
            }
            break;

        case Expr::Tag::SelfPath:
            {
                auto path = (SelfExpr*)expr;
                emitOpcode(Opcode::GetSelfPart);


                // Now based on number of levels "up", we need
                // to emit origin ops...
                auto s = _scope;
                while (s)
                {
                    if (s->_astDecl == path->_decl)
                    {
                        break;
                    }

                    emitOpcode(Opcode::GetMixinFromPart);
                    emitOpcode(Opcode::GetOriginPartFromMixin);

                    s = s->_parent;
                }
            }
            break;

        case Expr::Tag::OriginPath:
            {
                auto path = (OriginExpr*)expr;
                emitExpr(path->_base);
                emitOpcode(Opcode::GetMixinFromPart);
                emitOpcode(Opcode::GetOriginPartFromMixin);
            }
            break;


        }
    }

    void emitCreateObject()
    {
        emitOpcode(Opcode::CreateObject);
    }

    void emitSetPartSlot(size_t slotIndex)
    {
        emitOpcode(Opcode::SetPartSlot);
        emitUInt(slotIndex);
    }

    void emitReturn()
    {
        emitOpcode(Opcode::Return);
    }

    void emitStmt(Stmt* stmt)
    {
        switch (stmt->getTag())
        {
        default:
            if (auto expr = as<Expr>(stmt))
            {
                emitExpr(expr);
                emitOpcode(Opcode::Pop);
            }
            else
            {
                error(stmt->getLoc(), "unhandled stmt in emit");
            }
            break;

        case Stmt::Tag::SeqStmt:
            {
                auto s = (SeqStmt*)stmt;
                for (auto subStmt : s->stmts)
                {
                    emitStmt(subStmt);
                }
            }
            break;
        }
    }

    // Emit code to construct the pattern for `decl` on the stack...
    void emitPattern(ast::Decl* decl)
    {
        size_t baseCount = decl->_bases.size();
        auto mainPart = decl->_mainPart;
\
        // There are many special cases we want to handle...

        if( mainPart )
        {
            if( baseCount == 0 )
            {
                // We just have the main part, and need to turn
                // it into a pattern...
                emitOpcode(Opcode::CreatePatternFromMainPart);
            }
            else if( baseCount == 1 )
            {
                // We have one base and one main part
                auto base = decl->_bases[0];
                emitExpr(base);
                emitOpcode(Opcode::CreatePatternFromBaseAndMainPart);
            }
            else
            {
                error(decl->getLoc(), "unhandled merge case");
            }
        }
        else
        {
            if( baseCount == 0 )
            {
                // We just have the main part, and need to turn
                // it into a pattern...
                emitOpcode(Opcode::GetEmptyPattern);
            }
            else if( baseCount == 1 )
            {
                // We have one base and one main part
                auto base = decl->_bases[0];
                emitExpr(base);
            }
            else
            {
                error(decl->getLoc(), "unhandled merge case");
            }
        }
    }

    BCDecl* emitDecl(ast::Decl* astDecl)
    {
        BCDecl* bcDecl = new BCDecl();
        bcDecl->name = astDecl->_name;
        bcDecl->parent = getBCDecl();

        if (auto astMainPart = astDecl->_mainPart)
        {
            WithScope withScope(this, astDecl, bcDecl);

            bcDecl->_slotCount = astMainPart->_slotCount;

            for (auto astMember : astMainPart->_decls)
            {
                auto bcMember = emitDecl(astMember);
                bcDecl->_members.push_back(bcMember);
            }

            WithChunk withChunk(this, &bcDecl->bodyCode);
            if (auto stmt = astMainPart->_stmt)
            {
                emitStmt(stmt);
            }
            else
            {
                emitOpcode(Opcode::Inner);
            }
            emitOpcode(Opcode::Return);
        }

        // What we emit here depends a *lot* on what kind of declaration
        // we have.

        WithChunk withChunk(this, &bcDecl->initCode);

        switch(astDecl->getTag() )
        {
        default:
            error(astDecl->getLoc(), "unhandled decl in emit");
            break;

        case Decl::Tag::InlineValueDecl:
            // Need to emit logic that computes the pattern,
            // then create a value of that type, then install
            // it into the correct slot...
            //
            emitOpcode(Opcode::GetSelfPart);
            emitPattern(astDecl);
            emitCreateObject();
            emitSetPartSlot(astDecl->_slotIndex);
            break;

        case Decl::Tag::PatternDecl:
            // Need to emit the logic that computes the pattern,
            // and then installs it into the correct slot...
            emitOpcode(Opcode::GetSelfPart);
            emitPattern(astDecl);
            emitSetPartSlot(astDecl->_slotIndex);
            break;

        }

        emitReturn();

        return bcDecl;
    }

    BCDecl* emitProgram(ast::Decl* program)
    {
        return emitDecl(program);
    }
};

}

}
