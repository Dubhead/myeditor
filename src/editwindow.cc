#include <map>
#include <memory>
#include "command.h"
#include "global.h"
#include "editwindow.h"
#include "windowmgr.h"

using std::make_shared;
using std::map;
using std::shared_ptr;
using std::string;
using std::vector;
using sigc::mem_fun;

// last operation
// 'unsigned int' is used for various purposes, such as cursor position and
// number of times that a commands is repeated, etc.
enum class LastOpCode: unsigned int {
    Plain,
    Bubble,
    CaseUpDown,
    CtrlX,
    LongMovement,
    MidHighLow,
    Recenter,
    SetMark,
    WordCaps,
};
typedef std::tuple<LastOpCode, unsigned int> LastOp;

//// impl class ////

class EditWindow::Impl
{
    typedef bool (EditWindow::Impl::*KeyHandler)(GdkEventKey*);
    // using KeyHandler = bool (EditWindow::Impl::*)(GdkEventKey*);

public:
    Impl(EditWindow* parent);
    ~Impl() = default;
    void buildKeyHandlers();
    unsigned int getBubbleNumber();
    GsvBuffer& getBuffer();
    Gtk::EventBox* getColorBox();
    Gsv::View& getView();
    void gotoLine(unsigned int lineNum);
    void grabFocus();
    virtual void save(const string& altFilename="") {}
    void setBubbleNumber(unsigned int num);
    void setBuffer(const GsvBuffer& buf);
    void setLabelText(const string& text);
    ShadeMode shadeMode(const ShadeMode& sm);

    bool kh_bubble(GdkEventKey* ev);
    bool kh_ctrlX(GdkEventKey* ev);
    bool kh_deleteBuffer(GdkEventKey* ev);
    bool kh_deleteWindow(GdkEventKey* ev);
    bool kh_findFile(GdkEventKey* ev);
    bool kh_focusMinibuffer(GdkEventKey* ev);
    bool kh_quit(GdkEventKey* ev);
    bool kh_recenter(GdkEventKey* ev);
    bool kh_scrollDown(GdkEventKey* ev);
    bool kh_scrollUp(GdkEventKey* ev);
    bool kh_scroll_sub(bool forward);
    bool kh_splitWindow(GdkEventKey* ev);
    bool kh_switchBuffer(GdkEventKey* ev);

    bool colorBoxOnButtonPress(GdkEventButton* ev);
    void colorBoxOnDragDataGet(
	const Glib::RefPtr<Gdk::DragContext>&,
	Gtk::SelectionData& selData, guint, guint);
    void viewOnDragDataReceived(
	const Glib::RefPtr<Gdk::DragContext>& context, int x, int y,
	const Gtk::SelectionData& selData, guint info, guint time);
    bool viewOnFocusInOut(GdkEventFocus*);
    bool viewOnKeyPress(GdkEventKey*);
    bool viewOnScroll(GdkEventScroll*);

    EditWindow* ew;
    Gtk::Grid* headline;
    Gtk::EventBox* colorBox;
    Gtk::Label* label;
    Gtk::EventBox* wrapperForLabel;	// for coloring the label background
    Gtk::ScrolledWindow scrolledWindow;
    LastOp lastOp;
    ShadeMode shadeModeStatus;	// should be either Unshaded or Shaded
    Gsv::View view;
    GsvBuffer buffer;
    std::map<guint, KeyHandler> ctrlKeyMap;
    std::map<guint, KeyHandler> ctrlXKeyMap;
    std::map<guint, KeyHandler> ctrlXCtrlKeyMap;
    std::map<guint, KeyHandler> mod1KeyMap;
};

