#pragma once
#include <gui/View.h>
#include <gui/Label.h>
#include <gui/Button.h>
#include <gui/LineEdit.h>
#include <gui/TextEdit.h>
#include <gui/HorizontalLayout.h>
#include <gui/GridLayout.h>
#include <gui/GridComposer.h>
#include <fo/FileOperations.h>
#include <gui/FileDialog.h>
#include "ViewOptions.h"
#include <string>

class ViewConv : public gui::View
{
protected:
    sc::IPlugin*          _pIPlugin;
    sc::IPlugin::CallBack _onComplete;
    ViewOptions*          _pViewOptions = nullptr;

    gui::Label    _lblFnIn;
    gui::LineEdit _editFnIn;
    gui::Button   _btnSelectInFn;

    gui::Label    _lblFnOut;
    gui::LineEdit _editFnOut;
    gui::Button   _btnSelectOutFn;

    gui::Label    _lblStatus;
    gui::LineEdit _editStatus;
    gui::TextEdit _te;

    gui::Button           _btnInfo;
    gui::Button           _btnLoad;
    gui::Button           _btnConvert;
    gui::Button           _btnCancel;
    gui::HorizontalLayout _hlButtons;
    gui::GridLayout       _gl;

    td::UINT4 _wndID = 10;

    // Prati trenutni mod: true = .m odabran (Convert mod), false = Load mod
    bool _importMode = false;

protected:
    static td::String resolveOutPath(const td::String& rawOut, const td::String& inFile)
    {
        std::string out(rawOut.c_str());
        std::string in(inFile.c_str());

        if (out.size() < 6 || out.substr(out.size() - 6) != ".dmodl")
            out += ".dmodl";

        bool hasDir = (out.find('/') != std::string::npos ||
                       out.find('\\') != std::string::npos);
        if (!hasDir)
        {
            size_t sep = in.find_last_of("/\\");
            if (sep != std::string::npos)
                out = in.substr(0, sep + 1) + out;
        }

        return td::String(out.c_str());
    }

    static bool hasDmodlExt(const td::String& fn)
    {
        std::string s(fn.c_str());
        return s.size() >= 6 && s.substr(s.size() - 6) == ".dmodl";
    }

    static bool hasMExt(const td::String& fn)
    {
        std::string s(fn.c_str());
        return s.size() >= 2 && s.substr(s.size() - 2) == ".m";
    }

    // Prebacuje UI u Convert mod (.m odabran) ili Load mod (.dmodl / prazno)
    void setImportMode(bool importMode)
    {
        _importMode = importMode;
        if (importMode)
        {
            // .m odabran: Save as red aktivan, Load posivljen, Convert aktivan
            _lblFnOut.setTitle(tr("Save as:"));
            _editFnOut.enable();
            _btnSelectOutFn.enable();
            _btnLoad.disable();
            _btnConvert.enable();
        }
        else
        {
            // Load mod: drugi red sluzi za odabir .dmodl koji se ucitava
            _lblFnOut.setTitle(tr("Load model:"));
            _editFnOut.enable();        // moze se upisati put rucno
            _btnSelectOutFn.enable();   // ... otvara dijalog za odabir .dmodl
            _btnLoad.enable();
            _btnConvert.disable();
        }
    }

