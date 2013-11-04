#pragma once

#include <memory>
#include <string>
#include "global.h"
#include "editwindow.h"
#include "file.h"

class FileWindow: public EditWindow
{
public:
    FileWindow();
    virtual ~FileWindow();
    void init();
    std::shared_ptr<File> getFile();
    void save(const std::string& altFilename);
    void setBuffer(const GsvBuffer& buf);
    void setFile(std::shared_ptr<File> file);
    const std::string shortDesc();
private:
    FileWindow(const FileWindow&) = delete;	// copy ctor
    FileWindow(FileWindow&&) = delete;
    FileWindow& operator=(const FileWindow&) = delete;
    FileWindow& operator=(FileWindow&&) = delete;

    class Impl;
    const std::unique_ptr<Impl> pimpl;
};

// eof
