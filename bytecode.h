// bytecode.h
#pragma once

#include "diagnostics.h"
#include "source-manager.h"
#include "value.h"

namespace theta
{

namespace bytecode
{
typedef uint8_t Byte;

enum class Opcode : Byte
{
    Nop,
    Return,
    Constant,
    CreateObject,

    Pop,

    GetPartSlot,
    SetPartSlot,

    CreatePatternFromMainPart,
    CreatePatternFromBaseAndMainPart,
    GetEmptyPattern,

    GetSelfPart,
    GetObjectFromPart,
    GetPartFromObject,
    GetMixinFromPart,
    GetOriginPartFromMixin,

    Inner,
};

struct BCDecl;

struct CodeChunk
{
    std::vector<Byte> _bytes;
    std::vector<Value> _constants;

//    BCDecl* _decl;

    void dump()
    {
        Byte const* cursor = _bytes.data();
        Byte const* end = cursor + _bytes.size();

        while(cursor != end)
        {
            Opcode opcode = Opcode(*cursor++);
            switch (opcode)
            {
            default:
                error(SourceLoc(), "invalid opcode");
                return;

            case Opcode::Inner:
                printf("INNER");
                break;

            case Opcode::Pop:
                printf("POP");
                break;


            case Opcode::Nop:
                printf("NOP");
                break;

            case Opcode::Constant:
                printf("CONSANT %d", int(*cursor++));
                break;


            case Opcode::Return:
                printf("RETURN");
                return;

            case Opcode::CreateObject:
                printf("CREATE_OBJECT");
                break;

            case Opcode::SetPartSlot:
                printf("SET_PART_SLOT %d", int(*cursor++));
                break;

            case Opcode::GetPartSlot:
                printf("GET_PART_SLOT %d", int(*cursor++));
                break;

            case Opcode::CreatePatternFromMainPart:
                printf("CREATE_PATTERN_FROM_MAIN_PART");
                break;

            case Opcode::CreatePatternFromBaseAndMainPart:
                printf("CREATE_PATTERN_FROM_BASE_AND_MAIN_PART");
                break;

            case Opcode::GetEmptyPattern:
                printf("GET_EMPTY_PATTERN");
                break;

            case Opcode::GetSelfPart:
                printf("GET_SELF_PART");
                break;

            case Opcode::GetObjectFromPart:
                printf("GET_OBJECT_FROM_PART");
                break;

            case Opcode::GetPartFromObject:
                printf("GET_PART_FROM_OBJECT");
                break;

            case Opcode::GetMixinFromPart:
                printf("GET_MIXIN_FROM_PART");
                break;

            case Opcode::GetOriginPartFromMixin:
                printf("GET_ORIGIN_PART_FROM_MIXIN");
                break;
            }
            printf("\n");
        }
    }
};

struct BCDecl
{
    Symbol* name = nullptr;
    BCDecl* parent = nullptr;

    // Nested/child members of this declaration
    std::vector<BCDecl*> _members;

    // The number of "direct" slots that need to be allocated
    // in a part created from this decl
    size_t _slotCount = 0;

    // Code to initialize this member as part of
    // initializing a part based on the enclosing
    // main part.
    //
    CodeChunk initCode;

    // The "do" part of this decl
    CodeChunk bodyCode;

    void dumpName()
    {
        if (parent)
        {
            parent->dumpName();
            printf("::");
        }
        if (name)
        {
            printf("%s", name->text.getData());
        }
        else
        {
            printf("_");
        }
    }

    void dump()
    {
        printf("BCDecl(name: ");
        dumpName();
        printf(")\n");
        printf("INIT: {\n");
        initCode.dump();
        printf("}\n");
        printf("DO: {\n");
        bodyCode.dump();
        printf("}\n");

        for (auto member : _members)
        {
            member->dump();
        }
    }
};


}

}
