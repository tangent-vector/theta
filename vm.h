// vm.h
#pragma once

namespace theta
{
namespace vm
{
using namespace bytecode;

struct Object;
struct Part;
struct Pattern;
struct Mixin;

struct Pattern : ValueObj
{
    // The mixins that make up the state of the class
    std::vector<Mixin*> _mixins;
};

struct Mixin : ValueObj
{
        // The bytecode that describes the main part
    Pattern* _parentPattern;
    BCDecl const* _decl;
    Part* _origin;

    Mixin(Pattern* parentPattern, BCDecl const* decl, Part* origin)
        : _parentPattern(parentPattern)
        , _decl(decl)
        , _origin(origin)
    {}
};

struct Object : ValueObj
{
    // The direct run-time class of the object
    Pattern* _pattern;

    // The parts that comprise the state of the object
    std::vector<Part*> _parts;

    Object(Pattern* pattern)
        : _pattern(pattern)
    {}
};

struct Part : ValueObj
{
    // The object this is a part of
    Object* _parentObject;

    // The mixin (class part) that this object is instantiated from
    Mixin* _mixin;

    std::vector<Value> _slots;

    Part(Object* parentObject, Mixin* mixin)
        : _parentObject(parentObject)
        , _mixin(mixin)
    {
        _slots.resize(mixin->_decl->_slotCount);
    }
};

class Writer
{
public:
    FILE* file = nullptr;
    int indent = 0;
    bool atStartOfLine = true;

    std::map<Symbol*, size_t> mapNameToIDCounter;
    std::map<void const*, size_t> mapPtrToID;
    std::set<void const*> seenPtrs;

    int getPtrID(void const* ptr, Symbol* name)
    {
        auto i = mapPtrToID.find(ptr);
        if (i != mapPtrToID.end())
        {
            return (int)i->second;
        }

        size_t id = 0;

        auto j = mapNameToIDCounter.find(name);
        if (j != mapNameToIDCounter.end())
        {
            id = ++j->second;
        }
        else
        {
            id = 0;
            mapNameToIDCounter.insert(std::make_pair(name, id));
        }

        mapPtrToID.insert(std::make_pair(ptr, id));
        return (int) id;
    }

    bool haveAlreadySeen(void const* ptr)
    {
        if (!ptr)
            return true;

        if (seenPtrs.find(ptr) != seenPtrs.end())
            return true;

        seenPtrs.insert(ptr);
        return false;
    }

    void write(char const* text, size_t size)
    {
        char const* cursor = text;
        size_t remaining = size;

        while( remaining-- )
        {
            char c = *cursor++;

            if( c == '\n' )
            {
                fputs("\n", file);
                atStartOfLine = true;
                continue;
            }

            if( atStartOfLine )
            {
                for( int i = 0; i < indent; ++i )
                {
                    fputs("  ", file);
                }
                atStartOfLine = false;
            }

            fputc(c, file);
        }
    }

    void write(char const* text)
    {
        write(text, strlen(text));
    }

    void write(StringSpan const& text)
    {
        write(text.getData(), text.getSize());
    }

    void write(Symbol* symbol)
    {
        write(symbol->text);
    }

    void write(int value)
    {
        char buffer[32];
        sprintf(buffer, "%d", value);
        write(buffer);
    }

    void writeHexPtr(void const* ptr)
    {
        char buffer[32];
        sprintf(buffer, "%p", ptr);
        write(buffer);
    }

    void write(Value value)
    {
        auto obj = value.getPtr();
        if (auto object = dynamic_cast<Object*>(obj))
        {
            write("object ");
            write(object);
        }
        else if (auto pattern = dynamic_cast<Pattern*>(obj))
        {
            write("pattern ");
            write(pattern);
        }
        else
        {
            write("???");
        }
    }

    Symbol* getName(Symbol* name, const char* defaultName)
    {
        if (name) return name;

        return getSymbol(StringSpan(defaultName, defaultName + strlen(defaultName)));
    }

    void writeUniqueName(void const* ptr, Symbol* name, const char* defaultName)
    {
        if (!ptr)
        {
            write("null");
            return;
        }

        auto n = getName(name, defaultName);

        auto id = getPtrID(ptr, n);

        write("%");
        write(n);
        if (!name || id != 0)
        {
            write(id);
        }
    }

    void writeUniqueName(void const* ptr, const char* defaultName)
    {
        writeUniqueName(ptr, nullptr, defaultName);
    }

