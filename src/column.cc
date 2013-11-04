#include <memory>
#include <string>
#include <vector>
#include <boost/optional.hpp>
#include "global.h"
#include "column.h"
#include "editwindow.h"
#include "filewindow.h"
#include "scratchwindow.h"
#include "windowmgr.h"

using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;
using boost::optional;

//// impl class ////

class Column::Impl
{
public:
    Impl(Column* parent);
    ~Impl() = default;
    void init();
    void addWindow(EditWindow& ew, EditWindow& sibling);
    void appendWindow(const EditWindow&);
    optional<unsigned int> closeWindow(EditWindow*);
    unsigned int closeWindowsForFile(shared_ptr<File> f);
    optional<EditWindow*> getEditWindow(const unsigned int rowNum);
    optional<FileWindow*> getFileWindow(const string& path);
    optional<unsigned int> getRow(const EditWindow*);
    optional<ScratchWindow*> getScratchWindow();
    void replaceWindow(EditWindow* oldEW, EditWindow* newEW);

    Column* c;
    unique_ptr<vector<EditWindow*>> editWindows;
};

Column::Impl::Impl(Column* parent)
    : c{parent}, editWindows{new vector<EditWindow*>()}
{
}

void Column::Impl::init()
{
    c->set_orientation(Gtk::Orientation::ORIENTATION_VERTICAL);
    c->set_row_spacing(0);
    c->set_row_homogeneous(false);

    auto label = manage(new Gtk::Label("first label for Column"));
    c->add(*label);
}

void Column::Impl::addWindow(EditWindow& ew, EditWindow& sibling)
{
    // Update editWindows.
    vector<EditWindow*>::iterator it;
    for (it = begin(*editWindows); it != end(*editWindows); ++it) {
	if (*it == &sibling)
	    break;
    }
    if (*it != &sibling)
	return;
    ++it;
    editWindows->insert(it, &ew);

    // Remove (if necessary) and re-add every EditWindow in editWindows
    // to the Grid.
    for (unsigned int rowNumber = 0; rowNumber < editWindows->size();
	    ++rowNumber) {
	auto ew2 = editWindows->at(rowNumber);
	if (ew2 != &ew)
	    c->remove(*ew2);
	c->attach(*ew2, 0, rowNumber + 1, 1, 1);
    }
}

void Column::Impl::appendWindow(const EditWindow& ew)
{
    // Update editWindows.
    EditWindow& ew2 = const_cast<EditWindow&>(ew);
    editWindows->push_back(&ew2);

    ew2.grabFocus();
    windowMgr->setFrontEditWindow(&ew2);
    c->add(ew2);
}

// Delete the edit window from this column.
// Return the row number of the window if deleted; none if not.
// Note: Caller should do 'delete ew;'.  cf. Gtk::Container::remove
optional<unsigned int> Column::Impl::closeWindow(EditWindow* ew)
{
    // Delete ew from editWindows.
    optional<unsigned int> idx = getRow(ew);
    if (!idx)
	return idx;	// ew not found;
    editWindows->erase(begin(*editWindows) + *idx);

    c->remove(*ew);

    // Locate the EditWindow that should get the focus now.
    EditWindow* ewToBeFocused;
    auto size = editWindows->size();
    if (size == 0) {
	// Since we have deleted the last EditWindow in this column,
	// we add a new scratch window for the focus.
	auto scratch = manage(new ScratchWindow());
	scratch->init();
	editWindows->push_back(scratch);
	c->add(*scratch);

	ewToBeFocused = scratch;
    } else if (*idx < size) {
	ewToBeFocused = editWindows->at(*idx);
    } else {
	ewToBeFocused = editWindows->at(size - 1);
    }

    ewToBeFocused->grabFocus();
    windowMgr->setFrontEditWindow(ewToBeFocused);
    return idx;
}

