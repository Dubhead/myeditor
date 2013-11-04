// main.cc

#include <vector>
#include <gtkmm/main.h>
#include "command.h"
#include "global.h"
#include "filewindow.h"
#include "filemgr.h"
#include "scratchwindow.h"
#include "windowmgr.h"

using std::string;
using std::vector;

Command* commandMgr;
FileMgr* fileMgr;
WindowMgr* windowMgr;

int main(int argc, char* argv[])
{
    Gtk::Main kit(argc, argv);
    Gsv::init();

    commandMgr = new Command();
    fileMgr = new FileMgr();
    fileMgr->init();
    windowMgr = new WindowMgr();
    windowMgr->init();

    // windowMgr->addWindow(*manage(new ScratchWindow()));

    if (argc > 1) {
	// File(s) are specified in the command line.
	// Load them and open the first.
	vector<string> args(argv + 1, argv + argc);
	for (const string& arg: args)
	    fileMgr->getFile(arg, true);
	commandMgr->execute("e " + args[0]);
    }

    kit.run(*windowMgr);

    return 0;
}

// eof
