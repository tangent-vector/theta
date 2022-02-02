// semantics.h
#pragma once

namespace theta
{
namespace semantics
{

using namespace ast;

class Checker
{
public:
    SelfExpr* _self;

    Classifier::Kind getClassifierKind(Decl* decl)
    {
        switch (decl->getTag())
        {
        case Decl::Tag::InlineValueDecl:
        case Decl::Tag::ReferenceValueDecl:
            return Classifier::Kind::Value;

        case Decl::Tag::PatternDecl:
        case Decl::Tag::VirtualPatternDecl:
        case Decl::Tag::FurtherPatternDecl:
            return Classifier::Kind::Type;

        default:
            error(decl->getLoc(), "unhandled decl kind");
            return Classifier::Kind::Type;
        }
    }

    void pushScope(Decl* decl)
    {
        assert(decl->_mainPart);

        SelfExpr* origin = _self;

        Classifier selfClassifier = getClassifier(decl, origin);
        selfClassifier.kind = Classifier::Kind::Value;

        SelfExpr* newSelf = new SelfExpr(decl->getRangeInfo(), decl, origin, selfClassifier);

        _self = newSelf;
    }

    void popScope()
    {
        _self = _self->_parent;
    }

    // Get the classifier for `decl` with respect to `part` (it should have been looked up on `part`...
    Classifier getClassifier(Decl* decl, Expr* part)
    {
        Classifier classifier;
        classifier.kind = getClassifierKind(decl);
        classifier.pattern = createStaticPattern(part, decl);
        return classifier;
    }

    Expr* lookUpInSinglePart(SourceRangeInfo const& info, Symbol* name, Expr* part)
    {
        auto classifier = part->_classifier;
        assert(classifier.kind == Classifier::Kind::Value);

        auto mixin = as<StaticMixin>(classifier.pattern);
        if (!mixin)
            return nullptr;

        auto mainPart = mixin->getMainPart();
        for( auto decl : mainPart->_decls )
        {
            // TODO: need to handle the case of overrides for inherited virtual members

            if(decl->_name != name)
                continue;

            // TODO: the classifier will be relative to the given `part`
            // anyway, so we may not need to substitute here...

            // We need to refer to the given `decl`,
            // *but* we need to do it given all the information
            // currently available about what the actual type
            // of the `decl` would be in the context of the
            // object that contains `part`.
            //
            auto classifier = getClassifier(decl, part);
            auto slotPath = new SlotExpr(info, part, decl, classifier);
            return slotPath;
        }

        return nullptr;
    }

    Expr* lookUpInObject(SourceRangeInfo const& info, Symbol* name, Expr* viewPart)
    {
        // We have a single object, but we are "viewing" it through the given part
        // (typically because we are compiling code "inside" that part)
        //
        // Start by looking in just the one part, since it can/should shadow
        // the declarations coming from any other mixins for the same object.
        //
        if(auto directResult = lookUpInSinglePart(info, name, viewPart))
            return directResult;


        // Otherwise, we want to look in *all* the mixins, and signal ambiguity if
        // we find more than one declaration by the same name.
        //
        Expr* existingResult = nullptr;

        auto staticPattern = viewPart->_classifier.pattern;
        for (auto mixin : staticPattern->_mixins)
        {
            auto otherPart = staticCastToMixin(viewPart, mixin);

            Expr* otherResult = lookUpInSinglePart(info, name, otherPart);
            if (!otherResult)
                continue;

            if (!existingResult)
            {
                existingResult = otherResult;
                continue;
            }

            error(info.loc, "ambiguous lookup");
            break;
        }

        return existingResult;
    }

    Expr* lookUp(SourceRangeInfo const& info, Symbol* name)
    {
        SelfExpr* part = _self;

        while(part)
        {
            if( auto result = lookUpInObject(info, name, part) )
                return result;

            part = part->_parent;
        }

        error(SourceLoc(), "undefined identifier", name->text.getData());
        return nullptr;
    }

    Expr* checkNameRef(NameExpr* nameRef)
    {
        auto declRef = lookUp(nameRef->getRangeInfo(), nameRef->_name);
        return declRef;
    }

    Expr* checkMemberExpr(MemberExpr* expr)
    {
        auto base = checkExpr(expr->_base);

        return lookUpInObject(expr->getRangeInfo(), expr->_name, base);
    }

