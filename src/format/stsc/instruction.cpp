#include "instruction.hpp"
#include "string.hpp"
#include "../raw_item.hpp"

namespace Neptools
{
namespace Stsc
{

// helpers for generic CreateAndInsert
namespace
{
using CreateType = std::unique_ptr<InstructionBase> (*)(Context*, const Source&);

template <uint8_t I>
std::unique_ptr<InstructionBase> CreateAdapt(Context* ctx, const Source& src)
{ return ctx->Create<InstructionItem<I>>(I, src); }

template <typename T> struct CreateMapImpl;
template <uint8_t... I>
struct CreateMapImpl<std::index_sequence<I...>>
{
    static const constexpr CreateType MAP[] = { CreateAdapt<I>... };
};

template <uint8_t... I>
const CreateType CreateMapImpl<std::index_sequence<I...>>::MAP[];

using CreateMap = CreateMapImpl<std::make_index_sequence<256>>;
}

// base
InstructionBase* InstructionBase::CreateAndInsert(ItemPointer ptr)
{
    auto x = RawItem::GetSource(ptr, -1);
    x.src.CheckSize(1);
    uint8_t opcode = x.src.Read<boost::endian::little_uint8_t>();
    return x.ritem.Split(ptr.offset, CreateMap::MAP[opcode](x.ritem.GetContext(), x.src));
}

void InstructionBase::InstrDump(Sink& sink) const
{
    sink.Write(boost::endian::little_uint8_t(opcode));
}

std::ostream& InstructionBase::InstrInspect(std::ostream& os) const
{
    Item::Inspect_(os);
    auto flags = os.flags();
    os << "instr(0x" << std::setw(2) << std::setfill('0') << std::hex
       << unsigned(opcode);
    os.flags(flags);
    return os;
}


// generic implementation
namespace
{

template <typename... Args> struct PODTuple;

template <typename Head, typename... Args>
struct PODTuple<Head, Args...>
{
    Head head;
    PODTuple<Args...> tail;
};

template <typename T> struct PODTuple<T> { T head; };
template<> struct PODTuple<> {};

template <size_t I> struct PODTupleGet
{
    template <typename T> static auto& Get(T& tuple)
    { return PODTupleGet<I-1>::Get(tuple.tail); }
};
template<> struct PODTupleGet<0>
{
    template <typename T> static auto& Get(T& tuple)
    { return tuple.head; }
};

template <size_t I, typename T> inline auto& Get(T& tuple)
{ return PODTupleGet<I>::Get(tuple); }


template <typename T> struct EndianMap;
template<> struct EndianMap<uint8_t>
{ using Type = boost::endian::little_uint8_t; };
template<> struct EndianMap<uint16_t>
{ using Type = boost::endian::little_uint16_t; };
template<> struct EndianMap<uint32_t>
{ using Type = boost::endian::little_uint32_t; };

template <typename T> using ToVoid = void;


template <typename T, typename Enable = void> struct Traits;

template <typename T>
struct Traits<T, ToVoid<typename EndianMap<T>::Type>>
{
    using RawType = typename EndianMap<T>::Type;
    static constexpr const size_t SIZE = sizeof(T);

    static void Validate(RawType, FilePosition) {}
    static T Parse(RawType r, Context*) { return r; }
    static RawType Dump(T r) { return r; }
    static void Inspect(std::ostream& os, T t) { os << uint32_t(t); }
};

template<> struct Traits<float>
{
    using RawType = boost::endian::little_uint32_t;
    static constexpr const size_t SIZE = 4;

    static void Validate(RawType, FilePosition) {}
    static float Parse(RawType r, Context*)
    {
        uint32_t num = r;
        float ret;
        memcpy(&ret, &num, sizeof(ret));
        return ret;
    }

    static RawType Dump(float f)
    {
        uint32_t ret;
        memcpy(&ret, &f, sizeof(ret));
        return ret;
    }

    static void Inspect(std::ostream& os, float v) { os << v; }
};

template<> struct Traits<const Label*>
{
    using RawType = boost::endian::little_uint32_t;
    static constexpr const size_t SIZE = 4;

    static void Validate(uint32_t r, FilePosition size)
    { NEPTOOLS_VALIDATE_FIELD("Stsc::Instruction", r < size); }

    static const Label* Parse(uint32_t r, Context* ctx)
    { return ctx->GetLabelTo(r); }

    static RawType Dump(const Label* l)
    { return ToFilePos(l->second); }

    static void Inspect(std::ostream& os, const Label* l)
    { os << '@' << l->first; }
};

template<> struct Traits<std::string> : public Traits<const Label*>
{
    static const Label* Parse(uint32_t r, Context* ctx)
    {
        auto lbl = Traits<const Label*>::Parse(r, ctx);
        if (lbl->second.Maybe<RawItem>())
            StringItem::CreateAndInsert(lbl->second);
        return lbl;
    }
};

template <typename T, typename... Args> struct OperationsImpl;
template <typename... T, size_t... I>
struct OperationsImpl<std::index_sequence<I...>, T...>
{
    using Swallow = int[];
#define FORALL(...) (void) Swallow{0, ((void)(__VA_ARGS__), 0)...}

