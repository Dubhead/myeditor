#pragma once

#include <memory>
#include <string>
#include <boost/optional.hpp>
#include "global.h"

class File
{
public:
    explicit File(const std::string& path);
    virtual ~File();
    boost::optional<std::string> init();
    void addBuffer(const GsvBuffer&);
    void deleteBuffer(const GsvBuffer&);
    GioFile getGioFile();
    GsvBuffer getNewBuffer();
    void save(const std::string& text);
private:
    File(const File&) = delete;	// copy ctor
    File(File&&) = delete;
    File& operator=(const File&) = delete;
    File& operator=(File&&) = delete;

    class Impl;
    const std::unique_ptr<Impl> pimpl;
};
