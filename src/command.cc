#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <boost/optional.hpp>
#include "chooser.h"
#include "command.h"
#include "global.h"
#include "filemgr.h"
#include "filewindow.h"
#include "util.h"
#include "windowmgr.h"

using std::map;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;

//// impl class ////

class Command::Impl
{
    typedef CommandStatus (Command::Impl::*CommandHandler)(
	const string& args);
    // using CommandHandler =
    //   CommandStatus (Command::Impl::*)(const string& args);

public:
    Impl(Command* parent);
    ~Impl() = default;
    CommandStatus execute(const string& command);
    CommandStatus ch_bubble(const string& _);
    CommandStatus ch_choose(const string& _);
    CommandStatus ch_close(const string& _);
    CommandStatus ch_deleteBuffer(const string& args);
    CommandStatus ch_edit(const string& args);
    CommandStatus ch_files(const string& _);
    CommandStatus ch_gotoLine(const string& args);
    CommandStatus ch_newColumn(const string& args);
    CommandStatus ch_quit(const string& args);
    CommandStatus ch_save(const string& args);
    CommandStatus ch_shade(const string& args);
    CommandStatus ch_split(const string& _);
    void log(const string& msg);

    Command* cp;
    map<string, CommandHandler> commandMap;
};

Command::Impl::Impl(Command* parent) : cp{parent} {
    commandMap = {
	{"bd", &Command::Impl::ch_deleteBuffer},
	{"bubble", &Command::Impl::ch_bubble},
	{"choose", &Command::Impl::ch_choose},
	{"close", &Command::Impl::ch_close},
	{"e", &Command::Impl::ch_edit},
	{"files", &Command::Impl::ch_files},
	{"newcol", &Command::Impl::ch_newColumn},
	{"q", &Command::Impl::ch_quit},
	{"shade", &Command::Impl::ch_shade},
	{"split", &Command::Impl::ch_split},
	{"w", &Command::Impl::ch_save},
    };
}

CommandStatus Command::Impl::execute(const string& command)
{
    // Split 'command' into commandName and the trailing 'args'.
    auto idxFirstNonspace = command.find_first_not_of(" ");
    if (idxFirstNonspace == string::npos)
	return CommandStatus{CommandStatusCode::EmptyCommand, ""};
    auto idxSpace = command.find_first_of(" ", idxFirstNonspace);
    string commandName{command.substr(idxFirstNonspace,
	idxSpace - idxFirstNonspace)};
    string args{""};
    if (idxSpace != string::npos) {
	auto idxCharAfterSpace = command.find_first_not_of(" ", idxSpace);
	args = command.substr(idxCharAfterSpace, string::npos);
    }

    // If commandName is all-numeric, it's a goto-line.
    if (commandName.find_first_not_of("0123456789") == string::npos) {
	return ch_gotoLine(commandName);
    }

    // If commandName actually exists, execute it and return its result.
    auto it = commandMap.find(commandName);
    if (it != end(commandMap)) {
	// return (this->*(commandMap[commandName]))(args);
	return (this->*((*it).second))(args);
    }

    string msg{"Command execute: command not found: "};
    msg += commandName;
    log(msg);
    return CommandStatus{CommandStatusCode::CommandNotFound,
	"command not found: " + commandName};
}

CommandStatus Command::Impl::ch_bubble(const string& args)
{
    windowMgr->bubble();
    return CommandStatus{CommandStatusCode::Success, ""};
}

CommandStatus Command::Impl::ch_choose(const string& args)
{
    auto kit = Gtk::Main::instance();

    // Chooser's initial directory is passed thru result.
    string result(".");
    if (!args.empty()) {
	result = args;
    } else {
	// If this is a FileWindow, its directory is the initial.
	EditWindow* ew = windowMgr->getCurrentFocus();
	if (typeid(*ew) == typeid(FileWindow)) {
	    FileWindow* fw = reinterpret_cast<FileWindow*>(ew);
	    result = fw->getFile()->getGioFile()->get_parent()->get_path();
	}
    }

    auto chooser = std::make_shared<Chooser>(&result);
    chooser->init("");
    chooser->set_modal(true);
    chooser->set_transient_for(*windowMgr);
    chooser->set_position(Gtk::WindowPosition::WIN_POS_CENTER_ON_PARENT);
    kit->run(*chooser);

    return execute(result);
}

CommandStatus Command::Impl::ch_close(const string& _)
{
    EditWindow* ew = windowMgr->getCurrentFocus();
    bool actuallyClosed = windowMgr->closeWindow(ew);
    if (!actuallyClosed)
	return CommandStatus{CommandStatusCode::Error,
	    "cannot close the only window in the column"};
    if (typeid(*ew) == typeid(FileWindow)) {
	FileWindow* fw = reinterpret_cast<FileWindow*>(ew);
	fw->getFile()->deleteBuffer(fw->getBuffer());
	delete ew;
    }
    return CommandStatus{CommandStatusCode::Success, ""};
}

