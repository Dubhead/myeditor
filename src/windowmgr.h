#pragma once

#include <memory>
#include <string>
#include <boost/optional.hpp>
#include "global.h"
#include "column.h"
#include "scratchwindow.h"

// EditWindow's position on screen: (columnNum, rowNum); both 0-based
typedef std::tuple<unsigned int, unsigned int> EditWindowPos;

class WindowMgr: public Gtk::Window
{
public:
    WindowMgr();
    virtual ~WindowMgr();
    void init();
    void addWindow(const EditWindow& w);
    void bubble();
    bool closeWindow(EditWindow* ew);
    void closeWindowsForFile(std::shared_ptr<File> f);
    void deleteFromHistory(EditWindow* ew);
    void focusMinibuffer();
    Column& getCurrentColumn();
    EditWindow* getCurrentFocus();
    boost::optional<EditWindowPos> getEditWindowPosition(const EditWindow* ew);
    boost::optional<FileWindow*> getFileWindow(const std::string& path,
	bool createIfNecessary=false);
    boost::optional<ScratchWindow*> getScratchWindow(
	bool createIfNecessary=false);
    void moveWindow(EditWindow* ew, EditWindow* sibling);
    EditWindow* newColumn(boost::optional<EditWindow*> opt_ew);
    void replaceWindow(EditWindow* oldEW, EditWindow* newEW);
    void splitWindow(EditWindow& ew);
    void setEntryPlaceholderText(const std::string& msg);
    void setFrontEditWindow(EditWindow* ew);
private:
    WindowMgr(const WindowMgr&) = delete;	// copy ctor
    WindowMgr(WindowMgr&&) = delete;
    WindowMgr& operator=(const WindowMgr&) = delete;
    WindowMgr& operator=(WindowMgr&&) = delete;

    class Impl;
    const std::unique_ptr<Impl> pimpl;
};

// eof
