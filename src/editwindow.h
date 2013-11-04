#pragma once

#include <memory>
#include <string>
#include "global.h"

class EditWindow: public Gtk::Grid
{
public:
    EditWindow();
    virtual ~EditWindow();
    unsigned int getBubbleNumber();
    GsvBuffer& getBuffer();
    Gtk::EventBox* getColorBox();
    Gsv::View& getView();
    void gotoLine(unsigned int lineNum);
    void grabFocus();
    virtual void save(const std::string& altFilename) {}
    void setBubbleNumber(unsigned int num);
    void setBuffer(const GsvBuffer& buf);
    void setLabelText(const std::string& text);
    ShadeMode shadeMode(const ShadeMode& sm);
    virtual const std::string shortDesc() { return ""; }
private:
    class Impl;
    const std::unique_ptr<Impl> pimpl;
};

// eof