    void writeName(BCDecl const* decl)
    {
        writeUniqueName(decl, decl ? decl->name : nullptr, "decl");
    }

    void writeRef(BCDecl const* decl)
    {
        writeName(decl);
    }

    void write(BCDecl const* decl)
    {
        writeRef(decl);
    }

    void writeName(Mixin* mixin)
    {
        if (!mixin)
        {
            write("null");
            return;
        }

        if (auto origin = mixin->_origin)
        {
            writeName(origin);
            write(".");
        }
        writeRef(mixin->_decl);
    }

    void writeRef(Mixin* mixin)
    {
        writeName(mixin);
    }

    void write(Mixin* mixin)
    {
        writeRef(mixin);
    }

    void writeName(Part* part)
    {
        writeName(part->_parentObject);
        write("[");
        writeRef(part->_mixin);
        write("]");
    }

    void writeRef(Part* part)
    {
        writeName(part);
    }

    void write(Part* part)
    {
        writeRef(part);
    }

    void writeName(Object* object)
    {
        writeUniqueName(object, "obj");
    }

    void writeRef(Object* object)
    {
        writeName(object);

        if (!object) return;

        write(" : ");
        writeName(object->_pattern);
    }

    void write(Object* object)
    {
        writeName(object);
        if (haveAlreadySeen(object)) return;

        write("\n{");
        increaseIndent();

        bool firstPart = true;
        for( auto part : object->_parts )
        {
            write("\n");

            auto mixin = part->_mixin;

            decreaseIndent();
            write("[");
            writeRef(mixin);
            write("]");
            increaseIndent();

            increaseIndent();

            bool firstSlot = true;
            for (auto slotValue : part->_slots)
            {
                write("\n");

                // TODO: need to extract slot names from the mixin

                write(slotValue);
                firstSlot = false;
            }
            if (!firstSlot) write("\n");

            decreaseIndent();

            firstPart = false;
        }
        if (!firstPart) write("\n");

        decreaseIndent();
        write("}");
    }

    void writeName(Pattern* pattern)
    {
        if (!pattern)
        {
            write("null");
            return;
        }

        write("[");
        increaseIndent();

        bool first = true;
        for (auto mixin : pattern->_mixins)
        {
            if (!first) write(", ");
            else first = false;
            writeRef(mixin);
        }

        decreaseIndent();
        write("]");
    }

    void writeRef(Pattern* pattern)
    {
        writeName(pattern);
    }

    void write(Pattern* pattern)
    {
        writeRef(pattern);
    }

    void increaseIndent() { indent++; }
    void decreaseIndent() { indent--; }
};

void dumpObject(Object* object)
{
    Writer writer;
    writer.file = stdout;

    writer.write(object);
}

class VM
{
public:
    struct Frame
    {
        BCDecl const* _decl;
        CodeChunk const* _chunk;
        Byte const* _ip;
        std::vector<Value> _stack;
        Part* _self;

        Frame* _parent = nullptr;
    };

    Frame* _frame = nullptr;

    Pattern* loadProgram(BCDecl* bcProgram)
    {
        Pattern* pattern = new Pattern();

        Mixin* mixin = new Mixin(pattern, bcProgram, nullptr);

        pattern->_mixins.push_back(mixin);

        return pattern;
    }

    void pushFrame(BCDecl const* decl, CodeChunk const* chunk, Part* part)
    {
        Frame* frame = new Frame();
        frame->_decl = decl;
        frame->_chunk = chunk;
        frame->_ip = chunk->_bytes.data();
        frame->_self = part;

        frame->_parent = _frame;
        _frame = frame;
    }

    void popFrame()
    {
        _frame = _frame->_parent;
    }

    void initializePart(Part* part)
    {
        auto mixin = part->_mixin;
        auto mainPart = mixin->_decl;

        // Okay, now we run the initialization logic!!!

        // iterate over the members, and initialize them
        // based on their provided logic...

        for( auto member : mainPart->_members )
        {
            pushFrame(member, &member->initCode, part);

            execute();
        }

    }

    Object* createObject(Pattern* pattern)
    {
        Object* object = new Object(pattern);

        size_t partCount = pattern->_mixins.size();

        // First allocate all the parts, without initializing them
        for( auto mixin : pattern->_mixins )
        {
            auto mainPart = mixin->_decl;

            Part* part = new Part(object, mixin);

            object->_parts.push_back(part);
        }

        // Now walk through the parts and run their intialization logic
        for( auto part : object->_parts )
        {
            initializePart(part);
        }

        return object;
    }