    Expr* checkExpr(Expr* expr)
    {
        switch (expr->getTag())
        {
        case Expr::Tag::NameExpr:
            {
                auto nameRef = as<NameExpr>(expr);
                return checkNameRef(nameRef);
            }

        case Expr::Tag::MemberExpr:
            return checkMemberExpr((MemberExpr*)expr);


        default:
            error(expr->getLoc(), "unhandled expression class");
            return expr;
        }
    }

    void checkStmt(Stmt*& stmt)
    {
        if (!stmt)
            return;

        switch (stmt->getTag())
        {
        default:
            if (auto expr = as<Expr>(stmt))
            {
                stmt = checkExpr(expr);
            }
            else
            {
                error(expr->getLoc(), "unhandled stmt in semantics");
            }
            break;

        case Stmt::Tag::SeqStmt:
            {
                auto seqStmt = (SeqStmt*)stmt;
                for (auto& subStmt : seqStmt->stmts)
                {
                    checkStmt(subStmt);
                }
            }
            break;
        }

    }

    StaticPattern* checkPatternExpr(Expr** ioPatternExpr)
    {
        Expr*& patternExpr = *ioPatternExpr;
        patternExpr = checkExpr(patternExpr);

        if( patternExpr->_classifier.kind != Classifier::Kind::Type )
        {
            error(patternExpr->getLoc(), "expected a pattern");
            return nullptr;
        }

        return patternExpr->_classifier.pattern;
    }

    Expr* staticEval(MixinPath* expr, Expr* origin)
    {
        Expr* result = origin;

        for (;;)
        {
            switch (expr->getTag())
            {
            case MixinPath::Tag::EmptyMixinPath:
                return result;

            case MixinPath::Tag::BaseMixinPath:
            {
                auto basePath = (BaseMixinPath*)expr;
                auto baseIndex = basePath->_baseIndex;

                auto staticPattern = result->_classifier.pattern;
                assert(staticPattern->getTag() == StaticPattern::Tag::StaticMixin);
                auto staticMixin = (StaticMixin*)staticPattern;
                auto staticBase = staticMixin->_bases[baseIndex];

                Classifier classifier;
                classifier.kind = Classifier::Kind::Value;
                classifier.pattern = staticBase;

                expr = basePath->_rest;
                result = new CastToBaseExpr(result->getRangeInfo(), result, baseIndex, classifier);
            }
            break;

            default:
                error(SourceLoc(), "unhandled static op");
                return nullptr;
            }
        }
    }

    Expr* staticCastToMixin(Expr* partExpr, StaticMixin* mixin)
    {
        return staticEval(mixin->_relativePath, partExpr);
    }

    Expr* staticGetSlot(SourceRangeInfo const& rangeInfo, Expr* base, Decl* decl)
    {
        return new SlotExpr(rangeInfo, base, decl, getClassifier(decl, base));
    }

    Expr* staticEval(Expr* expr, Expr* origin)
    {
        switch (expr->getTag())
        {
        case Expr::Tag::SelfPath:
            return origin;

        case Expr::Tag::SlotPath:
            {
                auto slotExpr = (SlotExpr*)expr;
                return staticGetSlot(slotExpr->getRangeInfo(), origin, slotExpr->_decl);
            }
            break;

        default:
            error(expr->getLoc(), "unhandled static op");
            return nullptr;
        }
    }

    StaticPattern* evalStaticPattern(Expr* expr, Expr* origin)
    {
        Expr* patternRef = staticEval(expr, origin);
        auto classifier = patternRef->_classifier;
        if (classifier.kind != Classifier::Kind::Type)
        {
            error(expr->getLoc(), "expected a pattern!");
            return nullptr;
        }
        return classifier.pattern;
    }

    EmptyStaticPattern* emptyPattern = nullptr;
    EmptyStaticPattern* getEmptyPattern()
    {
        if (!emptyPattern) emptyPattern = new EmptyStaticPattern();
        return emptyPattern;
    }

