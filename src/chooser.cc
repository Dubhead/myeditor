#include <memory>
#include <string>
#include <vector>
#include "global.h"
#include "chooser.h"
#include "filemgr.h"
#include "util.h"

using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using sigc::mem_fun;

//// tree model ////

class ModelColumns: public Gtk::TreeModelColumnRecord
{
public:
    ModelColumns() { add(type); add(name); add(rowNum); }

    Gtk::TreeModelColumn<Glib::ustring> type;
    Gtk::TreeModelColumn<Glib::ustring> name;
    Gtk::TreeModelColumn<unsigned int> rowNum;	// hidden field
};

//// impl class ////

class Chooser::Impl
{
public:
    Impl(Chooser* parent, string* result);
    ~Impl() = default;
    void init(const string& args);
    void buildListStore(const string& pattern);
    bool fileNameMatcher(const string& filename, const string& pattern);
    void getFilesOnFS(const string& directory,
	shared_ptr<vector<string>> dotfiles,
	shared_ptr<vector<string>> regulars,
	shared_ptr<vector<string>> directories);
    void highlightLine(unsigned int lineNum);
    void entryOnActivate();
    void entryBufferOnDeletedText(unsigned int placeholder1,
        unsigned int placeholder2);
    void entryBufferOnInsertedText(unsigned int placeholder1,
        const char* placeholder2, unsigned int placeholder3);
    bool entryOnKeyPress(GdkEventKey*);

    Chooser* chooser;
    string* result;
    Gtk::Grid* grid;
    Gtk::Entry* entry;
    Gtk::ScrolledWindow scrolledWindow;
    string curdir;
    vector<string> recentFiles;
    shared_ptr<vector<string>> dotfiles;
    shared_ptr<vector<string>> regulars;
    shared_ptr<vector<string>> directories;
    ModelColumns modelColumns;
    Glib::RefPtr<Gtk::ListStore> refListStore;
    Gtk::TreeView treeView;
};

Chooser::Impl::Impl(Chooser* parent, string* result_)
    : chooser{parent}, result{result_}
{
}

void Chooser::Impl::init(const string& args)
{
    entry = manage(new Gtk::Entry());
    entry->set_has_frame(false);
    entry->signal_activate().connect(mem_fun(*this,
        &Chooser::Impl::entryOnActivate));
    entry->get_buffer()->signal_inserted_text().connect(mem_fun(*this,
        &Chooser::Impl::entryBufferOnInsertedText));
    entry->get_buffer()->signal_deleted_text().connect(mem_fun(*this,
        &Chooser::Impl::entryBufferOnDeletedText));
    entry->signal_key_press_event().connect(mem_fun(*this,
        &Chooser::Impl::entryOnKeyPress));

    dotfiles = make_shared<vector<string>>();
    regulars = make_shared<vector<string>>();
    directories = make_shared<vector<string>>();
    // curdir = *result;
    curdir = toFullPath(".", *result);
    *result = "";
    getFilesOnFS(curdir, dotfiles, regulars, directories);
    recentFiles = fileMgr->getRecentFiles();

    refListStore = Gtk::ListStore::create(modelColumns);
    treeView.set_model(refListStore);
    treeView.set_can_focus(false);
    treeView.set_headers_visible(false);
    treeView.append_column("", modelColumns.type);
    treeView.append_column("", modelColumns.name);
    buildListStore("");

    scrolledWindow.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    scrolledWindow.set_placement(Gtk::CORNER_TOP_RIGHT);
    scrolledWindow.set_hexpand(true);
    scrolledWindow.set_vexpand(true);
    scrolledWindow.add(treeView);

    grid = manage(new Gtk::Grid());
    grid->set_orientation(Gtk::Orientation::ORIENTATION_VERTICAL);
    grid->set_column_homogeneous(true);
    grid->set_column_spacing(0);
    grid->set_row_homogeneous(false);
    grid->set_row_spacing(0);
    grid->add(*entry);
    grid->add(scrolledWindow);

    chooser->add(*grid);
    chooser->show_all_children();
    chooser->set_title(entilde(curdir));
    entry->grab_focus();

    // Set size.
    Gdk::Geometry geom{400, 400, 0, 0, 0, 0, 0, 0, 0, 0,
	GDK_GRAVITY_NORTH_WEST};
    Gdk::WindowHints masks = Gdk::WindowHints::HINT_MIN_SIZE;
    chooser->set_geometry_hints(*grid, geom, masks);
}

