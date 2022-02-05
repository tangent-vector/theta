// vm.h
#pragma once

#include "basic.h"

namespace theta
{
namespace vm
{
using namespace bytecode;

struct Mixin;
struct Object;
struct Part;
struct Pattern;
struct SimplePattern;

struct MixinList
{
    Mixin* _first;

    struct Iterator
    {
    public:
        Iterator(Mixin* mixin)
            : _mixin(mixin)
        {}

        bool operator!=(Iterator const& that) const
        {
            return _mixin != that._mixin;
        }

        void operator++();

        Mixin* operator*() const
        {
            return _mixin;
        }

    private:
        Mixin* _mixin;
    };

    Iterator begin() const
    {
        return Iterator(_first);
    }

    Iterator end() const
    {
        return Iterator(nullptr);
    }
};

    // Base case for all patterns (e.g., including a pattern for `L & R`)
struct Pattern : ValueObj
{
    SimplePattern* getSimplePattern();

        // The mixin sequence that defines this pattern
    MixinList _mixins;

    MixinList const& getMixins() { return _mixins; }

    bool isEmpty() { return _mixins._first == nullptr; }

};

    // Common case for patterns, that are built out of a sequence of mixins
struct SimplePattern : Pattern
{
    // The total size, in bytes, of instance objects created from this pattern
    size_t _instanceSize = 0;

    Size getInstanceSize() { return _instanceSize; }
};

    // The empty pattern: used when we need to have a non-null object
    // to represent this case...
struct EmptyPattern : SimplePattern
{
public:
    static EmptyPattern* get()
    {
        static EmptyPattern* result = new EmptyPattern();
        return result;
    }
private:
    EmptyPattern()
    {}
};

    // The common case of patterns, where it one or more mixins
struct Mixin : SimplePattern
{
    Mixin(
        BCDecl const* decl,
        Part* origin,
        Mixin* next);

    BCDecl const* getDecl() { return _decl; }
    Part* getOrigin() { return _origin; }
    Count getSlotCount() { return _decl->_slotCount; }

    Offset getPartOffset() { return _partOffset; }

    typedef BCDecl::MemberList MemberList;
    MemberList const& getMembers() { return getDecl()->getMembers(); }

        // The declaration that corresponds to this "link" in the mixin chain
    BCDecl const* _decl = nullptr;

        // The origin part that corresponds to this "link" in the mixin chain
    Part* _origin = nullptr;

        // The offset of the part for this mixin in an allocated object
    size_t _partOffset = 0;

        // The next mixin in the chain for this pattern
        // Note: a null pointer here is equivalent of the next link being the `EmptyPattern`
    Mixin* _next = nullptr;

        // TODO: pointers to the mixins corresponding to the base(s) of this mixin
};

inline SimplePattern* Pattern::getSimplePattern()
{
    SimplePattern* firstMixin = _mixins._first;
    return firstMixin ? firstMixin : EmptyPattern::get();
}

inline void MixinList::Iterator::operator++()
{
    _mixin = _mixin->_next;
}

struct PartList
{
public:
    PartList(Object* object)
        : _object(object)
    {}

    struct Iterator
    {
    public:
        Iterator(Object* object, MixinList::Iterator const& mixinIter)
            : _object(object)
            , _mixinIter(mixinIter)
        {}

        bool operator!=(Iterator const& that) const
        {
            return _mixinIter != that._mixinIter;
        }

        void operator++()
        {
            ++_mixinIter;
        }
        Part* operator*() const;

    private:
        Object* _object;
        MixinList::Iterator _mixinIter;
    };

    Iterator begin() const;

    Iterator end() const;

private:
    Object* _object;
};

struct Object : ValueObj
{
    Object(SimplePattern* pattern)
        : _pattern(pattern)
    {}

        // The direct run-time pattern  that this object was created from
    SimplePattern* _pattern = nullptr;

    SimplePattern* getPattern() { return _pattern; }

    // The remainder of an `Object`s state consists of tail-allocated
    // `Part` objects of varying size, coresponding to the parts described
    // by `_pattern`.

    Part* getFirstPart()
    {
        return *getParts().begin();
    }

    PartList getParts()
    {
        return PartList(this);
    }

    Part* getPartForMixin(Mixin* mixin)
    {
        return getPartAtOffset(mixin->getPartOffset());
    }

    Part* getPartAtOffset(Offset offset)
    {
        return (Part*)((char*)this + offset);
    }
};

struct SlotList
{
public:
    SlotList(Value* begin, Count count)
        : _begin(begin)
        , _end(begin + count)
    {}

    typedef Value* Iterator;

    Iterator begin() const { return _begin; }
    Iterator end() const { return _end; }

private:
    Value* _begin;
    Value* _end;
};

struct Part : ValueObj
{
    Part(Mixin* mixin)
        : _mixin(mixin)
    {}

        // The mixin that this part corresponds to
    Mixin* _mixin = nullptr;

    Object* getObject()
    {
        return (Object*)((char*)this - getMixin()->getPartOffset());
    }

    Mixin* getMixin() { return _mixin; }
    BCDecl const* getDecl() { return getMixin()->getDecl(); }
    Part* getOrigin() { return getMixin()->getOrigin(); }

    Part* getBase(Index baseIndex);

    // The remainder of a `Part`s state consists of tail-allocated
    // `Value`s, corresponding to the slots described by `_mixin`.

    Count getSlotCount()
    {
        return getMixin()->getSlotCount();
    }
    SlotList getSlots()
    {
        return SlotList(_getSlots(), getSlotCount());
    }

    Value* _getSlots()
    {
        return (Value*)(this + 1);
    }

    Value& refSlot(Index index)
    {
        return _getSlots()[index];
    }

    Value getSlot(Index index)
    {
        return _getSlots()[index];
    }

