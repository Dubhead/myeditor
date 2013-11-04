#include <deque>
#include <memory>
#include <vector>
#include <boost/optional.hpp>
#include "command.h"
#include "global.h"
#include "column.h"
#include "editwindow.h"
#include "filemgr.h"
#include "filewindow.h"
#include "scratchwindow.h"
#include "windowmgr.h"

using std::deque;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;
using sigc::mem_fun;

//// impl class ////

class WindowMgr::Impl
{
public:
    Impl(WindowMgr* parent);
    ~Impl() = default;
    void init();
    void addWindow(const EditWindow&);
    void bubble();
    bool closeWindow(EditWindow*);
    void closeWindowsForFile(shared_ptr<File> f);
    void deleteFromHistory(EditWindow* ew);
    void focusMinibuffer();
    optional<Column&> getColumn(unsigned int colNum);
    Column& getCurrentColumn();
    EditWindow* getCurrentFocus();
    optional<EditWindow*> getEditWindow(const EditWindowPos& pos);
    optional<EditWindowPos> getEditWindowPosition(const EditWindow* ew);
    optional<FileWindow*> getFileWindow(const std::string& path,
	bool createIfNecessary=false);
    unsigned int getNumColumns();
    optional<ScratchWindow*> getScratchWindow(bool createIfNecessary=false);
    void moveWindow(EditWindow* ew, EditWindow* sibling);
    EditWindow* newColumn(optional<EditWindow*> opt_ew);
    void replaceWindow(EditWindow* oldEW, EditWindow* newEW);
    void setEntryPlaceholderText(const string& msg);
    void setFrontEditWindow(EditWindow* ew);
    void splitWindow(EditWindow& ew);

    void entryOnActivate();
    void entryOnFocus();
    bool entryOnKeyPress(GdkEventKey* ev);
    bool windowOnDelete(GdkEventAny* ev);

    WindowMgr* wm;
    Gtk::Grid grid;
    Gtk::Entry entry;
    Gtk::Grid columns;
    deque<EditWindow*>* editWindowHistory;
    EditWindowPos bubblePos;	// position where the bubbling started
};

WindowMgr::Impl::Impl(WindowMgr* parent)
  : wm{parent}, editWindowHistory{new deque<EditWindow*>()},
    bubblePos{EditWindowPos(0, 0)}
{
}

void WindowMgr::Impl::init()
{
    auto scratch = manage(new ScratchWindow());
    scratch->init();
    setFrontEditWindow(scratch);

    auto defaultColumn = manage(new Column());
    defaultColumn->init();
    defaultColumn->appendWindow(*scratch);

    entry.set_has_frame(false);
    setEntryPlaceholderText("*** minibuffer ***");
    entry.signal_activate().connect(mem_fun(*this,
	&WindowMgr::Impl::entryOnActivate));
    entry.signal_grab_focus().connect(mem_fun(*this,
	&WindowMgr::Impl::entryOnFocus));
    entry.signal_key_press_event().connect(mem_fun(*this,
	&WindowMgr::Impl::entryOnKeyPress));

    columns.set_orientation(Gtk::Orientation::ORIENTATION_HORIZONTAL);
    columns.set_column_homogeneous(true);
    columns.set_column_spacing(0);
    columns.set_row_homogeneous(true);
    columns.set_row_spacing(0);
    columns.add(*defaultColumn);

    grid.set_orientation(Gtk::Orientation::ORIENTATION_VERTICAL);
    grid.set_column_homogeneous(true);
    grid.set_column_spacing(0);
    grid.set_row_homogeneous(false);
    grid.set_row_spacing(0);
    grid.attach(entry, 0, 0, 1, 1);
    grid.attach(columns, 0, 1, 1, 1);

    wm->add(grid);
    wm->set_border_width(0);
    wm->signal_delete_event().connect(mem_fun(*this,
	&WindowMgr::Impl::windowOnDelete));
    wm->show_all_children();

    scratch->grabFocus();

    // Set size.
    Gdk::Geometry geom{400, 200, 0, 0, 0, 0, 0, 0, 0, 0,
	GDK_GRAVITY_NORTH_WEST};
    Gdk::WindowHints masks = Gdk::WindowHints::HINT_MIN_SIZE;
    wm->set_geometry_hints(grid, geom, masks);
}

void WindowMgr::Impl::addWindow(const EditWindow& ew)
{
    getCurrentColumn().appendWindow(ew);
    wm->show_all_children();
}

