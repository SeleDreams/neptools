#ifndef UUID_51AA015D_E824_48D3_8BB4_37BC559302DA
#define UUID_51AA015D_E824_48D3_8BB4_37BC559302DA
#pragma once

#include "../item.hpp"
#include "../../lua/value_object.hpp"
#include "../../source.hpp"
#include <boost/endian/arithmetic.hpp>

namespace Neptools
{
namespace Stcm
{

class CollectionLinkHeaderItem final : public Item
{
    NEPTOOLS_DYNAMIC_OBJECT;
public:
    struct Header
    {
        boost::endian::little_uint32_t field_00;
        boost::endian::little_uint32_t offset;
        boost::endian::little_uint32_t count;
        boost::endian::little_uint32_t field_0c;
        boost::endian::little_uint32_t field_10;
        boost::endian::little_uint32_t field_14;
        boost::endian::little_uint32_t field_18;
        boost::endian::little_uint32_t field_1c;
        boost::endian::little_uint32_t field_20;
        boost::endian::little_uint32_t field_24;
        boost::endian::little_uint32_t field_28;
        boost::endian::little_uint32_t field_2c;
        boost::endian::little_uint32_t field_30;
        boost::endian::little_uint32_t field_34;
        boost::endian::little_uint32_t field_38;
        boost::endian::little_uint32_t field_3c;

        void Validate(FilePosition file_size) const;
    };
    NEPTOOLS_STATIC_ASSERT(sizeof(Header) == 0x40);

    CollectionLinkHeaderItem(Key k, Context& ctx, NotNull<LabelPtr> data)
        : Item{k, ctx}, data{std::move(data)} {}

    NEPTOOLS_NOLUA
    CollectionLinkHeaderItem(Key k, Context& ctx, const Header& s);
    static CollectionLinkHeaderItem& CreateAndInsert(ItemPointer ptr);

    FilePosition GetSize() const noexcept override
    { return sizeof(Header); }

    NotNull<LabelPtr> data;

private:
    void Dump_(Sink& sink) const override;
    void Inspect_(std::ostream& os) const override;
};

class CollectionLinkItem final : public Item
{
    NEPTOOLS_DYNAMIC_OBJECT;
public:
    struct Entry
    {
        boost::endian::little_uint32_t name_0;
        boost::endian::little_uint32_t name_1;
        boost::endian::little_uint32_t ptr; // filled by engine
        boost::endian::little_uint32_t field_0c;
        boost::endian::little_uint32_t field_10;
        boost::endian::little_uint32_t field_14;
        boost::endian::little_uint32_t field_18;
        boost::endian::little_uint32_t field_1c;

        void Validate(FilePosition file_size) const;
    };
    NEPTOOLS_STATIC_ASSERT(sizeof(Entry) == 0x20);

    CollectionLinkItem(Key k, Context& ctx) : Item{k, ctx} {}
    CollectionLinkItem(Key k, Context& ctx, Source src, uint32_t count);

    FilePosition GetSize() const noexcept override
    { return entries.size() * sizeof(Entry); }

    struct LinkEntry : public Lua::ValueObject
    {
        NotNull<LabelPtr> name_0;
        NotNull<LabelPtr> name_1;

        LinkEntry(NotNull<LabelPtr> name_0, NotNull<LabelPtr> name_1)
            : name_0{std::move(name_0)}, name_1{std::move(name_1)} {}
        NEPTOOLS_LUA_CLASS;
    };
    NEPTOOLS_LUAGEN(get="::Neptools::Lua::GetSmartOwnedMember")
    std::vector<LinkEntry> entries;

    void Dispose() noexcept override;

private:
    void Dump_(Sink& sink) const override;
    void Inspect_(std::ostream& os) const override;
    void Parse_(Context& ctx, Source& src, uint32_t count);
};

}
}
#endif
