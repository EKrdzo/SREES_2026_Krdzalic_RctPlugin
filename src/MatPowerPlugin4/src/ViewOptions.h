#pragma once
#include <gui/View.h>
#include <gui/Label.h>
#include <gui/LineEdit.h>
#include <gui/GridLayout.h>
#include <gui/GridComposer.h>
#include <gui/VerticalLayout.h>
#include "MatPowerPlugin.h"
#include <cstdlib>

class ViewOptions : public gui::View
{
protected:
    gui::Label    _lblName;
    gui::LineEdit _editName;

    gui::Label    _lblMaxIter;
    gui::LineEdit _editMaxIter;

    gui::Label    _lblEps;
    gui::LineEdit _editEps;

    gui::GridLayout    _gl;
    gui::VerticalLayout _vl;
    Options             _options;

public:
    ViewOptions()
    : _lblName(tr("Model name:"))
    , _lblMaxIter(tr("Max iters:"))
    , _lblEps(tr("eps:"))
    , _gl(3, 2)
    , _vl(2)
    {
        _editName    = "MatPower Power Flow - Rectangular Coordinates";
        _editMaxIter = "100";
        _editEps     = "1e-6";

        gui::GridComposer gc(_gl);
        gc.appendRow(_lblName)    << _editName;
        gc.appendRow(_lblMaxIter) << _editMaxIter;
        gc.appendRow(_lblEps)     << _editEps;

        _vl << _gl;
        _vl.appendSpacer();

        setLayout(&_vl);
    }

    const Options& getOptions()
    {
        _options.modelName = _editName.getText();
        _options.maxIter   = std::atoi(_editMaxIter.getText().c_str());
        _options.eps       = std::atof(_editEps.getText().c_str());
        if (_options.maxIter <= 0) _options.maxIter = 100;
        if (_options.eps     <= 0) _options.eps     = 1e-6;
        return _options;
    }
};