    StaticPattern* createStaticPattern(Expr* origin, Decl* decl)
    {
        std::vector<StaticPattern*> bases;
        // TODO: handle further-binding
        for (auto baseExpr : decl->_bases)
        {
            auto basePattern = evalStaticPattern(baseExpr, origin);
            bases.push_back(basePattern);
        }
        size_t baseCount = bases.size();

        if (!decl->_mainPart)
        {
            if (baseCount == 0)
            {
                return getEmptyPattern();
            }
            else if (baseCount == 1)
            {
                return bases[0];
            }
            else
            {
                // Can't handle this case
                throw 99;
            }
        }
        else
        {
            StaticMixin* staticPattern = new StaticMixin(decl, origin, new EmptyMixinPath());

            if (baseCount == 0)
            {
                // Easy case: just the one mixin
            }
            else if (baseCount == 1)
            {
                int baseCounter = 0;

                auto basePattern = bases[0];
                for (auto baseMixin : basePattern->_mixins)
                {
                    int baseIndex = baseCounter++;

                    auto mixin = new StaticMixin(baseMixin->_decl, baseMixin->_origin, new BaseMixinPath(baseIndex, baseMixin->_relativePath));
                    staticPattern->_mixins.push_back(mixin);
                }
            }
            else
            {
                // Can't handle this case
                throw 99;
            }

            staticPattern->_mixins.push_back(staticPattern);

            return staticPattern;
        }
    }

#if 0
    StaticPattern* createStaticPattern(SourceLoc loc, std::vector<StaticPattern*> const& bases)
    {
        size_t baseCount = bases.size();
        if(baseCount == 0)
            return new StaticPattern();

        if(baseCount == 1)
            return bases[0];

        // TODO: another important special case is when there's a single
        //

        error(loc, "unhandled case for pattern merge");
        return new StaticPattern();
    }

    StaticPattern* createStaticPattern(
        SourceLoc                           loc,
        std::vector<StaticPattern*> const&  bases,
        MainPart*                           mainPart)
    {
        if( !mainPart )
        {
            return createStaticPattern(loc, bases);
        }

        size_t baseCount = bases.size();
        if(baseCount == 0)
        {
            // Easy case: it is just our main part
            StaticPattern* mainPartPattern = new StaticPattern();
            StaticMixin* mainPartMixin = new StaticMixin(mainPartPattern, mainPart, _currentScope);
            mainPartPattern->_mixins.push_back(mainPartMixin);
            return mainPartPattern;
        }

        if( baseCount == 1 )
        {
            auto base = bases[0];

            // Also easy: it is the mixins of the base, plus those of the main part...
            StaticPattern* combinedPattern = new StaticPattern();

            for( auto baseMixin : base->_mixins )
            {
                auto combinedMixin = new StaticMixin(combinedPattern, baseMixin->_mainPart, baseMixin->_origin);
                combinedPattern->_mixins.push_back(combinedMixin);
            }

            StaticMixin* mainPartMixin = new StaticMixin(combinedPattern, mainPart, _currentScope);
            combinedPattern->_mixins.push_back(mainPartMixin);

            return combinedPattern;
        }

        error(loc, "unhandled case for pattern merge");
        return new StaticPattern();
    }
#endif

    void checkDecl(Decl* decl)
    {
        // TODO: check for name conflict

        // Need to iterate over the bases, if any,
        // and check that they resolve to a type...
        //
        for( auto& baseExpr : decl->_bases )
        {
            auto basePattern = checkPatternExpr(&baseExpr);
        }

        // TODO: if we are further-binding,
        // we need to look up the pattern we are further-binding,
        // and *that* is implicitly one of our bases...

//        StaticPattern* pattern = createStaticPattern(decl->getLoc(), basePatterns, decl->_mainPart);

#if 0
//        decl->_classifier.pattern = pattern;
        switch( decl->getTag() )
        {
        case Decl::Tag::InlineValueDecl:
        case Decl::Tag::ReferenceValueDecl:
            decl->_classifier.kind = Classifier::Kind::Value;
            break;

        case Decl::Tag::PatternDecl:
        case Decl::Tag::VirtualPatternDecl:
        case Decl::Tag::FurtherPatternDecl:
            decl->_classifier.kind = Classifier::Kind::Type;
            break;

        default:
            error(decl->getLoc(), "unhandled decl kind");
            break;
        }
#endif

        if( auto mainPart = decl->_mainPart )
        {
            pushScope(decl);

            size_t slotCounter = 0;
            for( auto memberDecl : mainPart->_decls )
            {
                switch( memberDecl->getTag() )
                {
                default:
                    memberDecl->_slotIndex = slotCounter++;
                    break;

                case Decl::Tag::FurtherPatternDecl:
                    // TODO: need to set this one differently...
                    break;
                }
            }
            mainPart->_slotCount = slotCounter;

            for( auto memberDecl : mainPart->_decls )
            {
                checkDecl(memberDecl);
            }

            checkStmt(mainPart->_stmt);

            popScope();
        }
    }

    void checkProgram(Decl* program)
    {
        checkDecl(program);
    }

};

}

}