    template <typename Tuple>
    static void Validate(const Tuple& tuple, FilePosition size)
    { FORALL(Traits<T>::Validate(Get<I>(tuple), size)); }

    template <typename Dst, typename Src>
    static void Parse(Dst& dst, const Src& src, Context* ctx)
    { FORALL(std::get<I>(dst) = Traits<T>::Parse(Get<I>(src), ctx)); }

    template <typename Dst, typename Src>
    static void Dump(Dst& dst, const Src& src)
    { FORALL(Get<I>(dst) = Traits<T>::Dump(std::get<I>(src))); }

    template <typename Tuple>
    static void Inspect(std::ostream& os, const Tuple& tuple)
    {
        FORALL(
            os << ", ",
            Traits<T>::Inspect(os, std::get<I>(tuple)));
    }

    static constexpr size_t Size()
    {
        size_t sum = 0;
        FORALL(sum += Traits<T>::SIZE);
        return sum;
    }
#undef FORALL
};

template <typename... Args>
using Operations = OperationsImpl<std::index_sequence_for<Args...>, Args...>;

}

template <typename... Args>
const FilePosition SimpleInstruction<Args...>::SIZE =
    Operations<Args...>::Size() + 1;

template <typename... Args>
SimpleInstruction<Args...>::SimpleInstruction(
    Key k, Context* ctx, uint8_t opcode, Source src)
    : InstructionBase{k, ctx, opcode}
{
    AddInfo(&SimpleInstruction::Parse_, ADD_SOURCE(src), this, src);
}

template <typename... Args>
void SimpleInstruction<Args...>::Parse_(Source& src)
{
    src.CheckSize(SIZE);
    using Tuple = PODTuple<typename Traits<Args>::RawType...>;
    NEPTOOLS_STATIC_ASSERT(std::is_pod<Tuple>::value);
    NEPTOOLS_STATIC_ASSERT(EmptySizeof<Tuple> == Operations<Args...>::Size());

    Tuple raw = src.Read<Tuple>();

    Operations<Args...>::Validate(raw, GetContext()->GetSize());
    Operations<Args...>::Parse(args, raw, GetContext());
}

template <typename... Args>
void SimpleInstruction<Args...>::Dump_(Sink& sink) const
{
    InstrDump(sink);

    using Tuple = PODTuple<typename Traits<Args>::RawType...>;
    Tuple t;
    Operations<Args...>::Dump(t, args);
    sink.Write(t);
}

template <typename... Args>
void SimpleInstruction<Args...>::Inspect_(std::ostream& os) const
{
    InstrInspect(os);
    Operations<Args...>::Inspect(os, args);
    os << ')';
}

// specific instruction implementations
Instruction0dItem::Instruction0dItem(
    Key k, Context* ctx, uint8_t opcode, Source src)
    : InstructionBase{k, ctx, opcode}
{
    AddInfo(&Instruction0dItem::Parse_, ADD_SOURCE(src), this, src);
}

void Instruction0dItem::Parse_(Source& src)
{
    src.CheckRemaining(1);
    uint8_t n = src.Read<boost::endian::little_uint8_t>();
    src.CheckRemaining(4*n);

    tgts.reserve(n);

    for (size_t i = 0; i < n; ++i)
    {
        uint32_t t = src.Read<boost::endian::little_uint32_t>();
        NEPTOOLS_VALIDATE_FIELD(
            "Stsc::Instruction0dItem", t < GetContext()->GetSize());
        tgts.push_back(GetContext()->GetLabelTo(t));
    }
}

void Instruction0dItem::Dump_(Sink& sink) const
{
    InstrDump(sink);
    sink.Write(boost::endian::little_uint8_t(tgts.size()));
    for (auto l : tgts)
        sink.Write(boost::endian::little_uint32_t(ToFilePos(l->second)));
}

void Instruction0dItem::Inspect_(std::ostream& os) const
{
    InstrInspect(os);
    bool first = true;
    for (auto l : tgts)
    {
        if (!first) os << ", ";
        first = false;
        os << '@' << l->first;
    }
    os << ')';
}

Instruction19Item::Instruction19Item(
    Key k, Context* ctx, uint8_t opcode, Source src)
    : InstructionBase{k, ctx, opcode}
{
    abort();
}

void Instruction1dItem::FixParams::Validate(
    FilePosition rem_size, FilePosition size)
{
#define VALIDATE(x) NEPTOOLS_VALIDATE_FIELD("Stsc::Instruction1dItem::FixParams", x)
    VALIDATE(this->size * sizeof(NodeParams) <= rem_size);
    VALIDATE(tgt < size);
#undef VALIDATE
}

void Instruction1dItem::NodeParams::Validate(uint16_t size)
{
#define VALIDATE(x) NEPTOOLS_VALIDATE_FIELD("Stsc::Instruction1dItem::NodeParams", x)
    VALIDATE(left <= size);
    VALIDATE(right <= size);
#undef VALIDATE
}

Instruction1dItem::Instruction1dItem(
    Key k, Context* ctx, uint8_t opcode, Source src)
    : InstructionBase{k, ctx, opcode}
{
    AddInfo(&Instruction1dItem::Parse_, ADD_SOURCE(src), this, src);
}

void Instruction1dItem::Parse_(Source& src)
{
    src.CheckRemaining(sizeof(FixParams));
    auto fp = src.Read<FixParams>();
    fp.Validate(src.GetRemainingSize(), GetContext()->GetSize());
    tgt = GetContext()->GetLabelTo(fp.tgt);

    uint16_t n = fp.size;
    src.CheckRemaining(n * sizeof(NodeParams));
    tree.reserve(n);
    for (uint16_t i = 0; i < n; ++i)
    {
        auto nd = src.Read<NodeParams>();
        nd.Validate(n);
        tree.push_back({nd.operation, nd.value, nd.left, nd.right});
    }
}

void Instruction1dItem::Dump_(Sink& sink) const
{
    InstrDump(sink);
    sink.Write(FixParams{tree.size(), ToFilePos(tgt->second)});
    for (auto& n : tree)
        sink.Write(NodeParams{n.operation, n.value, n.left, n.right});
}

void Instruction1dItem::Inspect_(std::ostream& os) const
{
    InstrInspect(os) << ", @" << tgt->first << ", ";
    InspectNode(os, 0);
    os << ')';
}

void Instruction1dItem::InspectNode(std::ostream& os, size_t i) const
{
    if (i >= tree.size())
    {
        os << "nil";
        return;
    }

    auto& n = tree[i];
    os << '{' << unsigned(n.operation) << ", " << n.value << ", ";
    InspectNode(os, n.left - 1);
    os << ", ";
    InspectNode(os, n.right - 1);
    os << '}';
}

void Instruction1eItem::FixParams::Validate(FilePosition rem_size)
{
    NEPTOOLS_VALIDATE_FIELD("Stsc::Instruction1eItem::FixParams",
                            size * sizeof(ExpressionParams) <= rem_size);
}

void Instruction1eItem::ExpressionParams::Validate(FilePosition size)
{
    NEPTOOLS_VALIDATE_FIELD("Stsc::Instruction1eItem::ExpressionParams",
                            tgt < size);
}

Instruction1eItem::Instruction1eItem(
    Key k, Context* ctx, uint8_t opcode, Source src)
    : InstructionBase{k, ctx, opcode}
{
    AddInfo(&Instruction1eItem::Parse_, ADD_SOURCE(src), this, src);
}

void Instruction1eItem::Parse_(Source& src)
{
    src.CheckRemaining(sizeof(FixParams));
    auto fp = src.Read<FixParams>();
    fp.Validate(src.GetRemainingSize());

    field_0 = fp.field_0;
    flag = fp.size & 0x8000;
    auto size = flag ? fp.size & 0x7ff : uint16_t(fp.size);

    expressions.reserve(size);
    for (uint16_t i = 0; i < size; ++i)
    {
        auto exp = src.Read<ExpressionParams>();
        exp.Validate(GetContext()->GetSize());
        expressions.push_back({exp.expression, GetContext()->GetLabelTo(exp.tgt)});
    }
}

void Instruction1eItem::Dump_(Sink& sink) const
{
    InstrDump(sink);
    sink.Write(FixParams{field_0, (flag << 15) | expressions.size()});
    for (auto& e : expressions)
        sink.Write(ExpressionParams{e.first, ToFilePos(e.second->second)});

}

void Instruction1eItem::Inspect_(std::ostream& os) const
{
    InstrInspect(os) << ", " << field_0 << ", " << flag << ", {";
    bool first = true;
    for (auto& e : expressions)
    {
        if (!first) os << ", ";
        first = false;
        os << '{' << e.first << ", @" << e.second->first << '}';
    }
    os << "})";
}

}
}