    void runObject(Object* object)
    {
        // Basically, we want to run the "do part" of the object...
        //
        // This will always start with the most-general part object,
        // and work its way to the most-specialized...
        //
        if (object->_parts.size() == 0)
            return;

        auto part = object->_parts[0];
        auto decl = part->_mixin->_decl;

        pushFrame(decl, &decl->bodyCode, part);
        execute();
    }

    void execute(BCDecl* bcProgram)
    {
        bcProgram->dump();

        auto pattern = loadProgram(bcProgram);
        auto object = createObject(pattern);

        runObject(object);

        // TODO: now run the `do` part of `object`

        dumpObject(object);
    }

    Byte readByte()
    {
        return *_frame->_ip++;
    }

    unsigned int readUInt()
    {
        return readByte();
    }

    Opcode readOpcode()
    {
        return Opcode(readByte());
    }

    Value getConstant(unsigned int index)
    {
        return _frame->_chunk->_constants[index];
    }

    Value readConstant()
    {
        return getConstant(readUInt());
    }

    void push(Value value)
    {
        _frame->_stack.push_back(value);
    }

    Value pop()
    {
        Value result = _frame->_stack.back();
        _frame->_stack.pop_back();
        return result;
    }

    void execute()
    {
        for( ;;)
        {
            Opcode opcode = readOpcode();
            switch( opcode )
            {
            default:
                error(SourceLoc(), "invalid opcode");
                return;

            case Opcode::Nop:
                break;

            case Opcode::Pop:
                pop();
                break;

            case Opcode::Constant:
                push(readConstant());
                break;


            case Opcode::Return:
                popFrame();
                return;

            case Opcode::CreateObject:
                {
                    auto pattern = (Pattern*) pop().getPtr();
                    auto object = createObject(pattern);
                    push(object);
                }
                break;

            case Opcode::SetPartSlot:
                {
                    auto slotIndex = readUInt();
                    auto value = pop();
                    auto part = (Part*) pop().getPtr();

                    part->_slots[slotIndex] = value;
                }
                break;

            case Opcode::GetPartSlot:
                {
                    auto slotIndex = readUInt();
                    auto part = (Part*) pop().getPtr();

                    auto value = part->_slots[slotIndex];
                    push(value);
                }
                break;

            case Opcode::CreatePatternFromMainPart:
                {
                    auto pattern = new Pattern();

                    auto mainPart = _frame->_decl;

                    auto mainPartMixin = new Mixin(pattern, mainPart, _frame->_self);
                    pattern->_mixins.push_back(mainPartMixin);

                    push(pattern);
                }
                break;

            case Opcode::CreatePatternFromBaseAndMainPart:
                {
                    auto basePattern = (Pattern*) pop().getPtr();

                    auto pattern = new Pattern();

                    for( auto baseMixin : basePattern->_mixins )
                    {
                        auto mixin = new Mixin(pattern, baseMixin->_decl, baseMixin->_origin);
                        pattern->_mixins.push_back(mixin);
                    }

                    auto mainPart = _frame->_decl;

                    auto mainPartMixin = new Mixin(pattern, mainPart, _frame->_self);
                    pattern->_mixins.push_back(mainPartMixin);

                    push(pattern);
                }
                break;

            case Opcode::GetEmptyPattern:
                {
                    if( !_emptyPattern )
                    {
                        _emptyPattern = new Pattern();
                    }
                    push(_emptyPattern);
                }
                break;

            case Opcode::GetSelfPart:
                {
                    push(_frame->_self);
                }
                break;

            case Opcode::GetObjectFromPart:
                {
                    auto part = (Part*) pop().getPtr();
                    auto object = part->_parentObject;
                    push(object);
                }
                break;

            case Opcode::GetPartFromObject:
                {
                    auto object = (Object*) pop().getPtr();
                    // TODO: how to pick the right part?
                    auto part = object->_parts[0];
                    push(part);
                }
                break;

            case Opcode::GetMixinFromPart:
                {
                    auto part = (Part*) pop().getPtr();
                    auto mixin = part->_mixin;
                    push(mixin);
                }
                break;

            case Opcode::GetOriginPartFromMixin:
                {
                    auto mixin = (Mixin*) pop().getPtr();
                    auto part = mixin->_origin;
                    push(part);
                }
                break;
            }
        }
    }

private:
    static Pattern* _emptyPattern;
};

Pattern* VM::_emptyPattern = nullptr;

}

}