void Chooser::Impl::buildListStore(const string& pattern)
{
    // changing directory?
    bool changingDir = false;
    Glib::RefPtr<Gio::File> newDir;
    if (pattern == "~/") {	// home directory
	changingDir = true;
	newDir = Gio::File::create_for_path(Glib::get_home_dir());
    }
    else if (pattern == "../") {	// parent directory
	changingDir = true;
	newDir = Gio::File::create_for_path(curdir)->get_parent();
    }
    else if (pattern == "/") {	// root directory
	changingDir = true;
	newDir = Gio::File::create_for_path("/");
    }
    else if ((!pattern.empty()) && pattern.rfind('/') == pattern.size() - 1) {
	// pattern ends with '/'.  If this is the only match, chdir there.
	vector<string> matches;
	if (pattern.find(".") == 0)
	    for (const auto& df: *dotfiles) {
		if ((df.rfind('/') == df.size() - 1) &&
			(fileNameMatcher(df, pattern)))
		    matches.push_back(df);
	    }
	for (const auto& f: *directories) {
	    if (fileNameMatcher(f, pattern)) {
		matches.push_back(f);
	    }
	}
	if (matches.size() == 1) {	// only match!
	    changingDir = true;
	    newDir = Gio::File::create_for_path(curdir)->get_child(matches[0]);
	}
    }

    if (changingDir) {
	if (!newDir->query_exists())
	    return;

	recentFiles.clear();
	curdir = newDir->get_path();
	getFilesOnFS(curdir, dotfiles, regulars, directories);
	chooser->set_title(entilde(curdir));
	entry->set_text("");
    }

    // Populate the Gtk::ListStore.
    refListStore->clear();
    unsigned int rowNum = 0;
    for (const auto& rf: recentFiles) {
	if (!changingDir && !fileNameMatcher(rf, pattern))
	    continue;
	auto row = *(refListStore->append());
	row[modelColumns.type] = "recent";
	row[modelColumns.name] = rf;
	row[modelColumns.rowNum] = rowNum;
	++rowNum;
    }
    unsigned int numRecentFiles = rowNum;
    if (!changingDir && (pattern.find('.') == 0)) {
	for (const auto& df: *dotfiles) {
	    if (!changingDir && !fileNameMatcher(df, pattern))
		continue;
	    auto row = *(refListStore->append());
	    row[modelColumns.type] = "dot";
	    row[modelColumns.name] = df;
	    row[modelColumns.rowNum] = rowNum;
	    ++rowNum;
	}
    }
    for (const auto& f: *regulars) {
	if (!changingDir && !fileNameMatcher(f, pattern))
	    continue;
	auto row = *(refListStore->append());
	row[modelColumns.type] = "file";
	row[modelColumns.name] = f;
	row[modelColumns.rowNum] = rowNum;
	++rowNum;
    }
    for (const auto& d: *directories) {
	if (!changingDir && !fileNameMatcher(d, pattern))
	    continue;
	auto row = *(refListStore->append());
	row[modelColumns.type] = "directory";
	row[modelColumns.name] = d;
	row[modelColumns.rowNum] = rowNum;
	++rowNum;
    }

    // Highlight the row for the first regular file.
    // If there is none, highlight the last recent file.
    if (rowNum > 0) {
	unsigned int rowIndexToHighlight = numRecentFiles;
	if (numRecentFiles == rowNum) {
	    --rowIndexToHighlight;
	}
	highlightLine(rowIndexToHighlight);
    }
}

bool Chooser::Impl::fileNameMatcher(const string& filename,
    const string& pattern)
{
    std::string::size_type indexInFilename = 0;
    std::string::size_type indexInPattern = 0;

    while (indexInPattern < pattern.size()) {
	char needle = pattern[indexInPattern];
	std::string::size_type i =
	    filename.find_first_of(needle, indexInFilename);
	if (i == string::npos)
	    return false;
	indexInFilename = i + 1;
	++indexInPattern;
    }

    return true;
}

