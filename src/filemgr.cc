#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <boost/optional.hpp>
#include "command.h"
#include "file.h"
#include "filemgr.h"
#include "util.h"

using std::make_shared;
using std::map;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;
using boost::optional;

//// impl class ////

class FileMgr::Impl
{
public:
    Impl(FileMgr* parent);
    ~Impl() = default;
    void init();
    void cleanup();
    optional<shared_ptr<File>> getFile(const string& path,
	const bool supressErrorMsg=false);
    vector<string> getFileNames();
    vector<string> getRecentFiles();
    void deleteFile(shared_ptr<File> f);

    FileMgr* fm;
    map<string, shared_ptr<File>> files;	// use '~' for HOME
    vector<string>* recentFiles;	// old to new; use '~' for HOME
};

FileMgr::Impl::Impl(FileMgr* parent)
    : fm{parent}, files{}, recentFiles{new vector<string>()} {}

void FileMgr::Impl::init()
{
    // Read recent files.
    std::ifstream ifs{Glib::get_home_dir() + "/.myeditor/recents"};
    if (ifs) {
	string line;
	while (ifs >> line)
	    recentFiles->push_back(line);
    }
}

void FileMgr::Impl::cleanup()
{
    // Flash files.  TODO

    // Record the recent files.
    std::ofstream ofs{Glib::get_home_dir() + "/.myeditor/recents"};
    if (ofs) {
	for (string& recentFile: *recentFiles)
	    ofs << entilde(recentFile) << '\n';
    }
}

void FileMgr::Impl::deleteFile(shared_ptr<File> f)
{
    for (auto iter = begin(files); iter != end(files); ++iter) {
	if (iter->second == f) {
	    files.erase(iter);
	    return;
	}
    }
}

optional<shared_ptr<File>> FileMgr::Impl::getFile(const string& path,
    const bool supressErrorMsg)
{
    string fullpath = toFullPath(".", path);
    string tildedPath = entilde(fullpath);

    // Delete path from recentFiles.  TODO How many to keep?
    recentFiles->erase(
	std::remove(begin(*recentFiles), end(*recentFiles), tildedPath),
	end(*recentFiles));
    while (recentFiles->size() >= 4)
	recentFiles->erase(begin(*recentFiles));

    auto iter2 = files.find(tildedPath);
    if (iter2 != end(files)) {
	recentFiles->push_back(tildedPath);
	return optional<shared_ptr<File>>(iter2->second);
    }

    auto f = make_shared<File>(tildedPath);
    optional<string> errmsg = f->init();
    if (errmsg) {
	if (!supressErrorMsg)
	    commandMgr->log(*errmsg);
	return optional<shared_ptr<File>>();
    }
    files[tildedPath] = f;
    recentFiles->push_back(tildedPath);
    return optional<shared_ptr<File>>(f);
}

// Return the list of file names that are already loaded in the editor.
vector<string> FileMgr::Impl::getFileNames()
{
    vector<string> result{};
    for (const auto& f: files) {
	result.push_back(f.first);
    }
    std::sort(begin(result), end(result));
    return result;
}

vector<string> FileMgr::Impl::getRecentFiles()
{
    return *recentFiles;
}

//// interface class ////

FileMgr::FileMgr() : pimpl{new Impl{this}} {}
FileMgr::~FileMgr() = default;
void FileMgr::init() { pimpl->init(); }
void FileMgr::cleanup() { pimpl->cleanup(); }
void FileMgr::deleteFile(shared_ptr<File> f) { pimpl->deleteFile(f); }
optional<shared_ptr<File>> FileMgr::getFile(const string& path,
	const bool supressErrorMsg) {
    return pimpl->getFile(path, supressErrorMsg);
}
vector<string> FileMgr::getFileNames() { return pimpl->getFileNames(); }
vector<string> FileMgr::getRecentFiles() { return pimpl->getRecentFiles(); }

// eof