void WindowMgr::Impl::bubble()
{
    windowMgr->setEntryPlaceholderText("");
    if (editWindowHistory->size() < 2)
	return;

    // How many times 'bubble' is called in succession?
    EditWindow* currentEW = editWindowHistory->front();
    unsigned int times = currentEW->getBubbleNumber() + 1;

    if (times == 1)
	bubblePos = *(getEditWindowPosition(currentEW));

    // Update editWindowHistory and get the next EditWindow to put to front.
    editWindowHistory->pop_front();
    EditWindow* nextEW;
    if (times <= editWindowHistory->size()) {
        // Get editWindowHistory back to square one.
        auto iter = begin(*editWindowHistory);
        for (unsigned int i = 1; i < times; ++i)
            ++iter;
        editWindowHistory->insert(iter, currentEW);

        nextEW = editWindowHistory->at(times);
        iter = begin(*editWindowHistory);
        for (unsigned int i = 0; i < times; ++i)
            ++iter;
        editWindowHistory->erase(iter);
        editWindowHistory->push_front(nextEW);
    }
    else {	// All EditWindow's are bubbled; let's start over.
	commandMgr->log("bubbling: starting over");
	times = 0;
        editWindowHistory->push_back(currentEW);
	nextEW = editWindowHistory->front();
    }

    // Put nextEW to front.
    auto pos = getEditWindowPosition(nextEW);
    if (pos) {
	if (bubblePos == getEditWindowPosition(currentEW)) {
	    // Restore the EditWindow that was originally at the
	    // bubbling position.
	    EditWindow* originalEW = editWindowHistory->at(1);
	    replaceWindow(currentEW, originalEW);
	}
    } else {
	EditWindow* ewToBeReplaced = *(getEditWindow(bubblePos));
	replaceWindow(ewToBeReplaced, nextEW);
    }
    nextEW->grabFocus();
    setFrontEditWindow(nextEW);

    // Update lastOp for both EditWindows.
    currentEW->setBubbleNumber(0);
    nextEW->setBubbleNumber(times);
}

bool WindowMgr::Impl::closeWindow(EditWindow* ew)
{
    optional<unsigned int> rowNum;
    for (unsigned int colIndex = 0; colIndex < getNumColumns(); ++colIndex) {
	Column& c = *getColumn(colIndex);
	rowNum = c.closeWindow(ew);
	if (rowNum)
	    break;
    }
    if (!rowNum)
	return false;	// not deleted: ew not found in any column

    // Delete ew from editWindowHistory.
    editWindowHistory->erase(
	std::remove(begin(*editWindowHistory), end(*editWindowHistory), ew),
	end(*editWindowHistory));

    // Rectify bubblePos.
    if (std::get<1>(bubblePos) > *rowNum) {
	bubblePos = EditWindowPos(
	    std::get<0>(bubblePos), std::get<1>(bubblePos) - 1);
    }

    wm->show_all_children();
    return true;
}

void WindowMgr::Impl::closeWindowsForFile(shared_ptr<File> f)
{
    for (unsigned int colIndex = 0; colIndex < getNumColumns(); ++colIndex) {
	Column& c = *getColumn(colIndex);
	c.closeWindowsForFile(f);
    }
    wm->show_all_children();
}

void WindowMgr::Impl::deleteFromHistory(EditWindow* ew)
{
    editWindowHistory->erase(
	std::remove(begin(*editWindowHistory), end(*editWindowHistory), ew),
	end(*editWindowHistory));
}

void WindowMgr::Impl::focusMinibuffer()
{
    entry.grab_focus();
}

optional<Column&> WindowMgr::Impl::getColumn(unsigned int colNum)
{
    vector<Gtk::Widget*> vec = columns.get_children();
    if (colNum >= vec.size()) {
	return optional<Column&>();
    }
    Gtk::Widget* w = vec[colNum];
    Column* c = reinterpret_cast<Column*>(w);
    return optional<Column&>(*c);
}

Column& WindowMgr::Impl::getCurrentColumn()
{
    const EditWindow* currentEW = getCurrentFocus();
    for (unsigned int colIndex = 0; colIndex < getNumColumns(); ++colIndex) {
	Column& c = *getColumn(colIndex);
	if (c.getRow(currentEW))
	    return c;
    }

    // currentEW not found in any column, which should be impossible.
    // Just in case...
    return *getColumn(0);
}

EditWindow* WindowMgr::Impl::getCurrentFocus()
{
    return editWindowHistory->at(0);
}

