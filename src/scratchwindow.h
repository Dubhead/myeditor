#pragma once

#include <memory>
#include <string>
#include "global.h"
#include "editwindow.h"

class ScratchWindow: public EditWindow
{
public:
    ScratchWindow();
    virtual ~ScratchWindow();
    void init();
    void appendText(const std::string& text);
    const std::string shortDesc();
private:
    ScratchWindow(const ScratchWindow&) = delete;
    ScratchWindow(ScratchWindow&&) = delete;
    ScratchWindow& operator=(const ScratchWindow&) = delete;
    ScratchWindow& operator=(ScratchWindow&&) = delete;

    class Impl;
    const std::unique_ptr<Impl> pimpl;
};
