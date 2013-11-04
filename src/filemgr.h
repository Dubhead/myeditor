#pragma once

#include <memory>
#include <vector>
#include <boost/optional.hpp>
#include "global.h"
#include "file.h"

class FileMgr
{
public:
    FileMgr();
    virtual ~FileMgr();
    void init();
    void cleanup();
    void deleteFile(std::shared_ptr<File> f);
    boost::optional<std::shared_ptr<File>> getFile(const std::string& path,
	const bool supressErrorMsg=false);
    std::vector<std::string> getFileNames();
    std::vector<std::string> getRecentFiles();
private:
    FileMgr(const FileMgr&) = delete;	// copy ctor
    FileMgr(FileMgr&&) = delete;
    FileMgr& operator=(const FileMgr&) = delete;
    FileMgr& operator=(FileMgr&&) = delete;

    class Impl;
    const std::unique_ptr<Impl> pimpl;
};

// eof
