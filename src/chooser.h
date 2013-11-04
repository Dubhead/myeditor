#pragma once

#include <memory>
#include "global.h"

class Chooser: public Gtk::Window
{
public:
    Chooser(std::string* result);
    virtual ~Chooser();
    void init(const std::string& args);
private:
    Chooser(const Chooser&) = delete;
    Chooser(Chooser&&) = delete;
    Chooser& operator=(const Chooser&) = delete;
    Chooser& operator=(Chooser&&) = delete;

    class Impl;
    const std::unique_ptr<Impl> pimpl;
};

// eof