void Chooser::Impl::getFilesOnFS(const string& directory,
    shared_ptr<vector<string>> dotfiles,
    shared_ptr<vector<string>> regulars,
    shared_ptr<vector<string>> directories)
{
    auto giofile =
        Gio::File::create_for_path(directory)->resolve_relative_path(curdir);
    auto fileEnumerator = giofile->enumerate_children();
    // Glib::object_unref(giofile);	// bug in gtkmm?

    dotfiles->clear();
    regulars->clear();
    directories->clear();
    for (auto f = fileEnumerator->next_file(); f != 0;
	    f = fileEnumerator->next_file()) {
	string filename = f->get_name();
	if (filename.find_first_of('.') == 0) {
	    if (f->get_file_type() == Gio::FileType::FILE_TYPE_DIRECTORY)
		dotfiles->push_back(filename + '/');
	    else dotfiles->push_back(filename);
	}
	else if (f->get_file_type() == Gio::FileType::FILE_TYPE_DIRECTORY)
	    directories->push_back(filename + '/');
	else regulars->push_back(filename);
    }

    std::sort(begin(*dotfiles), end(*dotfiles));
    std::sort(begin(*regulars), end(*regulars));
    std::sort(begin(*directories), end(*directories));
}

void Chooser::Impl::highlightLine(unsigned int lineNum)
{
    auto rowToHighlight = refListStore->children()[lineNum];
    if (!rowToHighlight)
	return;
    treeView.get_selection()->select(rowToHighlight);
    treeView.scroll_to_row(refListStore->get_path(rowToHighlight));
}

//// event handlers ////

void Chooser::Impl::entryOnActivate()
{
    auto pattern = entry->get_text();
    if ((pattern.size() >= 3) && (pattern.find("e ") == 0)) {
	// Filename is specified in the entry.  This may be a new file.
	//auto giofile = Gio::File::create_for_path(curdir)->
	//    resolve_relative_path(pattern.substr(2));
	auto giofile = Gio::File::create_for_path(
	    toFullPath(curdir, pattern.substr(2)));
	string fullpath = giofile->get_path();
	*result = "e " + fullpath;
	chooser->hide();
	return;
    }

    Gtk::TreeModel::iterator iter = treeView.get_selection()->get_selected();
    if (!iter) {
	chooser->hide();
	return;
    }
    Glib::ustring selectedName = (*iter)[modelColumns.name];
    string fullpath = toFullPath(curdir, selectedName);

    // If a directory is selected:
    if (selectedName[selectedName.size() - 1] == '/') {
        curdir = fullpath;
        getFilesOnFS(curdir, dotfiles, regulars, directories);
	recentFiles.clear();
        buildListStore("");
        chooser->set_title(entilde(curdir));
        entry->set_text("");
        return;
    }

    *result = "e " + fullpath;
    chooser->hide();
}

void Chooser::Impl::entryBufferOnDeletedText(unsigned int placeholder1,
    unsigned int placeholder2)
{
    const string& pattern = entry->get_text();
    buildListStore(pattern);
}

void Chooser::Impl::entryBufferOnInsertedText(unsigned int placeholder1,
    const char* placeholder2, unsigned int placeholder3)
{
    const string& pattern = entry->get_text();
    buildListStore(pattern);
}

bool Chooser::Impl::entryOnKeyPress(GdkEventKey* ev)
{
    // no-op for modifier keys (only)
    constexpr auto ALL_MODIFIERS =
        GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK;
    if (ev->is_modifier)
        return true;

    // No-op if nothing is selected.
    Gtk::TreeModel::iterator iter = treeView.get_selection()->get_selected();
    if (!iter)
	return true;

    const unsigned int rowNum = (*iter)[modelColumns.rowNum];

    // Handle Up and Down.
    if ((ev->state & ALL_MODIFIERS) == 0) {
        switch (ev->keyval) {
        case GDK_KEY_Down:
	    highlightLine(rowNum + 1);
	    return true;
        case GDK_KEY_Up:
	    highlightLine((rowNum == 0) ? 0 : rowNum - 1);
	    return true;
        default:
            return false;
        }
    }

    // Handle ctrl-something.
    if ((ev->state & ALL_MODIFIERS) == GDK_CONTROL_MASK) {
        switch (ev->keyval) {
        case GDK_KEY_g:	// Quit the dialog.
	    chooser->hide();
            return true;
        case GDK_KEY_n:	// next line
	    highlightLine(rowNum + 1);
	    return true;
        case GDK_KEY_p:	// prev line
	    highlightLine((rowNum == 0) ? 0 : rowNum - 1);
	    return true;
        default:
            return false;
        }
    }

    return false;
}

//// interface class ////

Chooser::Chooser(string* result) : pimpl{new Impl{this, result}} {}
Chooser::~Chooser() = default;
void Chooser::init(const string& args) { pimpl->init(args); }

// eof
