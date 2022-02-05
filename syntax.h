// syntax.h
#pragma once

#include "token.h"

namespace theta
{

struct Parser;

namespace ast
{

class Decl;
class PatternDeclBase;
class Syntax;
class Expr;

struct SourceRangeInfo
{
    SourceRangeInfo()
    {}

    SourceRangeInfo(Token const& token)
    {
        range = token.range;
        loc = range.begin;
    }

    SourceRange range;
    SourceLoc loc;
};

class Node
{
public:
    enum class Tag
    {
        PatternDecl,
        VirtualPatternDecl,
        FurtherPatternDecl,

        ObjectDecl,

        VarDecl,
        LetDecl,
        ParamDecl,

        SyntaxDecl,

        NameExpr,
        MemberExpr,

        SeqStmt,

        SelfPath,
        SlotPath,
        OriginPath,

        CastToBaseExpr,

        MainPart,

        EmptyStaticPattern,
        StaticMixin,

        EmptyMixinPath,
        BaseMixinPath,

        BuiltinModifier,
    };

    Node(Tag tag)
        : _tag(tag)
    {}

    virtual ~Node() {}

    Tag getTag() { return _tag; }

private:
    Tag _tag;
};

class Syntax : public Node
{
public:
    typedef Node Super;

    Syntax(Tag tag)
        : Super(tag)
    {}

    Syntax(Tag tag, SourceRangeInfo info)
        : Super(tag)
        , _sourceRangeInfo(info)
    {}


    SourceRange getRange() { return _sourceRangeInfo.range; }
    SourceLoc getLoc() { return _sourceRangeInfo.loc; }
    SourceRangeInfo getRangeInfo() { return _sourceRangeInfo; }

private:
    SourceRangeInfo _sourceRangeInfo;
};

class StaticPattern;
class StaticMixin;
class EmptyStaticPattern;

class StaticPattern : public Node
{
public:
    typedef Node Super;

    StaticPattern(Tag tag)
        : Super(tag)
    {}

    // Flattened list of all mixins, in precedence order
    std::vector<StaticMixin*> _mixins;
};

// A path from a view part to another of its statically-known mixins
class MixinPath : public Node
{
public:
    typedef Node Super;

    MixinPath(Tag tag)
        : Super(tag)
    {}
};

class EmptyMixinPath : public MixinPath
{
public:
    typedef MixinPath Super;

    EmptyMixinPath()
        : Super(Tag::EmptyMixinPath)
    {}
};

class BaseMixinPath : public MixinPath
{
public:
    typedef MixinPath Super;

    BaseMixinPath(int baseIndex, MixinPath* rest)
        : Super(Tag::BaseMixinPath)
        , _baseIndex(baseIndex)
        , _rest(rest)
    {}

    int _baseIndex;
    MixinPath* _rest;
};


class StaticMixin : public StaticPattern
{
public:
    typedef StaticPattern Super;

    StaticMixin(PatternDeclBase* decl, Expr* origin, MixinPath* relativePath)
        : Super(Tag::StaticMixin)
        , _decl(decl)
        , _origin(origin)
        , _relativePath(relativePath)
    {}

    PatternDeclBase* getDecl();

    // Declared bases
    std::vector<StaticPattern*> _bases;

    PatternDeclBase* _decl;
    Expr* _origin;

    MixinPath* _relativePath;
};

class EmptyStaticPattern : public StaticPattern
{
public:
    typedef StaticPattern Super;

    EmptyStaticPattern()
        : Super(Tag::EmptyStaticPattern)
    {}
};


struct Classifier
{
    enum class Kind
    {
        Unknown,
        Value,
        Type,
    };

    Kind kind = Kind::Unknown;
    StaticPattern* pattern = nullptr;
};

class Modifier : public Syntax
{
public:
    typedef Syntax Super;

    Modifier(Tag tag, SourceRangeInfo const& info)
        : Super(tag, info)
    {}

    Modifier* _next = nullptr;
};

class Stmt : public Syntax
{
public:
    typedef Syntax Super;

    Stmt(Tag tag)
        : Super(tag)
    {}

    Stmt(Tag tag, SourceRangeInfo const& info)
        : Super(tag, info)
    {}
};

class SeqStmt : public Stmt
{
public:
    typedef Stmt Super;

    SeqStmt(SourceRangeInfo const& info)
        : Super(Tag::SeqStmt, info)
    {}

    std::vector<Stmt*> stmts;
};

class Decl : public Stmt
{
public:
    typedef Stmt Super;

    Decl(Tag tag)
        : Super(tag)
    {}

    Decl(Tag tag, SourceRangeInfo const& info, Symbol* name)
        : Super(tag, info)
        , _name(name)
    {}

    Symbol* _name = nullptr;
    size_t _slotIndex = size_t(-1);
};

class SyntaxDecl : public Decl
{
public:
    typedef Decl Super;

    typedef Node* (*Callback)(Parser* parser, void* userData);

    SyntaxDecl(Symbol* name, Callback callback, void* userData)
        : Super(Tag::SyntaxDecl, SourceRangeInfo(), name)
        , _callback(callback)
        , _userData(userData)
    {}

    Callback _callback;
    void* _userData;
};

class ValueDeclBase : public Decl
{
public:
    typedef Decl Super;

    ValueDeclBase(Tag tag, SourceRangeInfo const& info, Symbol* name, Expr* typeExpr)
        : Super(tag, info, name)
        , _typeExpr(typeExpr)
    {}

