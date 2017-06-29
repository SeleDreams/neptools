// Auto generated code, do not edit. See gen_binding in project root.
#ifndef NEPTOOLS_WITHOUT_LUA
#include "lua/user_type.hpp"

namespace Neptools::Lua
{

// class neptools.stcm.gbnl_item
template<>
void TypeRegisterTraits<::Neptools::Stcm::GbnlItem>::Register(TypeBuilder& bld)
{
    bld.Inherit<::Neptools::Stcm::GbnlItem, ::Neptools::Item, ::Neptools::Gbnl>();

    bld.AddFunction<
        &::Neptools::Lua::TypeTraits<::Neptools::Stcm::GbnlItem>::Make<LuaGetRef<::Neptools::Item::Key>, LuaGetRef<::Neptools::Context &>, LuaGetRef<::Neptools::Source>>
    >("new");
    bld.AddFunction<
        static_cast<::Neptools::Stcm::GbnlItem & (*)(::Neptools::ItemPointer)>(::Neptools::Stcm::GbnlItem::CreateAndInsert)
    >("create_and_insert");

}
static TypeRegister::StateRegister<::Neptools::Stcm::GbnlItem> reg_neptools_stcm_gbnl_item;

}


const char ::Neptools::Stcm::GbnlItem::TYPE_NAME[] = "neptools.stcm.gbnl_item";

#endif