optional<EditWindow*> WindowMgr::Impl::getEditWindow(const EditWindowPos& pos)
{
    const unsigned int needleCol = std::get<0>(pos);
    const unsigned int needleRow = std::get<1>(pos);

    optional<Column&> c = getColumn(needleCol);
    if (!c)
	return optional<EditWindow*>();

    return c->getEditWindow(needleRow);
}

optional<EditWindowPos> WindowMgr::Impl::getEditWindowPosition(
    const EditWindow* ew)
{
    for (unsigned int colIndex = 0; colIndex < getNumColumns(); ++colIndex) {
	Column& c = *getColumn(colIndex);
	auto opt_rowNum = c.getRow(ew);
	if (opt_rowNum) {
	    // ew is found in this column!
	    return optional<EditWindowPos>(
	        EditWindowPos(colIndex, *opt_rowNum));
	}
    }

    // not found
    return optional<EditWindowPos>();
}

optional<FileWindow*> WindowMgr::Impl::getFileWindow(const string& path,
    bool createIfNecessary)
{
    // If any column contains a FileWindow for 'path', return it.
    for (unsigned int colIndex = 0; colIndex < getNumColumns(); ++colIndex) {
	Column& c = *getColumn(colIndex);
	auto opt_fw = c.getFileWindow(path);
	if (opt_fw)
	    return opt_fw;
    }

    if (!createIfNecessary)
	return optional<FileWindow*>();

    // Create a new EditWindow and return it.
    FileWindow* new_fw = manage(new FileWindow());
    new_fw->init();
    optional<shared_ptr<File>> f = fileMgr->getFile(path);
    if (!f)
	return optional<FileWindow*>();	// error: file cannot be read
    new_fw->setFile(*f);
    replaceWindow(getCurrentFocus(), new_fw);
    return optional<FileWindow*>(new_fw);
}

unsigned int WindowMgr::Impl::getNumColumns()
{
    return columns.get_children().size();
}

optional<ScratchWindow*> WindowMgr::Impl::getScratchWindow(
    bool createIfNecessary)
{
    // If any column contains a ScratchWindow, return it.
    for (unsigned int colIndex = 0; colIndex < getNumColumns(); ++colIndex) {
	Column& c = *getColumn(colIndex);
	auto opt_sw = c.getScratchWindow();
	if (opt_sw)
	    return opt_sw;
    }

    if (!createIfNecessary)
	return optional<ScratchWindow*>();

    // Create a new ScratchWindow and return it.
    ScratchWindow* new_sw = manage(new ScratchWindow());
    new_sw->init();
    addWindow(*new_sw);
    return optional<ScratchWindow*>(new_sw);
}

// Move ew below sibling.
void WindowMgr::Impl::moveWindow(EditWindow* ew, EditWindow* sibling)
{
    if (ew == sibling)
	return;

    // Which column is sibling in?
    auto destPos = getEditWindowPosition(sibling);
    unsigned int destColIndex = std::get<0>(*destPos);
    Column& c = *getColumn(destColIndex);

    closeWindow(ew);
    c.addWindow(*ew, *sibling);
    setFrontEditWindow(ew);
}

// Create a new column.
// This will contain opt_ew (or newly-created ScratchWindow).
// Return the EditWindow in this newly-added column.
EditWindow* WindowMgr::Impl::newColumn(optional<EditWindow*> opt_ew)
{
    auto newCol = manage(new Column());
    newCol->init();

    // EditWindow that will be contained in the new column.
    EditWindow* ew;
    if (opt_ew)
	ew = *opt_ew;
    else {
	auto sw = manage(new ScratchWindow());
	sw->init();
	ew = sw;
    }

    newCol->appendWindow(*ew);
    columns.add(*newCol);
    wm->show_all_children();
    return ew;
}

void WindowMgr::Impl::replaceWindow(EditWindow* oldEW, EditWindow* newEW)
{
    getCurrentColumn().replaceWindow(oldEW, newEW);
    wm->show_all_children();
}

void WindowMgr::Impl::setEntryPlaceholderText(const string& msg)
{
    entry.set_placeholder_text(msg);
}

// Update editWindowHistory and window's title.
// Note: This doesn't do grab_focus() for the EditWindow.
void WindowMgr::Impl::setFrontEditWindow(EditWindow* ew)
{
    // Delete ew and any ScratchWindow from editWindowHistory.
    editWindowHistory->erase(
	std::remove_if(begin(*editWindowHistory), end(*editWindowHistory),
	    [&ew](EditWindow* elem) {
		return (elem == ew) ||
		    (typeid(*elem) == typeid(ScratchWindow));
	    }),
	end(*editWindowHistory));

    editWindowHistory->push_front(ew);
    wm->set_title(ew->shortDesc() + " - myeditor");
}