// Delete the buffer (i.e. File) from the editor.
// This doesn't delete the file on filesystem.
CommandStatus Command::Impl::ch_deleteBuffer(const string& args)
{
    // Which File to delete?
    shared_ptr<File> fileToDelete;
    auto currentEW = windowMgr->getCurrentFocus();
    if (args.empty()) {
	if (typeid(*currentEW) != typeid(FileWindow))
	    return CommandStatus{CommandStatusCode::Error,
		"file to delete not specified"};
	auto fw = reinterpret_cast<FileWindow*>(currentEW);
	fileToDelete = fw->getFile();
    } else {
	// TODO
	return CommandStatus{CommandStatusCode::Success, ""};
    }

    windowMgr->closeWindowsForFile(fileToDelete);
    fileMgr->deleteFile(fileToDelete);

    return CommandStatus{CommandStatusCode::Success, ""};
}

CommandStatus Command::Impl::ch_edit(const string& args)
{
    // Get a GioFile for 'args'.
    GioFile giofile;
    string baseDir{"."};
    if ((args.find("/") != 0) && (args.find("~/") != 0)) {
	// 'args' is a relative path.
	// If the current focus is on a FileWindow,
	// 'args' is relative to it.
	EditWindow* ew = windowMgr->getCurrentFocus();
	if (typeid(*ew) == typeid(FileWindow)) {
	    FileWindow* fw = reinterpret_cast<FileWindow*>(ew);
	    baseDir = fw->getFile()->getGioFile()->get_parent()->get_path();
	}
    }
    giofile = Gio::File::create_for_path(toFullPath(baseDir, args));

    if (giofile->query_file_type() == Gio::FileType::FILE_TYPE_DIRECTORY) {
	// This may be a recursive call, but anyway...
	return ch_choose(giofile->get_path());
    }

    string fullpath{giofile->get_path()};
    auto opt_fw = windowMgr->getFileWindow(fullpath, true);
    if (!opt_fw)
	return CommandStatus{CommandStatusCode::Error, ""};
    (*opt_fw)->grabFocus();
    (*opt_fw)->shadeMode(ShadeMode::Unshaded);
    windowMgr->setFrontEditWindow(*opt_fw);

    return CommandStatus{CommandStatusCode::Success, ""};
}

CommandStatus Command::Impl::ch_files(const string& args)
{
    string text{"\n"};
    for (const auto& f: fileMgr->getFileNames())
	text += "e " + f + "\n";
    auto opt_sw = windowMgr->getScratchWindow(true);
    (*opt_sw)->appendText(text);
    (*opt_sw)->grabFocus();
    windowMgr->setFrontEditWindow(*opt_sw);
    return CommandStatus{CommandStatusCode::Success, ""};
}

CommandStatus Command::Impl::ch_gotoLine(const string& args)
{
    unsigned int lineNum;
    std::stringstream ss;
    ss << args;
    ss >> lineNum;

    auto ew = windowMgr->getCurrentFocus();
    ew->gotoLine(lineNum);

    return CommandStatus{CommandStatusCode::Success, ""};
}

CommandStatus Command::Impl::ch_newColumn(const string& args)
{
    EditWindow* ew = windowMgr->newColumn(optional<EditWindow*>());
    ew->grabFocus();
    windowMgr->setFrontEditWindow(ew);
    return CommandStatus{CommandStatusCode::Success, ""};
}

CommandStatus Command::Impl::ch_quit(const string& args)
{
    fileMgr->cleanup();
    Gtk::Main::quit();
    return CommandStatus{CommandStatusCode::Success, ""};
}

CommandStatus Command::Impl::ch_save(const string& args)
{
    auto ew = windowMgr->getCurrentFocus();
    ew->save(args);
    return CommandStatus{CommandStatusCode::Success, ""};
}

CommandStatus Command::Impl::ch_shade(const string& args)
{
    auto ew = windowMgr->getCurrentFocus();
    ew->shadeMode(ShadeMode::Toggle);
    return CommandStatus{CommandStatusCode::Success, ""};
}

CommandStatus Command::Impl::ch_split(const string& _)
{
    windowMgr->splitWindow(*(windowMgr->getCurrentFocus()));
    return CommandStatus{CommandStatusCode::Success, ""};
}

void Command::Impl::log(const string& args)
{
    windowMgr->setEntryPlaceholderText(args);
}

//// interface class ////

Command::Command() : pimpl{new Impl{this}} {}
Command::~Command() = default;
CommandStatus Command::execute(const string& command) {
    return pimpl->execute(command);
}
void Command::log(const std::string& msg) { pimpl->log(msg); }

// eof