EditWindow::Impl::Impl(EditWindow* parent)
  : ew{parent}, headline{manage(new Gtk::Grid())},
    lastOp{LastOpCode::Plain, 0}, shadeModeStatus{ShadeMode::Unshaded}
{
    auto bgColor = *(new Gdk::RGBA("gray75"));

    // target for drag n' drop
    vector<Gtk::TargetEntry> listTargets;
    listTargets.emplace_back("move-EditWindow",
	Gtk::TargetFlags::TARGET_SAME_APP);

    colorBox = manage(new Gtk::EventBox());
    colorBox->set_size_request(16, 16);
    colorBox->override_background_color(
        *(new Gdk::RGBA("gray75")));
    //colorBox->signal_button_press_event().connect(mem_fun(*this,
    //    &EditWindow::Impl::colorBoxOnButtonPress));
    colorBox->drag_source_set(listTargets,
	Gdk::ModifierType::SHIFT_MASK | Gdk::ModifierType::BUTTON1_MASK,
	Gdk::ACTION_MOVE);
    colorBox->signal_drag_data_get().connect(mem_fun(*this,
	&EditWindow::Impl::colorBoxOnDragDataGet));

    auto spacer1 = manage(new Gtk::EventBox());
    spacer1->set_size_request(1, 1);
    spacer1->override_background_color(bgColor);

    label = manage(new Gtk::Label());

    wrapperForLabel = manage(new Gtk::EventBox());
    wrapperForLabel->override_background_color(bgColor);
    wrapperForLabel->add(*label);

    auto filler1 = manage(new Gtk::EventBox());
    filler1->set_hexpand(true);
    filler1->override_background_color(bgColor);

    headline->set_orientation(Gtk::Orientation::ORIENTATION_HORIZONTAL);
    headline->set_column_spacing(0);
    headline->set_column_homogeneous(false);
    headline->add(*colorBox);
    headline->add(*spacer1);
    headline->add(*wrapperForLabel);
    headline->add(*filler1);

    view.set_wrap_mode(Gtk::WrapMode::WRAP_WORD_CHAR);
    view.set_auto_indent(true);
    view.set_show_line_marks(true);
    //
    // TODO Make fonts customizable.
    auto fontDesc = make_shared<Pango::FontDescription>(
	"liberation\\ mono,inconsolata,monospace regular 8");
    view.override_font(*fontDesc);
    //
    view.signal_focus_in_event().connect(mem_fun(*this,
	&EditWindow::Impl::viewOnFocusInOut));
    view.signal_focus_out_event().connect(mem_fun(*this,
	&EditWindow::Impl::viewOnFocusInOut));
    view.signal_key_press_event().connect(
	mem_fun(*this, &EditWindow::Impl::viewOnKeyPress),
	false);	// false == Our handlers run *before* the default ones.
    view.signal_scroll_event().connect(mem_fun(*this,
	&EditWindow::Impl::viewOnScroll));
    view.drag_dest_set(listTargets, Gtk::DestDefaults::DEST_DEFAULT_MOTION,
	Gdk::ACTION_MOVE);
    view.signal_drag_data_received().connect(mem_fun(*this,
	&EditWindow::Impl::viewOnDragDataReceived));

    scrolledWindow.set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_ALWAYS);
    scrolledWindow.set_placement(Gtk::CORNER_TOP_RIGHT);
    scrolledWindow.set_hexpand(true);
    scrolledWindow.set_vexpand(true);
    scrolledWindow.add(view);

    ew->set_orientation(Gtk::Orientation::ORIENTATION_VERTICAL);
    ew->set_row_spacing(0);
    ew->set_row_homogeneous(false);
    ew->add(*headline);
    ew->add(scrolledWindow);

    buildKeyHandlers();
}

void EditWindow::Impl::buildKeyHandlers()
{
    ctrlKeyMap = {
	{GDK_KEY_0, &EditWindow::Impl::kh_deleteWindow},
	{GDK_KEY_2, &EditWindow::Impl::kh_splitWindow},
	{GDK_KEY_6, &EditWindow::Impl::kh_bubble},
	{GDK_KEY_l, &EditWindow::Impl::kh_recenter},
	{GDK_KEY_v, &EditWindow::Impl::kh_scrollUp},
	{GDK_KEY_x, &EditWindow::Impl::kh_ctrlX},
	{GDK_KEY_z, &EditWindow::Impl::kh_scrollDown},
    };

    ctrlXKeyMap = {
	{GDK_KEY_b, &EditWindow::Impl::kh_switchBuffer},
	{GDK_KEY_k, &EditWindow::Impl::kh_deleteBuffer},
    };

    ctrlXCtrlKeyMap = {	// for 'Ctrl-x ctrl-f' etc.
	{GDK_KEY_c, &EditWindow::Impl::kh_quit},
	{GDK_KEY_f, &EditWindow::Impl::kh_findFile},
    };

    mod1KeyMap = {
	{GDK_KEY_x, &EditWindow::Impl::kh_focusMinibuffer},
    };
}

unsigned int EditWindow::Impl::getBubbleNumber()
{
    if (std::get<0>(lastOp) == LastOpCode::Bubble)
	return std::get<1>(lastOp);
    return 0;
}

GsvBuffer& EditWindow::Impl::getBuffer()
{
    return this->buffer;
}

Gtk::EventBox* EditWindow::Impl::getColorBox()
{
    return this->colorBox;
}

Gsv::View& EditWindow::Impl::getView()
{
    return this->view;
}

void EditWindow::Impl::gotoLine(unsigned int lineNum)
{
    auto iter = getBuffer()->get_iter_at_line_index(lineNum - 1, 0);

    if (false) {	// TODO if transient mode
	// Move 'insert' mark, leave 'selection-bound'.
	getBuffer()->move_mark_by_name("insert", iter);
    }
    else {	// Move both 'insert' and 'selection-bound'.
	getBuffer()->place_cursor(iter);
    }

    getView().scroll_to(iter);
}

