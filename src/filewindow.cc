#include "command.h"
#include "global.h"
#include "filewindow.h"
#include "windowmgr.h"

using std::shared_ptr;
using std::string;
using sigc::mem_fun;

//// impl class ////

class FileWindow::Impl
{
public:
    Impl(FileWindow* parent);
    ~Impl() = default;
    void init();
    shared_ptr<File> getFile();
    void save(const string& altFilename);
    void setBuffer(const GsvBuffer& buf);
    void setFile(shared_ptr<File> f);
    const string shortDesc();
    void bufferOnInsert(const Gtk::TextBuffer::iterator& pos,
	const Glib::ustring& text, int bytes);

    FileWindow* fw;
    shared_ptr<File> file;
};

FileWindow::Impl::Impl(FileWindow* parent) : fw{parent}, file{nullptr} {}

void FileWindow::Impl::init() {}

shared_ptr<File> FileWindow::Impl::getFile()
{
    return file;
}

void FileWindow::Impl::save(const string& altFilename)
{
    // TODO: Use altFilename.

    auto text = fw->getBuffer()->get_text();
    file->save(text);
}

void FileWindow::Impl::setBuffer(const GsvBuffer& buf)
{
    fw->EditWindow::setBuffer(buf);
}

void FileWindow::Impl::setFile(shared_ptr<File> f)
{
    file = f;
    auto buf = f->getNewBuffer();
    fw->setBuffer(buf);

    // Set label for filepath.
    string path = f->getGioFile()->get_path();
    const string home = Glib::get_home_dir();
    if (path.find(home) == 0) {	// 'path' starts with 'home'.
	path = "~" + path.substr(home.size());
    }
    fw->setLabelText(shortDesc());

    // Set syntax.
    auto lm = Gsv::LanguageManager::get_default();
    auto lang = lm->guess_language(path, Glib::ustring());
    buf->set_language(lang);

    // Color the colorbox according to f's parent directory.
    std::hash<string> h1;
    auto hashValue = h1(f->getGioFile()->get_parent()->get_path());
    auto rgba = *(new Gdk::RGBA("black"));
    rgba.set_red_u((hashValue & 0xFFFF00000000) >> 32);
    rgba.set_green_u((hashValue & 0xFFFF0000) >> 16);
    rgba.set_blue_u((hashValue & 0xFFFF));
    fw->getColorBox()->override_background_color(rgba);

    buf->signal_insert().connect(mem_fun(*this,
	&FileWindow::Impl::bufferOnInsert));
}

const string FileWindow::Impl::shortDesc()
{
    auto gf = file->getGioFile();
    string directory = gf->get_parent()->get_path();
    const string home = Glib::get_home_dir();
    if (directory.find(home) == 0) {
	directory = "~" + directory.substr(home.size());
    }
    return gf->get_basename() + " (" + directory + ")";
}

//// event handlers ////

void FileWindow::Impl::bufferOnInsert(const Gtk::TextBuffer::iterator& pos,
    const Glib::ustring& text, int bytes)
{
    // string msg{"FileWindow::Impl::bufferOnInsert: "};
    // msg += text;
    // commandMgr->log(msg);
}

//// interface class ////

FileWindow::FileWindow() : EditWindow(), pimpl{new Impl{this}} {}
FileWindow::~FileWindow() = default;
void FileWindow::init() { pimpl->init(); }
shared_ptr<File> FileWindow::getFile() { return pimpl->getFile(); }
void FileWindow::save(const string& altFilename) { pimpl->save(altFilename); }
void FileWindow::setBuffer(const GsvBuffer& buf) { pimpl->setBuffer(buf); }
void FileWindow::setFile(shared_ptr<File> file) { pimpl->setFile(file); }
const string FileWindow::shortDesc() { return pimpl->shortDesc(); }

// eof
