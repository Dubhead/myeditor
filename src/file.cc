#include <string>
#include <vector>
#include <boost/optional.hpp>
#include "global.h"
#include "file.h"
#include "util.h"

using sigc::mem_fun;
using std::string;
using std::vector;
using std::unique_ptr;
using boost::optional;

//// impl class ////

class File::Impl
{
public:
    Impl(File* parent, const string& path);
    ~Impl() = default;
    optional<string> init();
    void addBuffer(const GsvBuffer& buf);
    void deleteBuffer(const GsvBuffer& buf);
    GioFile getGioFile();
    GsvBuffer getNewBuffer();
    void save(const string& text);

    void bufferOnErase(const Gtk::TextBuffer::iterator start,
	const Gtk::TextBuffer::iterator end);
    void bufferOnInsert(const Gtk::TextBuffer::iterator& pos,
	const Glib::ustring& text, int bytes);

    GioFile giofile;
    bool modifying;	// multi-buffer modification is in progress
    string path;
    unique_ptr<vector<GsvBuffer>> buffers;
};

File::Impl::Impl(File* parent, const string& path_)
    : modifying{false}, path{path_}, buffers{new vector<GsvBuffer>()}
{
}

// Return none on success, error message on failure.
optional<string> File::Impl::init() {
    path = toFullPath(".", path);
    giofile = Gio::File::create_for_path(path);

    string fileContent{""};
    try {
	fileContent = Glib::file_get_contents(path);
    }
    catch (Glib::FileError& e) {
	if (e.code() != Glib::FileError::Code::NO_SUCH_ENTITY)
	    return optional<string>(e.what());	// error
	// Otherwise, that's a new file.  Nothing to do.
    }

    auto buf = Gsv::Buffer::create();
    buf->begin_not_undoable_action();
    buf->set_text(fileContent);
    buf->place_cursor(buf->begin());
    buf->end_not_undoable_action();
    buf->signal_erase().connect(mem_fun(*this,
	&File::Impl::bufferOnErase), false);
    buf->signal_insert().connect(mem_fun(*this,
	&File::Impl::bufferOnInsert), false);

    buffers->push_back(buf);

    return optional<string>();
}

void File::Impl::addBuffer(const GsvBuffer& buf)
{
    buffers->push_back(buf);
}

void File::Impl::deleteBuffer(const GsvBuffer& buf)
{
    buffers->erase(
	std::remove(buffers->begin(), buffers->end(), buf),
	end(*buffers));
}

GioFile File::Impl::getGioFile()
{
    return giofile;
}

GsvBuffer File::Impl::getNewBuffer() {
    string fileContent = (*(buffers->begin()))->get_text();

    auto newBuffer = Gsv::Buffer::create();
    newBuffer->begin_not_undoable_action();
    newBuffer->set_text(fileContent);
    newBuffer->place_cursor(newBuffer->begin());
    newBuffer->end_not_undoable_action();
    newBuffer->signal_erase().connect(mem_fun(*this,
	&File::Impl::bufferOnErase), false);
    newBuffer->signal_insert().connect(mem_fun(*this,
	&File::Impl::bufferOnInsert), false);

    buffers->push_back(newBuffer);
    return newBuffer;
}

void File::Impl::save(const string& text)
{
    string new_etag;
    giofile->replace_contents(text, "", new_etag, nullptr);
}

//// event handlers ////

// Deletes text from other buffers related to this File.
void File::Impl::bufferOnErase(const Gtk::TextBuffer::iterator start,
    const Gtk::TextBuffer::iterator end)
{
    // We don't want other buffers to handle this signal.
    // Note that 'buf->erase(...)' below causes another signal-erase !
    if (modifying)
	return;
    modifying = true;

    GsvBuffer currentBuffer =
	Glib::RefPtr<Gsv::Buffer>::cast_dynamic(start.get_buffer());
    for (auto buf: *buffers) {
	if (buf == currentBuffer)
	    continue;
	auto startIntoBuf = buf->get_iter_at_offset(start.get_offset());
	auto endIntoBuf = buf->get_iter_at_offset(end.get_offset());
	buf->erase(startIntoBuf, endIntoBuf);
    }

    modifying = false;
}

// Insert text to other buffers related to this File.
void File::Impl::bufferOnInsert(const Gtk::TextBuffer::iterator& pos,
    const Glib::ustring& text, int bytes)
{
    // We don't want other buffers to handle this signal.
    // Note that 'buf->insert(...)' below causes another signal-insert !
    if (modifying)
	return;
    modifying = true;

    GsvBuffer currentBuffer =
	Glib::RefPtr<Gsv::Buffer>::cast_dynamic(pos.get_buffer());
    for (auto buf: *buffers) {
	if (buf == currentBuffer)
	    continue;
	auto posIntoBuf = buf->get_iter_at_offset(pos.get_offset());
	buf->insert(posIntoBuf, text);
    }

    modifying = false;
}

//// interface class ////

File::File(const string& path) : pimpl{new Impl{this, path}} {}
File::~File() = default;
optional<string> File::init() { return pimpl->init(); }
void File::addBuffer(const GsvBuffer& buf) { pimpl->addBuffer(buf); }
void File::deleteBuffer(const GsvBuffer& buf) { pimpl->deleteBuffer(buf); }
void File::save(const string& text) { pimpl->save(text); }
GioFile File::getGioFile() { return pimpl->getGioFile(); }
GsvBuffer File::getNewBuffer() { return pimpl->getNewBuffer(); }

// eof