void EditWindow::Impl::grabFocus()
{
    shadeMode(ShadeMode::Unshaded);
    view.grab_focus();
}

void EditWindow::Impl::setBubbleNumber(unsigned int num)
{
    if (num == 0)
	lastOp = LastOp{LastOpCode::Plain, 0};
    else lastOp = LastOp{LastOpCode::Bubble, num};
}

void EditWindow::Impl::setBuffer(const GsvBuffer& buf)
{
    buffer = buf;
    view.set_buffer(buf);
}

void EditWindow::Impl::setLabelText(const string& text)
{
    label->set_text(text);
}

ShadeMode EditWindow::Impl::shadeMode(const ShadeMode& sm)
{
    if ((sm == ShadeMode::Query) || (sm == shadeModeStatus)) {
	// Nothing to do.
    }
    else if (shadeModeStatus == ShadeMode::Unshaded) {
	// EditWindow should be shaded.
	ew->remove(scrolledWindow);
	shadeModeStatus = ShadeMode::Shaded;
    }
    else {	// Otherwise, EditWindow should be unshaded.
	ew->add(scrolledWindow);
	shadeModeStatus = ShadeMode::Unshaded;
	grabFocus();
    }

    return shadeModeStatus;
}

//// key handlers ////

bool EditWindow::Impl::kh_bubble(GdkEventKey* ev)
{
    // lastOp will be cleared by WindowMgr.
    commandMgr->execute("bubble");
    return true;
}

bool EditWindow::Impl::kh_ctrlX(GdkEventKey* ev)
{
    lastOp = LastOp{LastOpCode::CtrlX, 0};
    return true;
}

bool EditWindow::Impl::kh_deleteBuffer(GdkEventKey* ev)
{
    lastOp = LastOp{LastOpCode::Plain, 0};
    commandMgr->execute("bd");
    return true;
}

bool EditWindow::Impl::kh_deleteWindow(GdkEventKey* ev)
{
    lastOp = LastOp{LastOpCode::Plain, 0};
    commandMgr->execute("close");
    return true;
}

bool EditWindow::Impl::kh_findFile(GdkEventKey* ev)
{
    lastOp = LastOp{LastOpCode::Plain, 0};
    commandMgr->execute("choose");
    return true;
}

bool EditWindow::Impl::kh_focusMinibuffer(GdkEventKey* ev)
{
    lastOp = LastOp{LastOpCode::Plain, 0};
    windowMgr->focusMinibuffer();
    return true;
}

bool EditWindow::Impl::kh_quit(GdkEventKey* ev)
{
    lastOp = LastOp{LastOpCode::Plain, 0};
    commandMgr->execute("q");
    return true;
}

bool EditWindow::Impl::kh_recenter(GdkEventKey* ev)
{
    double position;
    if (std::get<0>(lastOp) == LastOpCode::Recenter) {
	if (std::get<1>(lastOp) == 1) { // This is the 2nd time; go to the top.
	    position = 0;
	    lastOp = LastOp{LastOpCode::Recenter, 2};
	}
	else {
	    // This is the 3rd time; go to the bottom.  Also clear lastOp.
	    position = 1;
	    lastOp = LastOp{LastOpCode::Plain, 0};
	}
    }
    else {	// This is the 1st time; go to the mid.
	position = 0.5;
	lastOp = LastOp{LastOpCode::Recenter, 1};
    }

    auto iter = getBuffer()->get_insert()->get_iter();
    getView().scroll_to(iter, 0, 0, position);

    return true;
}

bool EditWindow::Impl::kh_scrollDown(GdkEventKey* ev)
{
    return kh_scroll_sub(false);
}

bool EditWindow::Impl::kh_scrollUp(GdkEventKey* ev)
{
    return kh_scroll_sub(true);
}

bool EditWindow::Impl::kh_scroll_sub(bool forward)
{
    auto lastOffset = getBuffer()->get_insert()->get_iter().get_offset();
    lastOp = LastOp{LastOpCode::LongMovement, lastOffset};

    // Get the Y coord (in buffer coordinates) of the target line.
    // The target is the bottom line of the visible area if going forward,
    // or the top line if backward.
    Gdk::Rectangle visible_rect;
    getView().get_visible_rect(visible_rect);
    int targetLine = visible_rect.get_y();
    if (forward)
	targetLine += visible_rect.get_height();

    // Get the iterator for targetLine.
    Gtk::TextBuffer::iterator iter;
    getView().get_iter_at_location(iter, 0, targetLine);

    if (false) {	// TODO if transient mode
	// Move 'insert' mark, leave 'selection-bound'.
	getBuffer()->move_mark_by_name("insert", iter);
    }
    else {	// Move both 'insert' and 'selection-bound'.
	getBuffer()->place_cursor(iter);
    }

    getView().scroll_to(iter, 0, 0, (forward? 0: 1));
    // getView().place_cursor_onscreen();

    return true;
}

