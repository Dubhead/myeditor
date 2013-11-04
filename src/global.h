// global.h

#pragma once

#include <gtkmm.h>
#include <gtksourceviewmm.h>

typedef Glib::RefPtr<Gsv::Buffer> GsvBuffer;
// using GsvBuffer = Glib::RefPtr<Gsv::Buffer>;

typedef Glib::RefPtr<Gio::File> GioFile;
// using GioFile = Glib::RefPtr<Gio::File>;

enum class ShadeMode: unsigned int {
    Unshaded,
    Shaded,
    Toggle,	// operation
    Query,	// query
};

class Command;
class FileMgr;
class WindowMgr;

extern Command* commandMgr;
extern FileMgr* fileMgr;
extern WindowMgr* windowMgr;

// eof
