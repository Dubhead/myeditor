#include <string>
#include "global.h"
#include "util.h"

using std::string;

string entilde(const string& path)
{
    const string home = Glib::get_home_dir();

    if (path.find(home) != 0)
	return path;
    return "~" + path.substr(home.size());
}

string toFullPath(const string& basedir, const string& path)
{
    if (path.find('/') == 0)
	return path;
    if (path.find('~') == 0)
	return Glib::get_home_dir() + path.substr(1);
    return Gio::File::create_for_path(basedir)->resolve_relative_path(path)->
	get_path();
}

// eof
