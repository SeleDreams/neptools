#ifndef UUID_830A2646_22D6_49BF_A2D9_7E03977404C7
#define UUID_830A2646_22D6_49BF_A2D9_7E03977404C7
#pragma once

#include "../../buffer.hpp"
#include "../context.hpp"

namespace Cl3
{

class HeaderItem;
class FileCollectionItem;

class File : public Context
{
public:
    File();
    File(std::shared_ptr<Buffer> buf) : File{buf, 0, buf->GetSize()} {}
    File(std::shared_ptr<Buffer> buf, size_t offset, size_t len);
    File(const std::string& fname) : File{ReadFile(fname)} {}
    File(const char* fname) : File{ReadFile(fname)} {}

    void Fixup() override;

    const HeaderItem& GetHeader() const noexcept;
    HeaderItem& GetHeader() noexcept;

    const FileCollectionItem& GetFileCollection() const;
    FileCollectionItem& GetFileCollection();
};

Item* PadItem(Item* item);

}
#endif