// Delete all the file windows for File from this column.
// Also do 'delete ew;' for each deleted window; cf. Gtk::Container::remove
// Return the number of windows deleted.
unsigned int Column::Impl::closeWindowsForFile(shared_ptr<File> f)
{
    unsigned int result = 0;
    vector<Gtk::Widget*> vec = c->get_children();
    unsigned int rowNum = 0;	// 0-based, NOT including the column header
    while (rowNum < editWindows->size()) {	// for each edit window...
	// Check if we need to delete this edit window.
	EditWindow* ew = editWindows->at(rowNum);
	if (typeid(*ew) != typeid(FileWindow)) {
	    ++rowNum;
	    continue;
	}
	auto fw = reinterpret_cast<FileWindow*>(ew);
	if (fw->getFile() != f) {
	    ++rowNum;
	    continue;
	}

	closeWindow(fw);
	windowMgr->deleteFromHistory(fw);
	delete fw;
	++result;
	// Here we do NOT increment rowNum.
    }

    return result;
}

optional<EditWindow*> Column::Impl::getEditWindow(const unsigned int rowNum)
{
    if (rowNum >= editWindows->size())
	return optional<EditWindow*>();
    return optional<EditWindow*>(editWindows->at(rowNum));
}

optional<FileWindow*> Column::Impl::getFileWindow(const string& path)
{
    for (auto iter: *editWindows) {
        if (typeid(*iter) != typeid(FileWindow))
            continue;

	FileWindow* fw = reinterpret_cast<FileWindow*>(iter);
	if (fw->getFile()->getGioFile()->get_path() == path)
	    return optional<FileWindow*>(fw);
    }
    return optional<FileWindow*>();
}

// @return	row index of ew, 0-based; none if not found
optional<unsigned int> Column::Impl::getRow(const EditWindow* ew)
{
    vector<EditWindow*>::size_type idx;
    for (idx = 0; idx < editWindows->size(); ++idx) {
	if ((*editWindows)[idx] == ew)
	    return optional<unsigned int>(idx);
    }
    return optional<unsigned int>();	// not found
}

// @return	the first ScratchWindow in this column; none if not found
optional<ScratchWindow*> Column::Impl::getScratchWindow()
{
    for (auto ew: *editWindows) {
	if (typeid(*ew) == typeid(ScratchWindow)) {
	    auto sw = reinterpret_cast<ScratchWindow*>(ew);
	    return optional<ScratchWindow*>(sw);
	}
    }

    return optional<ScratchWindow*>();	// not found
}

// Note: Caller should do 'delete oldEW;'.  cf. Gtk::Container::remove
void Column::Impl::replaceWindow(EditWindow* oldEW, EditWindow* newEW)
{
    // Update editWindows.
    optional<unsigned int> idx = getRow(oldEW);
    if (!idx)
	return;	// oldEW not found; XXX
    editWindows->at(*idx) = newEW;

    c->remove(*oldEW);
    c->attach(*newEW, 0, *idx + 1, 1, 1);	// +1 for the column header
    newEW->grabFocus();
    windowMgr->setFrontEditWindow(newEW);
}

//// interface class ////

Column::Column() : Gtk::Grid(), pimpl{new Impl{this}} {}
Column::~Column() = default;
void Column::init() { pimpl->init(); }
void Column::addWindow(EditWindow& ew, EditWindow& sibling) {
    pimpl->addWindow(ew, sibling);
}
void Column::appendWindow(const EditWindow& ew) { pimpl->appendWindow(ew); }
optional<unsigned int> Column::closeWindow(EditWindow* ew) {
    return pimpl->closeWindow(ew);
}
unsigned int Column::closeWindowsForFile(shared_ptr<File> f) {
    return pimpl->closeWindowsForFile(f);
}
optional<EditWindow*> Column::getEditWindow(const unsigned int rowNum) {
    return pimpl->getEditWindow(rowNum);
}
optional<FileWindow*> Column::getFileWindow(const string& path) {
    return pimpl->getFileWindow(path);
}
optional<unsigned int> Column::getRow(const EditWindow* ew) {
    return pimpl->getRow(ew);
}
optional<ScratchWindow*> Column::getScratchWindow() {
    return pimpl->getScratchWindow();
}
void Column::replaceWindow(EditWindow* oldEW, EditWindow* newEW) {
    pimpl->replaceWindow(oldEW, newEW);
}

// eof
