#include <string>
#include "global.h"
#include "filewindow.h"
#include "scratchwindow.h"

using std::string;

//// scratch buffer singleton ////

class ScratchBufferSingleton {
public:
    static GsvBuffer getBuffer() {
	static ScratchBufferSingleton self;
	if (!self.buffer) {
	    self.buffer = Gsv::Buffer::create();
	    self.buffer->set_text("This is a scratch buffer.\n");
	}
	return self.buffer;
    }
private:
    ScratchBufferSingleton() {}
    GsvBuffer buffer;
};

//// impl class ////

class ScratchWindow::Impl
{
public:
    Impl(ScratchWindow* parent);
    ~Impl() = default;
    void init();
    void appendText(const string& text);
    const string shortDesc();

    ScratchWindow* sw;
};

ScratchWindow::Impl::Impl(ScratchWindow* parent) : sw{parent} {}

void ScratchWindow::Impl::init()
{
    sw->setBuffer(ScratchBufferSingleton::getBuffer());
    sw->setLabelText(shortDesc());
}

void ScratchWindow::Impl::appendText(const string& text)
{
    auto buf = sw->getBuffer();
    auto iter = buf->get_iter_at_offset(buf->get_char_count());
    buf->insert(iter, text);

    // Scroll to the last line.
    iter = buf->get_iter_at_offset(buf->get_char_count());
    sw->getView().scroll_to(iter);
}

const string ScratchWindow::Impl::shortDesc()
{
    return "*scratch*";
}

//// interface class ////

ScratchWindow::ScratchWindow() : EditWindow(), pimpl{new Impl{this}} {}
ScratchWindow::~ScratchWindow() = default;
void ScratchWindow::init() { pimpl->init(); }
void ScratchWindow::appendText(const string& text) { pimpl->appendText(text); }
const string ScratchWindow::shortDesc() { return pimpl->shortDesc(); }

// eof
