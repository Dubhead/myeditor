#pragma once

#include <memory>
#include <string>
#include <boost/optional.hpp>
#include "global.h"
#include "editwindow.h"
#include "filewindow.h"
#include "scratchwindow.h"

class Column: public Gtk::Grid
{
public:
    Column();
    virtual ~Column();
    void init();
    void addWindow(EditWindow& ew, EditWindow& sibling);
    void appendWindow(const EditWindow&);
    boost::optional<unsigned int> closeWindow(EditWindow*);
    unsigned int closeWindowsForFile(std::shared_ptr<File> f);
    boost::optional<EditWindow*> getEditWindow(const unsigned int rowNum);
    boost::optional<FileWindow*> getFileWindow(const std::string& path);
    boost::optional<unsigned int> getRow(const EditWindow* ew);
    boost::optional<ScratchWindow*> getScratchWindow();
    void replaceWindow(EditWindow* oldEW, EditWindow* newEW);
private:
    Column(const Column&) = delete;	// copy ctor
    Column(Column&&) = delete;
    Column& operator=(const Column&) = delete;
    Column& operator=(Column&&) = delete;

    class Impl;
    const std::unique_ptr<Impl> pimpl;
};

// eof
