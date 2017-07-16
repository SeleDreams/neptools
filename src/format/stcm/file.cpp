#include "file.hpp"
#include "gbnl.hpp"
#include "header.hpp"
#include "../eof_item.hpp"
#include "../item.hpp"
#include "../../open.hpp"

namespace Neptools::Stcm
{

File::File(Source src)
{
    AddInfo(&File::Parse_, ADD_SOURCE(src), this, src);
}

void File::Parse_(Source& src)
{
    auto root = Create<RawItem>(src);
    SetupParseFrom(*root);
    root->Split(root->GetSize(), Create<EofItem>());
    HeaderItem::CreateAndInsert({root.get(), 0});
}

void File::Inspect_(std::ostream& os) const
{
    Item::Inspect_(os);
    os << "neptools.stcm.file()";
    InspectChildren(os);
}

std::vector<NotNull<SmartPtr<const GbnlItem>>> File::FindGbnl() const
{
    std::vector<NotNull<SmartPtr<const GbnlItem>>> ret;
    FindGbnl_<const Item, const GbnlItem>(*this, ret);
    return ret;
}

std::vector<NotNull<SmartPtr<GbnlItem>>> File::FindGbnl()
{
    std::vector<NotNull<SmartPtr<GbnlItem>>> ret;
    FindGbnl_<Item, GbnlItem>(*this, ret);
    return ret;
}

namespace
{
// automatically determine if we need dynamic_cast to ptr* or const ptr*
template <typename T, typename Ref> struct MkPtr;
template <typename T, typename Ref>
struct MkPtr<T*, Ref> { using type = T*; };
template <typename T, typename Ref>
struct MkPtr<T*, const Ref> { using type = const T*; };

template <typename T, typename Ref>
using MkPtrT = typename MkPtr<T, Ref>::type;
}

template <typename ItemT, typename GbnlT>
void File::FindGbnl_(
    ItemT& root, std::vector<NotNull<SmartPtr<GbnlT>>>& vect) const
{
    auto x = dynamic_cast<MkPtrT<Stcm::GbnlItem*, ItemT>>(&root);
    if (x)
    {
        vect.emplace_back(x);
        return;
    }

    auto ch = dynamic_cast<MkPtrT<ItemWithChildren*, ItemT>>(&root);
    if (ch)
        for (auto& c : ch->GetChildren())
            FindGbnl_<ItemT, GbnlT>(c, vect);
}

void File::WriteTxt_(std::ostream& os) const
{
    for (auto& x : FindGbnl())
        x->WriteTxt(os);
}

void File::ReadTxt_(std::istream& is)
{
    for (auto& x : FindGbnl())
        x->ReadTxt(is);
}

static OpenFactory stcm_open{[](Source src) -> SmartPtr<File>
{
    if (src.GetSize() < sizeof(HeaderItem::Header)) return nullptr;
    char buf[4];
    src.PreadGen(0, buf);
    if (memcmp(buf, "STCM", 4) == 0)
        return MakeSmart<File>(src);
    else
        return nullptr;
}};

}

#include "../../lua/table_ret_wrap.hpp"
#include "file.binding.hpp"