    void handleUserActions()
    {
        // Odabir .m fajla za import — odabir uvijek prebacuje u Convert mod
        _btnSelectInFn.onClick([this] {
            gui::OpenFileDialog::show(this, tr("openMCase"), "*.m", _wndID + 1000,
                [this](gui::FileDialog* pDlg)
                {
                    if (pDlg->getStatus() == gui::FileDialog::Status::OK)
                    {
                        td::String fn = pDlg->getFileName();
                        if (!fn.isEmpty())
                        {
                            _editFnIn = fn;
                            _editFnIn.setFocus();
                            setImportMode(hasMExt(fn));
                        }
                    }
                });
        });

        // Odabir izlaznog .dmodl fajla (samo u Convert modu)
        _btnSelectOutFn.onClick([this] {
            gui::OpenFileDialog::show(this, tr("createDmodl"), "*.dmodl", _wndID + 2000,
                [this](gui::FileDialog* pDlg)
                {
                    if (pDlg->getStatus() == gui::FileDialog::Status::OK)
                    {
                        td::String fn = pDlg->getFileName();
                        if (!fn.isEmpty())
                        {
                            _editFnOut = fn;
                            _editFnOut.setFocus();
                        }
                    }
                });
        });

        // Info — provjera da li fajl postoji
        _btnInfo.onClick([this] {
            td::String fileName = _editFnIn.getText();
            if (fileName.isEmpty())
            { _editStatus = "ERROR! No file selected!"; return; }
            if (!fo::fileExists(fileName))
            { _editStatus = "ERROR! File doesn't exist!"; return; }
            _editStatus = "OK! File exists and is accessible.";
        });

        // Load — ucitava postojeci .dmodl iz drugog reda direktno u dTwin
        _btnLoad.onClick([this] {
            td::String fileName = _editFnOut.getText();
            if (fileName.isEmpty())
            { _editStatus = "ERROR! Select .dmodl file first!"; return; }
            if (!fo::fileExists(fileName))
            { _editStatus = "ERROR! File not found!"; return; }

            td::String content;
            if (!fo::loadBinaryFile(fileName, content))
            { _editStatus = "ERROR! Cannot load file!"; return; }

            auto pDigitModel = _pIPlugin->getArchive(sc::IPlugin::ArchType::DigitalModel);
            pDigitModel->put(content.c_str(), content.length());

            _editStatus = "OK! Model loaded.";
            _onComplete(_pIPlugin);
            gui::Window* pWnd = getParentWindow();
            pWnd->close();
            onClosedPluginWindow();
        });

        // Clear — brise sva polja i vraca u Load mod
        _btnCancel.onClick([this] {
            _editFnIn  = "";
            _editFnOut = "";
            _editStatus = "";
            setImportMode(false);
        });

        // Convert — parsira .m, gradi Y matricu, pise i ucitava .dmodl
        _btnConvert.onClick([this] {
            td::String inputFileName = _editFnIn.getText();
            if (inputFileName.isEmpty())
            { _editStatus = "ERROR! Select import .m file first!"; return; }
            if (!fo::fileExists(inputFileName))
            { _editStatus = "ERROR! Import file not found!"; return; }

            td::String rawOut = _editFnOut.getText();
            if (rawOut.isEmpty())
            { _editStatus = "ERROR! Output file name is empty!"; return; }

            td::String outFileName = resolveOutPath(rawOut, inputFileName);
            _editFnOut = outFileName;

            const auto& options = _pViewOptions->getOptions();
            if (!createModel(inputFileName, outFileName, _pIPlugin, options, _editStatus))
                return;

            _onComplete(_pIPlugin);
            gui::Window* pWnd = getParentWindow();
            pWnd->close();
            onClosedPluginWindow();
        });
    }

    ViewConv() = delete;

public:
    ViewConv(sc::IPlugin* pIPlugin, const sc::IPlugin::CallBack& onComplete)
    : _pIPlugin(pIPlugin)
    , _onComplete(onComplete)
    , _lblFnIn(tr("Import model (.m):"))
    , _lblFnOut(tr("Load model:"))
    , _lblStatus(tr("Status:"))
    , _btnSelectInFn("...")
    , _btnSelectOutFn("...")
    , _btnInfo(tr("Info"))
    , _btnLoad(tr("Load"))
    , _btnConvert(tr("Convert"))
    , _btnCancel(tr("Cancel"))
    , _hlButtons(5)
    , _gl(4, 3)
    {
        assert(_pIPlugin);
        _editStatus.setAsReadOnly();
        _te.setAsReadOnly();

        gui::GridComposer gc(_gl);
        gc.appendRow(_lblFnIn)   << _editFnIn   << _btnSelectInFn;
        gc.appendRow(_lblFnOut)  << _editFnOut  << _btnSelectOutFn;
        gc.appendRow(_lblStatus); gc.appendCol(_editStatus, 0);
        _hlButtons.appendSpacer() << _btnInfo << _btnLoad << _btnConvert << _btnCancel;
        gc.appendRow(_hlButtons, 2);

        setLayout(&_gl);
        handleUserActions();

        // Pocetno stanje: Load mod (nema .m fajla)
        setImportMode(false);
    }

    void setOptions(ViewOptions* pViewOptions) { _pViewOptions = pViewOptions; }
    td::String getOutFileName() const { return _editFnOut.getText(); }
};