    void setSlot(Index index, Value const& value)
    {
        _getSlots()[index] = value;
    }

};

//

Mixin::Mixin(
    BCDecl const* decl,
    Part* origin,
    Mixin* next)
    : _decl(decl)
    , _origin(origin)
    , _next(next)
{
    _mixins._first = this;

    Size existingSize = next ? next->getInstanceSize() : sizeof(Object);

    Size partSize = sizeof(Part) + getSlotCount() * sizeof(Value);

    _partOffset = existingSize;
    _instanceSize = existingSize + partSize;
}

PartList::Iterator PartList::begin() const
{
    return Iterator(_object, _object->getPattern()->getMixins().begin());
}

PartList::Iterator PartList::end() const
{
    return Iterator(_object, _object->getPattern()->getMixins().end());
}

Part* PartList::Iterator::operator*() const
{
    return _object->getPartForMixin(*_mixinIter);
}


//

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
        writeName(part->getObject());
        write("[");
        writeRef(part->getMixin());
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
        for( auto part : object->getParts() )
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
            for (auto slotValue : part->getSlots())
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
        for (auto mixin : pattern->getMixins())
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
        Mixin* mixin = new Mixin(bcProgram, nullptr, nullptr);
        return mixin;
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

    void initializePart(Part* part, Mixin* mixin)
    {

        // Okay, now we run the initialization logic!!!

        // iterate over the members, and initialize them
        // based on their provided logic...

        for( auto member : mixin->getMembers() )
        {
            pushFrame(member, &member->initCode, part);

            execute();
        }

    }

    Object* createObject(SimplePattern* pattern)
    {
        Size instanceSize = pattern->getInstanceSize();

        void* objectMemory = malloc(instanceSize);
        memset(objectMemory, 0, instanceSize);

        // Run constructors to get things into a basic
        // constructed-but-unitinitialized state.
        //
        Object* object = new(objectMemory) Object(pattern);
        for (auto mixin : pattern->getMixins())
        {
            void* partMemory = object->getPartForMixin(mixin);
            Part* part = new(partMemory) Part(mixin);
        }

        // Now run per-part initialization logic.
        //
        // Note: we run the code on a sub-VM because this one
        // could already be executing code on its stack, and
        // could be creating an object as part of implementing
        // one of its opcodes.
        //
        // TODO: make the `createObject()` path be "stackless"
        // so that it can run the initialization code of sub-objects
        // directly on the stack.
        //
        // E.g., we could have this code push all the necessary
        // frames (might need to be in the reverse of the order given
        // here...) and then return the object *without* calling
        // `execute()`, so that resuming the VM would automatically
        // run all those frames in order (each returning to the next).
        //
        // Alternatively, the init code for each member could have built-in
        // logic to jump to the initialization of the next member (or just
        // concatenate them, of course...).
        //
        VM subVM;
        for (auto part : object->getParts())
        {
            auto mixin = part->getMixin();

            for (auto member : mixin->getMembers())
            {
                subVM.pushFrame(member, &member->initCode, part);
                subVM.execute();
            }
        }

        // TODO: We logically want to run the "do" part of
        // the object here as well...

        return object;
    }

    Object* createObject(Pattern* pattern)
    {
        return createObject(pattern->getSimplePattern());
    }

    void runObject(Object* object)
    {
        // We basically want to "wind up" all the code that
        // makes up the body of the parts of `object`, from
        // the most-general part to the most-specialized.
        //
        // In practice, the parts come in pre-ordered in
        // exactly that order (most general first, then next
        // most general, etc.)
        //
        // We have to handle the slightly annoying special case
        // of an object with *no* parts first.
        //
        if( object->getPattern()->isEmpty() )
            return;

        // Otherwise, we can simply start running the body code
        // of the first part in order, and assume that its `Inner`
        // ops will migrate to subsequence parts as needed.
        //
        auto part = object->getFirstPart();
        auto decl = part->getDecl();
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

            case Opcode::Inner:
                {
                    auto currentPart = _frame->_self;
                    auto currentMixin = currentPart->getMixin();

                    auto innerMixin = currentMixin->_next;
                    if (innerMixin)
                    {
                        auto object = currentPart->getObject();
                        auto innerPart = object->getPartForMixin(innerMixin);

                        auto innerDecl = innerPart->getDecl();
                        pushFrame(innerDecl, &innerDecl->bodyCode, innerPart);
                    }
                }
                break;

            case Opcode::Nop:
                break;

            case Opcode::Pop:
                pop();
                break;

            case Opcode::Constant:
                push(readConstant());
                break;


            case Opcode::Return:
                {
                    popFrame();
                    if (!_frame)
                        return;
                }
                break;

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

                    part->setSlot(slotIndex, value);
                }
                break;

            case Opcode::GetPartSlot:
                {
                    auto slotIndex = readUInt();
                    auto part = (Part*) pop().getPtr();

                    auto value = part->getSlot(slotIndex);
                    push(value);
                }
                break;

            case Opcode::CreatePatternFromMainPart:
                {
                    auto mixin = new Mixin(_frame->_decl, _frame->_self, nullptr);

                    push(mixin);
                }
                break;

            case Opcode::CreatePatternFromBaseAndMainPart:
                {
                    // TODO: We need to handle any cases that do *not* evaluate to
                    // a mixin-based pattern elsewhere...

                    auto basePattern = (Mixin*) pop().getPtr();

                    auto pattern = new Mixin(_frame->_decl, _frame->_self, basePattern);

                    push(pattern);
                }
                break;

            case Opcode::GetEmptyPattern:
                {
                    auto pattern = EmptyPattern::get();
                    push(pattern);
                }
                break;

            case Opcode::GetSelfPart:
                {
                    push(_frame->_self);
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
};

}

}