bool EditWindow::Impl::kh_splitWindow(GdkEventKey* ev)
{
    lastOp = LastOp{LastOpCode::Plain, 0};
    commandMgr->execute("split");
    return true;
}

bool EditWindow::Impl::kh_switchBuffer(GdkEventKey* ev)
{
    // TODO Implement this.
    lastOp = LastOp{LastOpCode::Plain, 0};
    commandMgr->execute("files");
    return true;
}

//// event handlers ////

bool EditWindow::Impl::colorBoxOnButtonPress(GdkEventButton* ev)
{
    lastOp = LastOp{LastOpCode::Plain, 0};
    if (ev->button == 1) {
	ew->shadeMode(ShadeMode::Toggle);
	return true;
    }
    return false;
}

void EditWindow::Impl::colorBoxOnDragDataGet(
    const Glib::RefPtr<Gdk::DragContext>&,
    Gtk::SelectionData& selData, guint, guint)
{
    selData.set(8, (const guchar*)&ew, sizeof(guchar*));
}

void EditWindow::Impl::viewOnDragDataReceived(
    const Glib::RefPtr<Gdk::DragContext>& context, int x, int y,
    const Gtk::SelectionData& selData, guint info, guint time)
{
    EditWindow* srcEW = *(EditWindow**)(selData.get_data());
    context->drag_finish(true, true, false);
    windowMgr->moveWindow(srcEW, ew);
    srcEW->getView().grab_focus();
}

bool EditWindow::Impl::viewOnFocusInOut(GdkEventFocus* ev)
{
    lastOp = LastOp{LastOpCode::Plain, 0};
    if (ev->in) {	// focus in
	view.set_highlight_current_line(true);
	windowMgr->setFrontEditWindow(ew);
    } else {	// focus out
	view.set_highlight_current_line(false);
    }
    return false;
}

bool EditWindow::Impl::viewOnKeyPress(GdkEventKey* ev)
{
    constexpr auto ALL_MODIFIERS =
	GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK;
    if (ev->is_modifier)
	return true;

    // Which keymap to use?
    std::map<guint, KeyHandler>* keymap;
    switch (ev->state & ALL_MODIFIERS) {
    case GDK_CONTROL_MASK:
	if (std::get<0>(lastOp) == LastOpCode::CtrlX)
	    keymap = &ctrlXCtrlKeyMap;
	else keymap = &ctrlKeyMap;
	break;
    case GDK_MOD1_MASK:
	keymap = &mod1KeyMap;
	break;
    default:
	if (std::get<0>(lastOp) == LastOpCode::CtrlX) {
	    keymap = &ctrlXKeyMap;
	    break;
	}
	else {	// no keymap applicable; plain self-insert
	    lastOp = LastOp{LastOpCode::Plain, 0};
	    return false;
	}
    }

    auto it = keymap->find(ev->keyval);
    if (it != end(*keymap))
	return (this->*((*it).second))(ev);	// Execute it!

    commandMgr->log("invalid keybind");
    lastOp = LastOp{LastOpCode::Plain, 0};
    return true;
}

bool EditWindow::Impl::viewOnScroll(GdkEventScroll* ev)
{
    lastOp = LastOp{LastOpCode::Plain, 0};
    view.place_cursor_onscreen();	// doesn't work; gtkmm bug?
    return false;
}

//// interface class ////

EditWindow::EditWindow() : Gtk::Grid(), pimpl{new Impl{this}} {}
EditWindow::~EditWindow() = default;
unsigned int EditWindow::getBubbleNumber() { return pimpl->getBubbleNumber(); }
GsvBuffer& EditWindow::getBuffer() { return pimpl->getBuffer(); }
Gtk::EventBox* EditWindow::getColorBox() { return pimpl->getColorBox(); }
Gsv::View& EditWindow::getView() { return pimpl->getView(); }
void EditWindow::gotoLine(unsigned int lineNum) { pimpl->gotoLine(lineNum); }
void EditWindow::grabFocus() { pimpl->grabFocus(); }
void EditWindow::setBubbleNumber(unsigned int num) {
    pimpl->setBubbleNumber(num);
}
void EditWindow::setBuffer(const GsvBuffer& buf) { pimpl->setBuffer(buf); }
void EditWindow::setLabelText(const string& text) {
    pimpl->setLabelText(text);
}
ShadeMode EditWindow::shadeMode(const ShadeMode& sm) {
    return pimpl->shadeMode(sm);
}

// eof