void WindowMgr::Impl::splitWindow(EditWindow& ew)
{
    if (!getEditWindowPosition(&ew))	// ew is not on screen.
	return;
    if (typeid(ew) == typeid(ScratchWindow))
	return;	// cannot split a ScratchWindow

    FileWindow* newWindow = manage(new FileWindow());
    newWindow->init();
    newWindow->setFile(dynamic_cast<FileWindow&>(ew).getFile());

    getCurrentColumn().addWindow(*newWindow, ew);
    wm->show_all_children();
    ew.grabFocus();
    setFrontEditWindow(newWindow);
    setFrontEditWindow(&ew);

    // XXX We want to make sure ew's cursor is on screen,
    // but neither of these works.  Perhaps a gtkmm bug?
    // ew.getView().place_cursor_onscreen();
    // ew.getView().scroll_to(ew.getBuffer()->get_insert());
}

//// event handlers ////

void WindowMgr::Impl::entryOnActivate()
{
    auto result = commandMgr->execute(entry.get_text());
    entry.set_text("");
    entry.set_placeholder_text(std::get<1>(result));
    EditWindow* ew = getCurrentFocus();
    setFrontEditWindow(ew);
    ew->grabFocus();
}

void WindowMgr::Impl::entryOnFocus()
{
    entry.set_placeholder_text("");
}

bool WindowMgr::Impl::entryOnKeyPress(GdkEventKey* ev)
{
    // no-op for modifier keys (only)
    constexpr auto ALL_MODIFIERS =
	GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK;
    if (ev->is_modifier)
	return true;

    // Handle ctrl-something.
    if ((ev->state & ALL_MODIFIERS) == GDK_CONTROL_MASK) {
        switch (ev->keyval) {
        case GDK_KEY_g:	// Cancel; return focus to EditWindow.
            windowMgr->getCurrentFocus()->grabFocus();
            return true;
        default:
            return false;
        }
    }

    return false;
}

bool WindowMgr::Impl::windowOnDelete(GdkEventAny* ev)
{
    fileMgr->cleanup();
    return false;
}

//// interface class ////

WindowMgr::WindowMgr() : pimpl{new Impl{this}} {}
WindowMgr::~WindowMgr() = default;
void WindowMgr::init() { pimpl->init(); }
void WindowMgr::addWindow(const EditWindow& ew) { pimpl->addWindow(ew); }
void WindowMgr::bubble() { pimpl->bubble(); }
bool WindowMgr::closeWindow(EditWindow* ew) { return pimpl->closeWindow(ew); }
void WindowMgr::closeWindowsForFile(shared_ptr<File> f) {
    pimpl->closeWindowsForFile(f);
}
void WindowMgr::deleteFromHistory(EditWindow* ew) {
    pimpl->deleteFromHistory(ew);
}
void WindowMgr::focusMinibuffer() { pimpl->focusMinibuffer(); }
Column& WindowMgr::getCurrentColumn() { return pimpl->getCurrentColumn(); }
EditWindow* WindowMgr::getCurrentFocus() { return pimpl->getCurrentFocus(); }
optional<EditWindowPos> WindowMgr::getEditWindowPosition(
	const EditWindow* ew) {
    return pimpl->getEditWindowPosition(ew);
}

optional<FileWindow*> WindowMgr::getFileWindow(const string& path,
	bool createIfNecessary) {
    return pimpl->getFileWindow(path, createIfNecessary);
}
optional<ScratchWindow*> WindowMgr::getScratchWindow(bool createIfNecessary) {
    return pimpl->getScratchWindow(createIfNecessary);
}
void WindowMgr::moveWindow(EditWindow* ew, EditWindow* sibling) {
    pimpl->moveWindow(ew, sibling);
}
EditWindow* WindowMgr::newColumn(optional<EditWindow*> opt_ew) {
    return pimpl->newColumn(opt_ew);
}
void WindowMgr::replaceWindow(EditWindow* oldEW, EditWindow* newEW) {
    pimpl->replaceWindow(oldEW, newEW);
}
void WindowMgr::splitWindow(EditWindow& ew) { pimpl->splitWindow(ew); }
void WindowMgr::setEntryPlaceholderText(const string& msg) {
    pimpl->setEntryPlaceholderText(msg);
}
void WindowMgr::setFrontEditWindow(EditWindow* ew) {
    pimpl->setFrontEditWindow(ew);
}

// eof