    Expr* _typeExpr;
};

class VarDeclBase : public ValueDeclBase
{
public:
    typedef ValueDeclBase Super;

    VarDeclBase(Tag tag, SourceRangeInfo const& info, Symbol* name, Expr* typeExpr)
        : Super(tag, info, name, typeExpr)
    {}
};

class VarDecl : public VarDeclBase
{
public:
    typedef VarDeclBase Super;

    VarDecl(SourceRangeInfo const& info, Symbol* name, Expr* typeExpr)
        : Super(Tag::VarDecl, info, name, typeExpr)
    {}
};

class LetDeclBase : public VarDeclBase
{
public:
    typedef VarDeclBase Super;

    LetDeclBase(Tag tag, SourceRangeInfo const& info, Symbol* name, Expr* typeExpr)
        : Super(tag, info, name, typeExpr)
    {}
};

class LetDecl : public LetDeclBase
{
public:
    typedef LetDeclBase Super;

    LetDecl(SourceRangeInfo const& info, Symbol* name, Expr* typeExpr)
        : Super(Tag::LetDecl, info, name, typeExpr)
    {}
};

class ParamDecl : public LetDeclBase
{
public:
    typedef LetDeclBase Super;

    ParamDecl(SourceRangeInfo const& info, Symbol* name, Expr* typeExpr)
        : Super(Tag::ParamDecl, info, name, typeExpr)
    {}
};

class PatternDeclBase : public Decl
{
public:
    typedef Decl Super;

    PatternDeclBase(Tag tag)
        : Super(tag)
    {}

    PatternDeclBase(Tag tag, SourceRangeInfo const& info, Symbol* name)
        : Super(tag, info, name)
    {}

    std::vector<Expr*> _bases;

    std::vector<Decl*> _members;
    size_t _slotCount = 0;

    Stmt* _bodyStmt = nullptr;
};

class PatternDecl : public PatternDeclBase
{
public:
    typedef PatternDeclBase Super;

    PatternDecl()
        : Super(Tag::PatternDecl)
    {}

    PatternDecl(SourceRangeInfo const& info, Symbol* name)
        : Super(Tag::PatternDecl, info, name)
    {}
};

class ObjectDecl : public PatternDeclBase
{
public:
    typedef PatternDeclBase Super;

    ObjectDecl()
        : Super(Tag::ObjectDecl)
    {}

    ObjectDecl(SourceRangeInfo const& info, Symbol* name)
        : Super(Tag::ObjectDecl, info, name)
    {}
};

class Expr : public Stmt
{
public:
    typedef Stmt Super;

    Expr(Tag tag, SourceRangeInfo const& info)
        : Super(tag, info)
    {}

    void setClassifier(Classifier classifier)
    {
        assert(_classifier.kind == Classifier::Kind::Unknown);
        _classifier = classifier;
    }

    Classifier _classifier;
};

class NameExpr : public Expr
{
public:
    typedef Expr Super;

    NameExpr(SourceRangeInfo const& info, Symbol* name)
        : Super(Tag::NameExpr, info)
        , _name(name)
    {}

    Symbol* _name;
};

class MemberExpr : public Expr
{
public:
    typedef Expr Super;

    MemberExpr(SourceRangeInfo const& info, Expr* base, Symbol* name)
        : Super(Tag::MemberExpr, info)
        , _base(base)
        , _name(name)
    {}

    Expr* _base;
    Symbol* _name;
};

    // An expression that is typed at the point it is created
class TypedExpr : public Expr
{
public:
    typedef Expr Super;

    TypedExpr(Tag tag, SourceRangeInfo const& info, Classifier classifier)
        : Super(tag, info)
    {
        setClassifier(classifier);
    }
};

    // The trivial path for the "current" part
class SelfExpr : public TypedExpr
{
public:
    typedef TypedExpr Super;

    SelfExpr(SourceRangeInfo const& info, PatternDeclBase* decl, SelfExpr* parent, Classifier classifier)
        : Super(Tag::SelfPath, info, classifier)
        , _decl(decl)
        , _parent(parent)
    {}

    PatternDeclBase* _decl;
    SelfExpr* _parent;
};

    // A path that acccesses a single "slot" of a part
class SlotExpr : public TypedExpr
{
public:
    typedef TypedExpr Super;

    SlotExpr(SourceRangeInfo const& info, Expr* base, Decl* decl, Classifier classifier)
        : Super(Tag::SlotPath, info, classifier)
        , _base(base)
        , _decl(decl)
    {}

    Expr* _base;
    Decl* _decl;
};

    // A cast from a part to the part for one of its statically-identified bases
class CastToBaseExpr : public TypedExpr
{
public:
    typedef TypedExpr Super;

    CastToBaseExpr(SourceRangeInfo const& info, Expr* base, int baseIndex, Classifier classifier)
        : Super(Tag::CastToBaseExpr, info, classifier)
        , _base(base)
        , _baseIndex(baseIndex)
    {}

    Expr* _base;
    int _baseIndex;
};


    // A path that moves from a part to its origin part (through its mixin)
class OriginExpr : public TypedExpr
{
public:
    typedef TypedExpr Super;

    OriginExpr(SourceRangeInfo const& info, Expr* base, Classifier classifier)
        : Super(Tag::OriginPath, info, classifier)
        , _base(base)
    {}

    Expr* _base;
};

// Other paths...



template<typename T>
T* as(Node* node)
{
    return dynamic_cast<T*>(node);
}

}

}
