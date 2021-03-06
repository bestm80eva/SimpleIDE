/*
 * Copyright (c) 2012-2013 Parallax Inc.
 * Initial Code by John Steven Denson
 *
 * All Rights for this file are MIT Licensed.
 *
 TERMS OF USE: MIT License
 +--------------------------------------------------------------------
 Permission is hereby granted, free of charge, to any person obtaining
 a copy of this software and associated documentation files
 (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge,
 publish, distribute, sublicense, and/or sell copies of the Software,
 and to permit persons to whom the Software is furnished to do so,
 subject to the following conditions:

 The above copyright notice and this permission notice shall be
 included in all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 +--------------------------------------------------------------------
 */

#include "mainspinwindow.h"
#include "qextserialenumerator.h"
#include "qportcombobox.h"
#include "Sleeper.h"
#include "hintdialog.h"
#include "blinker.h"
#include "build.h"
#include "buildstatus.h"
//#include "quazip.h"
//#include "quazipfile.h"
#include "PropellerID.h"
#include "directory.h"

#define ENABLE_ADD_LINK
#define APP_FOLDER_TEMPLATES
//#define BUTTON_PORT_SCAN
#define CBCLICK_PORT_SCAN
#define SHOW_IDE_EARLY

/*
 * SIMPLE_BOARD_TOOLBAR puts Board Combo on the buttons toolbar.
 * We decided since simple users will rarely use the Board Combo
 * there isn't much point in having it visible at all times.
#define SIMPLE_BOARD_TOOLBAR
 */

#define ESP8266_MODULE

#define SD_TOOLS
#define APPWINDOW_START_HEIGHT 620
#define APPWINDOW_START_WIDTH 720
#define EDITOR_MIN_WIDTH 500
#define PROJECT_WIDTH 300

#define SOURCE_FILE_SAVE_TYPES "C (*.c);; C++ (*.cpp);; C Header (*.h);; COG C (*.cogc);; ECOG C (*.ecogc);; ESPIN (*.espin);; SPIN (*.spin);; Any (*)"
#ifdef ENABLE_OPEN_ALLZIPS
#define SOURCE_FILE_TYPES "Source Files (*.c *.cogc *.cpp *.ecogc *.espin *.h *.spin *.side *.zip *.zipside);; Any (*)"
#else
#define SOURCE_FILE_TYPES "Source Files (*.c *.cogc *.cpp *.ecogc *.espin *.h *.spin *.side;; Any (*)"
#endif
#define PROJECT_FILE_FILTER "SIDE Project (*.side);; All (*)"

#define GDB_TABNAME "GDB Output"
#define SIDE_EXTENSION ".side"
#define SPIN_TEXT      "SPIN"
#define SPIN_EXTENSION ".spin"
#define AUTO_PORT "AUTO"

#define CloseFile "Close"
#define NewFile "&New"
#define OpenFile "&Open"
#define SaveFile "&Save"
#define SaveAsFile "Save &As"

#define AddTab "Add Tab to Project"
#define OpenTab "Open Tab to Project"
#define AddLib "Add Simple &Library"
#define CloseProject "Close Project"
#define SaveAsProject "Save Project As"
#define SetProject "Set Project"
#define CloneProject "Clone Project"
#define ZipProject "Zip"

#define AddFileCopy "Add File Copy"
#define AddFileLink "Add File Link"
#define AddIncludePath "Add Include Path"
#define AddLibraryPath "Add Library Path"

#define ProjectView "Set Project View"
#define SimpleView  "Set Simple View"

#define FileToSDCard "File to SD Card"

#define BuildAllLibraries "Build All Libraries"

/**
 * @brief g_ApplicationClosing
 * Some events like terminal processes can cause bad behaviour if someone exits the program.
 * Use this global to ensure we don't continue writing to terminal on exit.
 */
bool g_ApplicationClosing = false;

/**
 * Having some global symbols may seem a little odd, but serves a purpose.
 * This is the IDE debug editor where all qDebug() output goes. The output is
 * used for IDE debug information displayed by Build Status with CTRL+SHIFT+D.
 */
QPlainTextEdit *debugStatus;

#if defined(IDEDEBUG)
#include "qapplication.h"

#ifdef QT5
void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    debugStatus->appendPlainText(msg);
}
#else
void myMessageOutput(QtMsgType type, const char *msg)
{
    debugStatus->appendPlainText(msg);
}
#endif
#endif

MainSpinWindow::MainSpinWindow(QWidget *parent) : QMainWindow(parent),
    compileStatusClickEnable(true), tabChangeDisable(false), fileChangeDisable(false)
{
#if defined(IDEDEBUG)
    debugStatus = new QPlainTextEdit(this);
    debugStatus->setLineWrapMode(QPlainTextEdit::NoWrap);
    debugStatus->setReadOnly(true);
#ifdef QT5
    qInstallMessageHandler(myMessageOutput);
#else
    qInstallMsgHandler(myMessageOutput);
#endif
#endif

    /* setup application registry info */
    QCoreApplication::setOrganizationName(publisherKey);
    QCoreApplication::setOrganizationDomain(publisherComKey);
    QCoreApplication::setApplicationName(ASideGuiKey);

    /* global settings */
    settings = new QSettings(publisherKey, ASideGuiKey, this);

    /* get last geometry. using x,y,w,h is unreliable.
     */
    QVariant geov = settings->value(ASideGuiGeometry);
    if(geov.canConvert(QVariant::ByteArray)) {
        // byte array convert is always possible
        QByteArray geo = geov.toByteArray();
        // restoreGeometry makes sure the array is valid
        this->restoreGeometry(geo);
    }

    statusDialog = new StatusDialog(this);

    /* setup properties dialog */
    propDialog = new Properties(this);
    connect(propDialog,SIGNAL(accepted()),this,SLOT(propertiesAccepted()));
    connect(propDialog,SIGNAL(clearAndExit()),this,SLOT(clearAndExit()));

    /* setup user's editor font */
    QVariant fontv = settings->value(editorFontKey);
    if(fontv.canConvert(QVariant::String)) {
        QString family = fontv.toString();
        editorFont = QFont(family);
    }
    else {
        int fontsz = 10;
#ifdef Q_OS_MAC
        fontsz = 14;
#endif
        editorFont = QFont("Courier", fontsz, QFont::Normal, false);
        settings->setValue(fontSizeKey,fontsz);
    }

    fontv = settings->value(fontSizeKey);
    if(fontv.canConvert(QVariant::Int)) {
        int size = fontv.toInt();
        editorFont.setPointSize(size);
    }
    else {
        editorFont.setPointSize(10);
    }


    /* setup new project dialog */
    newProjDialog = new NewProject(this);
    connect(newProjDialog,SIGNAL(accepted()),this,SLOT(newProjectAccepted()));

    /* setup find/replace dialog */
    replaceDialog = new ReplaceDialog(this);

    /* new ASideConfig class */
    aSideConfig = new ASideConfig();

    projectModel = NULL;
    referenceModel = NULL;

    /* main container */
    setWindowTitle(ASideGuiKey);
    vsplit = new QSplitter(this);
    setCentralWidget(vsplit);

    /* project tools */
    setupProjectTools(vsplit);

    /* start with an empty file if fresh install */
    newFile();

    /* get app settings at startup and before any compiler call */
    getApplicationSettings();

    /* set up ctag tool */
    ctags = new CTags(aSideCompilerPath);

    /* setup gui components */
    setupFileMenu();
    setupHelpMenu();
    setupToolBars();

    /* show gui */
    QApplication::processEvents();

    initBoardTypes();

    /* start a process object for the loader to use */
    process = new QProcess(this);

#ifdef ENABLE_WXLOADER
    wxProcess = new QProcess(this);

    connect(wxProcess, SIGNAL(readyReadStandardOutput()),this,SLOT(wxProcReadyRead()));
    connect(wxProcess, SIGNAL(error(QProcess::ProcessError)),this,SLOT(wxProcError(QProcess::ProcessError)));
    connect(wxProcess, SIGNAL(finished(int,QProcess::ExitStatus)),this,SLOT(wxProcFinished(int,QProcess::ExitStatus)));
#endif

    projectFile = "none";

    buildC = new BuildC(projectOptions, compileStatus, status, programSize, progress, cbBoard, propDialog);
#ifdef SPIN
    buildSpin = new BuildSpin(projectOptions, compileStatus, status, programSize, progress, cbBoard, propDialog);
#endif
    builder = buildC;

    connect(buildC, SIGNAL(showCompileStatusError()), this, SLOT(showCompileStatusError()));
#ifdef SPIN
    connect(buildSpin, SIGNAL(showCompileStatusError()), this, SLOT(showCompileStatusError()));
#endif

    /* setup loader and port listener */
    /* setup the terminal dialog box */
    term = new Terminal(this);
    connect(term, SIGNAL(enablePortCombo()),this,SLOT(enablePortCombo()));
    connect(term, SIGNAL(disablePortCombo()),this,SLOT(disablePortCombo()));

    termEditor = term->getEditor();

    QVariant gv = settings->value(termGeometryKey);
    if(gv.canConvert(QVariant::ByteArray)) {
        QByteArray geo = gv.toByteArray();
        term->restoreGeometry(geo);
    }

    /* Start with simpleview; detect user's startup view later. */
    simpleViewType = true;
    this->showSimpleView(simpleViewType);

#ifdef SHOW_IDE_EARLY
    this->show(); // show gui before about for mac
    this->raise();
    this->activateWindow();
    QApplication::processEvents();

    /* show help dialog */
    QVariant helpStartup = settings->value(helpStartupKey,true);
    if(helpStartup.canConvert(QVariant::Bool)) {
        if(helpStartup == true)
            aboutDialog->exec();
    }
#endif

    /* tell port listener to use terminal editor for i/o */
    portListener = new PortListener(this, termEditor);
    portListener->setTerminalWindow(termEditor);

    term->setPortListener(portListener);

    //term->setWindowTitle(QString(ASideGuiKey)+" "+tr("Simple Terminal"));
    // education request that the window title be SimpleIDE Terminal
    term->setWindowTitle(QString(ASideGuiKey)+" "+tr("Terminal"));

    connect(term,SIGNAL(accepted()),this,SLOT(terminalClosed()));
    connect(term,SIGNAL(rejected()),this,SLOT(terminalClosed()));


    /* Before window shows:
     * Create new workspace from package
     *   or
     * Replace an existing one that's out of date.
     */
    propDialog->replaceLearnWorkspace();


    /* get available ports at startup */
    enumeratePorts();

    portConnectionMonitor = new PortConnectionMonitor();
    connect(portConnectionMonitor, SIGNAL(portChanged()), this, SLOT(enumeratePortsEvent()));

    /* these are read once per app startup */
    QVariant lastportv  = settings->value(lastPortNameKey);
    if(lastportv.canConvert(QVariant::String))
        portName = lastportv.toString();

    /* setup the first port displayed in the combo box */
    if(cbPort->count() > 0) {
        int ndx = 0;
        if(portName.length() != 0) {
            for(int n = cbPort->count()-1; n > -1; n--)
                if(cbPort->itemText(n) == portName)
                {
                    ndx = n;
                    break;
                }
        }
        setCurrentPort(ndx);
    }

#ifdef ALWAYS_ALLOW_PROJECT_VIEW
    allowProjectView = true;
#else
    QVariant viewv = settings->value(allowProjectViewKey);
    allowProjectView = false;
    if(viewv.isNull() == false) {
        if(viewv.canConvert(QVariant::Int)) {
            allowProjectView = viewv.toInt() != 0;
        }
        if(allowProjectView) {
            QVariant viewv = settings->value(simpleViewKey);
            if(viewv.canConvert(QVariant::Bool)) {
                simpleViewType = viewv.toBool();
            }
        }
    }
#endif

    this->show();
    QApplication::processEvents();

    QString workspace;

    /* load the last file into the editor to make user happy */
    QVariant lastfilev = settings->value(lastFileNameKey);
    if(!lastfilev.isNull()) {
        if(lastfilev.canConvert(QVariant::String)) {
            if(lastfilev.toString().isEmpty()) {
                QVariant wrkv = settings->value(gccWorkspaceKey);
                if(wrkv.canConvert(QVariant::String)) {
                    workspace = wrkv.toString();
                    QString tmp = workspace+"My Projects/Welcome.side";
                    if(QFile::exists(tmp)) {
                        openProject(tmp);
                    }
                    else {
                        QMessageBox::critical(this,tr("File not found"),
                            tr("The SimpleIDE workspace default project is not available.")+" "+
                            tr("Program will start without a file and project."));
                    }
                }
            }
            else {
                QString fileName = lastfilev.toString();
                if(fileName.length() > 0) {
    #ifndef SPIN
                    if(fileName.mid(fileName.lastIndexOf(".")+1).contains("spin",Qt::CaseInsensitive)) {
                        QMessageBox::critical(
                                this,tr("SPIN Not Supported"),
                                tr("Spin projects are not supported with this version."),
                                QMessageBox::Ok);
                        return;
                    }
    #endif
                    if(QFile::exists(fileName)) {
                        //tabChangeDisable = true;
                        openFileName(fileName);
                        setProject(); // last file is always first project
                        //tabChangeDisable = false;
                    }
                    else {
                        QMessageBox::critical(this,tr("File not found"),
                            tr("The last opened file has disappeared.")+" "+
                            tr("Program will start without a file and project."));
                    }
                }
            }
        }
    }
    else {
        QVariant wrkv = settings->value(gccWorkspaceKey);
        if(wrkv.canConvert(QVariant::String)) {
            workspace = wrkv.toString();
            openProject(workspace+"My Projects/Welcome.side");
        }
    }

    /* get user's last open path */
    QVariant  lastfile = settings->value(lastFileNameKey);
    if(lastfile.canConvert(QVariant::String)) {
        if(lastfile.toString().isEmpty()) {
            lastPath = workspace;
        }
        else {
            lastPath = sourcePath(lastfile.toString());
        }
    }


    // old hardware dialog configuration feature
    //  hardwareDialog = new Hardware(this);
    //  connect(hardwareDialog,SIGNAL(accepted()),this,SLOT(initBoardTypes()));

    int tab = editorTabs->currentIndex();
    if(tab > -1) {
        Editor *ed = editors->at(tab);
        ed->setFocus();
        ed->raise();
    }

#ifndef SHOW_IDE_EARLY
    this->show(); // show gui before about for mac
    QApplication::processEvents();

    /* show help dialog */
    QVariant helpStartup = settings->value(helpStartupKey,true);
    if(helpStartup.canConvert(QVariant::Bool)) {
        if(helpStartup == true)
            aboutDialog->exec();
    }
#endif
    rescueDialog = new RescueDialog(this);

#if 0
    // remove according to issue 212
    /* show hint for Simple/Project view settings */
    if(this->simpleViewType)
        HintDialog::hint("SimpleProjectView", "Welcome to the new Simple View.  If you prefer Project View from previous versions, just click Tools and select Set Project View.");
    else
        HintDialog::hint("SimpleProjectView", "This is the Project View.  If you prefer Simple View from previous versions, just click Tools and select Set Simple View.");
#endif

    showSimpleView(simpleViewType);
}

void MainSpinWindow::keyHandler(QKeyEvent* event)
{
    //qDebug() << "MainSpinWindow::keyHandler";
    QString s = termEditor->eventKey(event);
    if(!s.length())
        return;
    sendPortMessage(s);
}

void MainSpinWindow::sendPortMessage(QString s)
{
    QByteArray barry;
    foreach(QChar c, s.toUtf8()) {
        Sleeper::ms(1);
        barry.append(c);
        portListener->send(barry);
        barry.clear();
        this->thread()->yieldCurrentThread();
    }
}

void MainSpinWindow::terminalEditorTextChanged()
{
    //QString text = termEditor->toPlainText();
}

/*
 * get the application settings from the registry for compile/startup
 */
void MainSpinWindow::getApplicationSettings()
{
    QFile file;
    QVariant compv = settings->value(gccCompilerKey);

    if(compv.canConvert(QVariant::String))
        aSideCompiler = compv.toString();

    if(!file.exists(aSideCompiler))
    {
        QMessageBox::critical(this,tr("GCC Compiler Not Found"),tr("GCC Compiler not found.\n\nPlease select the propeller-elf-gcc GCC Compiler in the Properties dialog."));
        propDialog->showProperties();
    }

    /* get the separator used at startup
     * Qt always translates \ to / so this isn't really necessary
     */
    QString appPath = QCoreApplication::applicationDirPath ();
    if(appPath.indexOf('\\') > -1)
        aSideSeparator = "\\";
    else
        aSideSeparator = "/";

    /* get the compiler path */
    if(aSideCompiler.indexOf('\\') > -1) {
        aSideCompilerPath = aSideCompiler.mid(0,aSideCompiler.lastIndexOf('\\')+1);
    }
    else if(aSideCompiler.indexOf('/') > -1) {
        aSideCompilerPath = aSideCompiler.mid(0,aSideCompiler.lastIndexOf('/')+1);
    }

#ifdef ENABLE_WXLOADER
    //aSideLoader = aSideCompilerPath + "proploader-latest-qt";
    aSideLoader = appPath+"/proploader";
#endif

#ifdef ENABLE_PROPELLER_LOAD
    aSideLoader = aSideCompilerPath + "propeller-load";
#endif

#if defined(Q_OS_WIN)
    aSideLoader += ".exe";
#endif

    /* get the include path and config file set by user */
    QVariant incv = settings->value(propLoaderKey);
    QVariant cfgv = settings->value(configFileKey);

    /* convert registry values to strings */
    if(incv.canConvert(QVariant::String))
        aSideIncludes = incv.toString();

    if(cfgv.canConvert(QVariant::String))
        aSideCfgFile = cfgv.toString();

    /* get doc path from include path */
    QString tmp = aSideIncludes;
    if(tmp.length() > 0) {
        if(tmp.at(tmp.length()-1) == '/')
            tmp = tmp.left(tmp.length()-1);
        tmp = tmp.left(tmp.lastIndexOf("/")+1)+"share/lib/html";
        aSideDocPath = tmp;
    }

    aSideCfgFile = aSideIncludes;

    if(!file.exists(aSideCfgFile))
    {
        QMessageBox::critical(this,tr("Propeller Loader Path Not Found"),tr("Please select propeller-load folder in the Properties dialog."));
        propDialog->showProperties();
    }
    else
    {
        /* load boards in case there were changes */
        aSideConfig->loadBoards(aSideCfgFile);
    }
}

void MainSpinWindow::exitSave()
{
    bool saveAll = false;
    QMessageBox mbox(QMessageBox::Question, tr("Save File?"), "",
                     QMessageBox::Discard | QMessageBox::Save | QMessageBox::SaveAll, this);

    saveProjectOptions();

    for(int tab = editorTabs->count()-1; tab > -1; tab--)
    {
        QString tabName = editorTabs->tabText(tab);
        if(tabName.at(tabName.length()-1) == '*')
        {
            mbox.setInformativeText(tr("Save File? ") + tabName.mid(0,tabName.indexOf(" *")));
            if(saveAll)
            {
                saveFileByTabIndex(tab);
            }
            else
            {
                int ret = mbox.exec();
                switch (ret) {
                    case QMessageBox::Discard:
                        // Don't Save was clicked
                        return;
                        break;
                    case QMessageBox::Save:
                        // Save was clicked
                        saveFileByTabIndex(tab);
                        break;
                    case QMessageBox::SaveAll:
                        // save all was clicked
                        saveAll = true;
                        break;
                    default:
                        // should never be reached
                        break;
                }
            }
        }
    }

}

void MainSpinWindow::closeEvent(QCloseEvent *event)
{
    if(statusDialog->isRunning()) {
        int rc = QMessageBox::critical(this,tr("The IDE Is Busy"),
            tr("Quitting the program now may cause a program error.")+"\n\n"+
            tr("Exit the program anyway?"),
            QMessageBox::Yes, QMessageBox::No);
        if(rc == QMessageBox::No) {
            event->ignore();
            return;
        }
    }
    quitProgram();
}

void MainSpinWindow::clearAndExit()
{
    QSettings settings(publisherKey, ASideGuiKey);
    QStringList list = settings.allKeys();

    foreach(QString key, list) {
        if(key.indexOf(ASideGuiKey) == 0) {
            settings.remove(key);
        }
    }

    settings.remove(publisherComKey);
    settings.remove(publisherKey);

}

void MainSpinWindow::quitProgram()
{
    ::g_ApplicationClosing = true;

    /* never leave port open */
    portListener->close();
    term->accept(); // just in case serial terminal is open

    portConnectionMonitor->stop();
    programStopBuild();

    exitSave(); // find

    QString fileName = "";

    if(settings->value(useKeys).toInt() == 1) {
        if(projectFile.isEmpty()) {
            fileName = editorTabs->tabToolTip(editorTabs->currentIndex());
            if(!fileName.isEmpty())
                settings->setValue(lastFileNameKey,fileName);
        }
        else {
            QFile proj(projectFile);
            if(proj.open(QFile::ReadOnly | QFile::Text)) {
                fileName = sourcePath(projectFile)+proj.readLine();
                fileName = fileName.trimmed();
                proj.close();
            }
            settings->setValue(lastFileNameKey,fileName);
            saveProjectOptions();
        }

        QString boardstr = cbBoard->itemText(cbBoard->currentIndex());
        QString portstr = cbPort->itemText(cbPort->currentIndex());

        settings->setValue(lastBoardNameKey,boardstr);
        settings->setValue(lastPortNameKey,portstr);

        QString fontstr = editorFont.toString();
        settings->setValue(editorFontKey,fontstr);

        int fontsize = editorFont.pointSize();
        settings->setValue(fontSizeKey,fontsize);

        // save user's width/height
        QByteArray geo = this->saveGeometry();
        settings->setValue(ASideGuiGeometry,geo);
    }
    else {
        settings->remove(useKeys);
        settings->remove(publisherComKey);
        settings->remove(publisherKey);
    }
    delete replaceDialog;
    delete propDialog;
    delete projectOptions;
    delete term;

    qApp->exit(0);
}

/*
 * The purpose of addLib is add an existing library to the project manager.
 * Several things will be necessary for this.
 * 1) #include "libname.h" gets added to source at top of the main project file
 *    (after first comment block) or after existing includes.
 * 2) -I and -L paths will be added to the project manager.
 *    Open dialog automatically gets set to parallax library path.
 * 3) -lname will be added to linker options.
 */
void MainSpinWindow::addLib()
{
    if(projectFile.isEmpty() || projectModel == NULL) {
        return;
    }

    // find library to add
    QString workspace = "";
    QVariant wrkv = settings->value(gccLibraryKey);
    if(wrkv.canConvert(QVariant::String)) {
        workspace = wrkv.toString();
    }

    if(workspace.isEmpty()){
        QMessageBox::warning(this, tr("No Workspace"), tr("A workspace should be defined in properties. Using last visited folder for library search."));
        workspace = this->lastPath;
    }

    QString path = QFileDialog::getExistingDirectory(this, tr("Add Library from Folder"),workspace);
    if(path.isEmpty())
        return;
    path = QDir::fromNativeSeparators(path);
    lastPath = path;

    // make sure we have a library that matches the library folder name
    QString libname = path;
    if(path.at(path.length()-1) == '/')
        path = path.left(path.length()-1);
    if(path.isEmpty())
        return;

    QString s;

    for(;;) {
        if(path.isEmpty() || path.lastIndexOf("/") < 0) {
            QMessageBox::information(this, tr("Can't find Library"),
                tr("Library not found in\n")+libname);
            return;
        }
        s= path.mid(path.lastIndexOf("/")+1);
        s = s.left(3);
        if(s.compare("lib") == 0) {
            libname = path.mid(path.lastIndexOf("/")+1);
            libname = libname.mid(3);
            break;
        }
        path = path.left(path.lastIndexOf("/"));
    }

    QString linkOptions = projectOptions->getLinkOptions();
    QStringList liblist = linkOptions.split(" ",QString::SkipEmptyParts);

    // don't error if libname exists in the linkOptions
    // just skip adding it.
    bool islname = false;
    foreach(QString lib, liblist) {
        if(lib.compare("-l"+libname) == 0) {
            islname = true;
        }
    }

    QString model = projectOptions->getMemModel();
    model = model.mid(0,model.indexOf(" "));
    if(QFile::exists(path+"/"+model+"/lib"+libname+".a") == false) {
        QMessageBox::critical(this, tr("Can't find Library"),
            model.toUpper()+" "+tr("memory model library not found in\n")+path);
        return;
    }

    // get the project and main file
    if(projectModel == NULL) {
        QMessageBox::critical(this,
            tr("No Project"),
            tr("A project must be opened before trying to add a library."));
        return;
    }

    QFile file(projectFile);
    if(projectFile.isEmpty() || file.exists() == false) {
        QMessageBox::critical(this,
            tr("Project File Not Found"),
            tr("Can't find the project file. A project file must exist before adding a library."));
        return;
    }

    QString projstr;
    if(file.open(QFile::ReadOnly | QFile::Text)) {
        projstr = file.readAll();
        file.close();
    } else {
        return;
    }
    QStringList list = projstr.split("\n");
    QString mainFile = projectFile.mid(0,projectFile.lastIndexOf("/"));
    mainFile += "/"+list[0];

    // add #include to main file
    // look for open file and use it if possible, else open a new file
    bool isOpen = false;
    for(int n = 0; n < editorTabs->count(); n++) {
        s = editorTabs->tabToolTip(n);
        if(s.compare(mainFile) == 0) {
            isOpen = true;
            editorTabs->setCurrentIndex(n);
        }
    }
    if(isOpen == false)
        openFileName(mainFile); // make sure mainFile is open

    Editor *ed = editors->at(editorTabs->currentIndex());
    QTextCursor cur = ed->textCursor();

    // find first error line
    QTextDocument *doc = ed->document();

    /*
     * look for a nice place to insert library.
     * if nice place not found, just add to top of file.
     */
    QString line;
    int len = doc->lineCount();
    int comment = 0;
    int candidate = 0;
    int added = 0;

    if(QFile::exists(path+"/"+libname+".h"))
    {
        for(int n = 0; n < len; n++) {
            QTextBlock block = doc->findBlockByLineNumber(n);
            line = block.text();
            if(line.contains("/*") && line.indexOf("*/") < line.indexOf("/*")) {
                comment++;
                candidate = 0;
            } else if(line.indexOf("#include \""+libname+".h\"") == 0) {
                    added++; /* mark as added */
                    break;
            } else if(line.contains("//")) {
                /* skip single line commented lines */
                candidate = n+1;
            } else if(comment != 0 && line.contains("*/")) {
                comment = 0;
                candidate = n+1;
            } else if(line.indexOf("#include") == 0) {
                if( line.indexOf("\""+libname+".h\"") > line.indexOf("#include")) {
                    if(line.indexOf("//") > 0 && line.indexOf("#include") > line.indexOf("//"))
                        break;
                    /* #include libname exists. don't add it */
                    added++; /* mark as added */
                    break;
                }
                else {
                    candidate = n+1;
                }
            }
            else if(candidate != 0) {
                /* insert here */
                cur.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
                cur.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor,candidate);
                cur.insertText("#include \""+libname+".h\"\n");
                ed->setTextCursor(cur);
                saveFile();
                openFileName(mainFile);
                added++;
                break;
            }
        }

        if(added == 0) {
            /* insert at beginning */
            cur.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
            cur.insertText("#include \""+libname+".h\"\n");
            ed->setTextCursor(cur);
            saveFile();
            openFileName(mainFile);
        }
    }

    // add -I and -L paths to project manager
    this->addProjectIncPath(path);
    this->addProjectLibPath(path);

    // add -lname to linker options if it's not already there
    if(islname == false) {
        s = projectOptions->getLinkOptions();
        if(s.isEmpty()) {
            s = "-l"+libname;
        }
        else {
            s += " -l"+libname;
        }
        projectOptions->setLinkOptions(s);
    }
}

int MainSpinWindow::tabIndexByShortName(QString shortname)
{
    for(int n = editorTabs->count()-1; n > -1; n--) {
        QString s = editorTabs->tabText(n);
        if(s.endsWith("*")) {
            s = s.mid(0,s.lastIndexOf("*"));
            s = s.trimmed();
        }
        if(shortname.compare(s) == 0) {
           return n;
        }
    }
    return -1;
}

int MainSpinWindow::tabIndexByFileName(QString filename)
{
    for(int n = editorTabs->count()-1; n > -1; n--) {
        QString s = editorTabs->tabText(n);
        QString t = editorTabs->tabToolTip(n);
        if(s.endsWith("*")) {
            s = s.mid(0,s.lastIndexOf("*"));
            s = s.trimmed();
        }
        if(shortFileName(filename).compare(s) == 0 &&
           filename.compare(t) == 0) {
           return n;
        }
    }
    return -1;
}

/*
 * The purpose of addTab is to create a new tab for the editor
 * with a user's new filename and add it to the project manager.
 */
void MainSpinWindow::addTab()
{
    if(projectFile.isEmpty() || projectModel == NULL) {
        return;
    }

    // new file tab
    newFile();

    QString selectedFilter = 0;
    QString filter;
    QStringList filtList = this->getAsFilters();
    int filtListCount = filtList.count();
    for(int n = 0; n < filtListCount; n++) {
        filter += filtList[n];
        if(n < filtListCount-1)
            filter += ";;";
    }

    QString filename = QFileDialog::getSaveFileName(this,tr("Add Tab"), sourcePath(projectFile)+"New File", filter, &selectedFilter);
    if(filename.isEmpty()) {
        editors->remove(tabIndexByShortName(untitledstr));
        editorTabs->removeTab(tabIndexByShortName(untitledstr));
        return;
    }

    if(sourcePath(projectFile).compare(sourcePath(filename),Qt::CaseInsensitive)) {
        editors->remove(tabIndexByShortName(untitledstr));
        editorTabs->removeTab(tabIndexByShortName(untitledstr));
        QMessageBox::critical(this,tr("Can't Add Tab"), tr("Can't Add Tab to a project from outside of the current project folder."));
        return;
    }

    tabChangeDisable = true;

    QString ftype = selectedFilter;
    filename = filterAsFilename(filename, ftype);

    // chose editor
    int tab = tabIndexByShortName(untitledstr);
    Editor *ed = editors->at(tab);

    // change tab file name and save file
    QString tipname  = filename;

    // add to project
    QString projFile = projectFile;
    projFile = projFile.mid(0,projFile.lastIndexOf("/"));
    QDir path(projFile);
    QString relfile = tipname;
    relfile = path.relativeFilePath(relfile);
    filename = this->shortFileName(filename);
    addProjectListFile(relfile);

    QString data;
    if(QFile::exists(tipname)) {
        QFile file(tipname);
        if(file.open(QFile::ReadOnly)) {
            QTextStream in(&file);
            if(this->isFileUTF16(&file))
                in.setCodec("UTF-16");
            else
                in.setCodec("UTF-8");
            data = in.readAll();
            file.close();
            setEditorTab(tab, filename, tipname, data);
        }
    }
    else if(ed->toPlainText().length() == 0) {
        data = "\n";
        setEditorTab(tab, filename, tipname, data);
        saveFile();
    }
    tabChangeDisable = false;
}

/*
 * The purpose of openTab is to create a new tab for the editor
 * with an existing file and add it to the project manager.
 */
void MainSpinWindow::openTab()
{
    if(projectFile.isEmpty() || projectModel == NULL) {
        return;
    }
    // new file tab
    newFile();

    QString filename = getOpenAsFile(this->sourcePath(projectFile));
    if(filename.isEmpty()) {
        editors->remove(tabIndexByShortName(untitledstr));
        editorTabs->removeTab(tabIndexByShortName(untitledstr));
        return;
    }
    if(sourcePath(projectFile).compare(sourcePath(filename),Qt::CaseInsensitive)) {
        editors->remove(tabIndexByShortName(untitledstr));
        editorTabs->removeTab(tabIndexByShortName(untitledstr));
        QMessageBox::information(this, tr("Can't Open Tab"), tr("Can't Open Tab to project from outside the current project folder."));
        return;
    }

    tabChangeDisable = true;

    while(tabIndexByFileName(filename) > -1) {
        int index = tabIndexByFileName(filename);
        editors->remove(index);
        editorTabs->removeTab(index);
    }

    // chose editor
    int tab = tabIndexByShortName(untitledstr);

    // change tab file name and save file
    QString tipname  = filename;

    // add to project
    QString projFile = projectFile;
    projFile = projFile.mid(0,projFile.lastIndexOf("/"));
    QDir path(projFile);
    QString relfile = tipname;
    relfile = path.relativeFilePath(relfile);
    filename = this->shortFileName(filename);

    QString data;
    if(QFile::exists(tipname)) {
        QFile file(tipname);
        if(file.open(QFile::ReadOnly)) {
            QTextStream in(&file);
            if(this->isFileUTF16(&file))
                in.setCodec("UTF-16");
            else
                in.setCodec("UTF-8");
            data = in.readAll();
            file.close();
            setEditorTab(tab, filename, tipname, data);
        }
        addProjectListFile(relfile);
    }
    tabChangeDisable = false;
}

void MainSpinWindow::newFile()
{
    fileChangeDisable = true;
    int tab = setupEditor();
    editorTabs->addTab(editors->at(tab),(const QString&)untitledstr);
    editorTabs->setCurrentIndex(tab);
    connect(editorTabs,SIGNAL(currentChanged(int)),this,SLOT(currentTabChanged()));
    Editor *ed = editors->at(tab);
    ed->setFocus();
    fileChangeDisable = false;
}

void MainSpinWindow::openFile(const QString &path)
{
    QString fileName = path;

    if (fileName.isNull()) {
        fileName = QFileDialog::getOpenFileName(this, tr("Open File"), lastPath, SOURCE_FILE_TYPES);
        fileName = fileName.trimmed();
        if(fileName.length() > 0)
            lastPath = sourcePath(fileName);
    }
#if 1
    // if side file and not the project file, open
    if(fileName.indexOf(SIDE_EXTENSION) > 0 &&
       fileName.compare(projectFile) != 0) {
        // save old project options before loading new one
        saveProjectOptions();
        // load new project
        projectFile = fileName;
        setCurrentProject(projectFile);
        QFile proj(projectFile);
        if(proj.open(QFile::ReadOnly | QFile::Text)) {
            fileName = sourcePath(projectFile)+proj.readLine();
            fileName = fileName.trimmed();
            proj.close();
        }
#ifndef SPIN
        if(fileName.mid(fileName.lastIndexOf(".")+1).contains("spin",Qt::CaseInsensitive)) {
            QMessageBox::critical(
                    this,tr("SPIN Not Supported"),
                    tr("Spin projects are not supported with this version."),
                    QMessageBox::Ok);
            return;
        }
#endif
        updateProjectTree(fileName);
    }
    else
#endif
    {
        if(fileName.length())
            setCurrentFile(fileName);
    }
    openFileName(fileName);

    /* for old project manager method only
    if(projectFile.length() == 0) {
        setProject();
    }
    else if(editorTabs->count() == 1) {
        setProject();
    }
    */
}

bool MainSpinWindow::isFileUTF16(QFile *file)
{
    char str[2];
    file->read(str,2);
    file->seek(0);
    if(str[0] == -1 && str[1] == -2) {
        return true;
    }
    return false;
}

QString MainSpinWindow::getUnzipTempPath(QString zFile)
{
    QString pathName = QDir::tempPath()+"/SimpleIDE_";
    while(pathName.indexOf("\\") > -1)
        pathName = pathName.replace("\\","/");
    while(zFile.indexOf("\\") > -1)
        zFile = zFile.replace("\\","/");
    qDebug() << "getUnzipTempPath" << pathName;
    if(zFile.indexOf("/") > 0) {
        int pos = zFile.lastIndexOf("/")+1;
        int len = zFile.lastIndexOf(".")-pos;
        pathName += zFile.mid(pos, len);
    }
    else {
        pathName += zFile.left(zFile.lastIndexOf("."));
    }
    qDebug() << "getUnzipTempPath" << pathName;
    return pathName;
}

void MainSpinWindow::openFileName(QString fileName)
{
    QString data;
    if (fileName == NULL || fileName.isEmpty()) {
        return;
    }
    QString s = fileName;


    if(s.endsWith(SIDE_EXTENSION, Qt::CaseInsensitive)) {
        if(s.compare(projectFile) != 0) {
            openProject(fileName);
        }
    }
#ifdef ENABLE_OPEN_ALLZIPS
    else if(s.endsWith(".zip",Qt::CaseInsensitive) || s.endsWith(".zipside",Qt::CaseInsensitive)) {
        Zipper  zip;
        QString zFile;
        if(zip.unzipFileCount(s) > 0) {
            QString folder = s.mid(0,s.lastIndexOf(".zip"));
            QString projName = folder;
            projName = projName.mid(projName.lastIndexOf("/")+1);
            QString projFile = projName+".side";

            statusDialog->init("Open File", fileName);

            if(!zip.unzipFileExists(s,projFile)) {
                projFile = projName+"/"+projName+".side";
            }

            if(zip.unzipFileExists(s,projFile)) {
                QString pathName = getUnzipTempPath(projName);
/*
                QString pathName = QDir::tempPath()+"/SimpleIDE_"+projName;
                while(pathName.indexOf("\\") > -1)
                    pathName = pathName.replace("\\","/");
*/
                QDir dst(pathName);
                if(!dst.exists())
                    dst.mkdir(pathName);

                statusDialog->init("Unzip", "Unzipping project: "+projFile+"\n"+pathName);
                zip.unzipAll(s,pathName);
                statusDialog->stop(4);
                openProject(pathName+"/"+projFile);
            }
            else {
                //data = zip.unzipFirstFile(s, &zFile);
                statusDialog->init("Unzip", "Finding zip files.\n"+s);
                zFile = zip.unzipTopTypeFile(s, ".side");
                statusDialog->stop(4);
                if(zFile.endsWith(".side")) {
                    QString pathName = getUnzipTempPath(zFile);
                    QDir dst(pathName);
                    if(!dst.exists())
                        dst.mkdir(pathName);

                    statusDialog->init("Unzip", "Unzipping .side project: "+zFile+"\n"+pathName);
                    zip.unzipAll(s,pathName);
                    statusDialog->stop(4);
                    openProject(pathName+"/"+zFile);
                }
                else {
                    int unzip = 0;
                    zFile = zip.unzipTopTypeFile(s, ".c");
                    if(zFile.endsWith(".c")) {
                        unzip++;
                    }
                    else {
                        zFile = zip.unzipTopTypeFile(s, ".cpp");
                        if(zFile.endsWith(".cpp")) {
                            unzip++;
                        }
                        else {
                            zFile = zip.unzipTopTypeFile(s, ".spin");
                            if(zFile.endsWith(".spin")) {
                                unzip++;
                            }
                        }
                    }

                    if(unzip)
                    {
                        QString pathName = getUnzipTempPath(zFile);
                        QDir dst(pathName);
                        if(!dst.exists())
                            dst.mkdir(pathName);
                        statusDialog->init("Unzip", "Unzipping file: "+zFile+"\n"+pathName);
                        zip.unzipAll(s,pathName);
                        statusDialog->stop(4);

                        QFile file(pathName+"/"+zFile);
                        if (file.open(QFile::ReadOnly))
                        {
                            QTextStream in(&file);
                            if(this->isFileUTF16(&file))
                                in.setCodec("UTF-16");
                            else
                                in.setCodec("UTF-8");
                            data = in.readAll();
                            file.close();
                            openFileStringTab(pathName+"/"+zFile, data);
                        }
                    }
                }
            }
            statusDialog->stop(4);
        }
    }
#else
    else if(s.endsWith(".zip",Qt::CaseInsensitive)) {
        Zipper  zip;
        QString fileName;
        QString data;
        if(zip.unzipFileCount(s) > 0) {
            QString folder = s.mid(0,s.lastIndexOf(".zip"));
            QString projName = folder;
            projName = projName.mid(projName.lastIndexOf("/")+1);
            QString projFile = projName+".side";
            if(!zip.unzipFileExists(s,projFile)) {
                projFile = projName+"/"+projName+".side";
            }
            if(zip.unzipFileExists(s,projFile)) {
                QString pathName = QDir::tempPath()+"/SimpleIDE_"+projName;
                while(pathName.indexOf("\\") > -1)
                    pathName = pathName.replace("\\","/");
                QDir dst(pathName);
                if(!dst.exists())
                    dst.mkdir(pathName);
                zip.unzipAll(s,pathName);
                openProject(pathName+"/"+projFile);
            }
            else {
                data = zip.unzipFirstFile(s, &fileName);
            }
        }
        if(fileName.length() && data.length()) {
            openFileStringTab(fileName, data);
        }
    }
#endif
    else {
        QFile file(fileName);
        if (file.open(QFile::ReadOnly))
        {
            QTextStream in(&file);
            if(this->isFileUTF16(&file))
                in.setCodec("UTF-16");
            else
                in.setCodec("UTF-8");
            data = in.readAll();
            file.close();
#ifdef TAB_SPACE_EXPANSION
            // remove tab to space conversion for now.
            data = data.replace('\t',"    ");
#endif
#ifndef TODO
            openFileStringTab(fileName, data);
#else
            // remove this after testing
            QString sname = this->shortFileName(fileName);
            if(editorTabs->count()>0) {
                for(int n = editorTabs->count()-1; n > -1; n--) {
                    if(editorTabs->tabText(n) == sname) {
                        setEditorTab(n, sname, fileName, data);
                        return;
                    }
                }
            }
            if(editorTabs->tabText(0).contains(untitledstr)) {
                setEditorTab(0, sname, fileName, data);
                return;
            }
            newFile();
            setEditorTab(tabIndexByShortName(untitledstr), sname, fileName, data);
#endif
        }
    }
}

void MainSpinWindow::openFileStringTab(QString fileName, QString data)
{
    if(!fileName.contains("/")) fileName = "./"+fileName;
    QString sname = this->shortFileName(fileName);
    if(editorTabs->count()>0) {
        for(int n = editorTabs->count()-1; n > -1; n--) {
            if(editorTabs->tabText(n) == sname) {
                setEditorTab(n, sname, fileName, data);
                return;
            }
        }
    }
    if(editorTabs->tabText(0).contains(untitledstr)) {
        setEditorTab(0, sname, fileName, data);
        return;
    }
    newFile();
    setEditorTab(tabIndexByShortName(untitledstr), sname, fileName, data);
}

/*
 * this is similar to when a user clicks a tab's X
 */
void MainSpinWindow::closeFile()
{
    int tab = editorTabs->currentIndex();
    if(tab > -1) {
        closeTab(tab);
    }
}

/*
 * close project if open and close all tabs.
 * do exitSave function and close all windows.
 */
void MainSpinWindow::closeAll()
{
    saveProjectOptions();
    setWindowTitle(QString(ASideGuiKey));
    // this->projectOptions->clearOptions();
    if(projectModel != NULL) {
        delete projectModel;
        projectModel = NULL;
    }
    for(int tab = editorTabs->count()-1; tab > -1; tab--)
        closeTab(tab);
    projectFile = "";
    projectTree->hide();
}

QString MainSpinWindow::getNewProjectDialog(QString workspace, QStringList filters)
{
    QFileDialog dialog(this,tr("New Project"), workspace+"Project Name", "");

    QString comp = projectOptions->getCompiler();
    if(comp.compare(projectOptions->CPP_COMPILER) == 0) {
        filters.removeAt(1);
        filters.insert(0,"C++ Project (*.cpp)");
    }
#ifdef SPIN
    else if(comp.compare(projectOptions->SPIN_COMPILER) == 0) {
        filters.removeAt(2);
        filters.insert(0,"Spin Project (*.spin)");
    }
#endif
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setNameFilters(filters);
    if(dialog.exec() == QDialog::Rejected)
        return "";
    QStringList dstList = dialog.selectedFiles();
    if(dstList.length() < 1)
        return "";
    QString dstPath = dstList.at(0);
    if(dstPath.length() < 1)
       return "";

    QString ftype = dialog.selectedNameFilter();
    QString dstName = dstPath.mid(dstPath.lastIndexOf("/")+1);
    dstName = dstName.mid(0,dstName.lastIndexOf("."));
    dstName = dstName.trimmed();
    dstPath = dstPath.mid(0,dstPath.lastIndexOf("/")+1);
    dstPath += dstName;

    ftype = ftype.mid(ftype.lastIndexOf("."));
    ftype = ftype.mid(0,ftype.lastIndexOf(")"));
    if(ftype.endsWith(".c", Qt::CaseInsensitive)) {
        dstPath += ".c";
        //comp = projectOptions->C_COMPILER;
    }
    else if(ftype.endsWith(".cpp", Qt::CaseInsensitive)) {
        dstPath += ".cpp";
        //comp = projectOptions->CPP_COMPILER;
    }
#ifdef SPIN
    else if(ftype.endsWith(".spin", Qt::CaseInsensitive)) {
        dstPath += ".spin";
        //comp = projectOptions->SPIN_COMPILER;
    }
#endif
    else {
        QMessageBox::critical(this,
            tr("Unknown Project Type"),
            tr("The New Project type is not recognized.")+"\n"+
            tr("Please try again with a suggested type."));
        return "";
    }

    return dstPath;
}

/*
 * New project should start a new project by some means. Method has evolved ....
 * First generation wizard asked for project name and project folder (workspace?).
 * Second generation has Project View and Simple View.
 * Project View allows user to set project name and type using a Save QFileDialog.
 * Simple View should allow user to select the type of Blank Simple Project C/C++ (or SPIN?)
 */
void MainSpinWindow::newProject()
{
    if(builder != NULL) builder->clearIncludeHash();

    this->compileStatus->setPlainText("");
    this->status->setText("");
    this->programSize->setText("");

    // simpleView version
    if(this->simpleViewType) {

        QString workspace = "";

        QVariant wrkv = settings->value(gccWorkspaceKey);
        if(wrkv.canConvert(QVariant::String)) {
            workspace = wrkv.toString();
        }
        if(workspace.isEmpty()){
            QMessageBox::critical(this, tr("No Workspace"),
                tr("A workspace is not defined in properties for Blank Simple Project.")+"\n"+
                tr("Can not continue."));
            return;
        }
        if(workspace.endsWith("/") == false)
            workspace += "/";

        QStringList filters;
        filters << "C Project (*.c)";
        filters << "C++ Project (*.cpp)";
        #ifdef SPIN
        //filters << "Spin Project (*.spin)";
        #endif

        QString dstName = getNewProjectDialog(workspace, filters);
        dstName = dstName.trimmed();
        if(dstName.isEmpty())
            return;

        QString comp;
        QString dstProj = dstName;
        dstProj = dstProj.mid(0, dstProj.lastIndexOf("."))+".side";
        if(dstName.endsWith(".c")) {
            comp = projectOptions->C_COMPILER;
        }
        else if(dstName.endsWith(".cpp")) {
            comp = projectOptions->CPP_COMPILER;
        }
        else if(dstName.endsWith(".spin")) {
            comp = projectOptions->SPIN_COMPILER;
        }

        workspace = propDialog->getApplicationWorkspace();

        if(dstName.endsWith(".c")) {
            workspace = workspace + "My Projects/Blank Simple Project.side";
        }
        else if(dstName.endsWith(".cpp")) {
            workspace = workspace + "My Projects/Blank Simple C++ Project.side";
        }
        else if(dstName.endsWith(".spin")) {
            workspace = workspace + "My Projects/Blank Simple SPIN Project.side";
        }

        if(QFile::exists(workspace)) {
            if(copyProjectAs(workspace, dstProj, dstName) < 0)
                return; // error, don't open bad project
            this->openProject(dstProj);
        }
        else {
            QMessageBox::critical(this, tr("Project Not Found"),
                tr("Blank Simple Project was not found."));
        }
    }
    // new projectView version
    else {

        QString workspace;
        QVariant wrkv = settings->value(gccWorkspaceKey);
        if(wrkv.canConvert(QVariant::String)) {
            workspace = wrkv.toString();
        }
        if(workspace.isEmpty()){
            QMessageBox::warning(this, tr("No Workspace"),
                tr("A workspace is not defined in properties.")+"\n"+
                tr("Using last file path."));
            workspace = lastPath;
        }

        QFileDialog dialog(this,tr("New Project"), workspace, "");

        QStringList filters;
        filters << "C Project (*.c)";
        filters << "C++ Project (*.cpp)";
    #ifdef SPIN
        filters << "Spin Project (*.spin)";
    #endif

        QString comp = projectOptions->getCompiler();
        if(comp.compare(projectOptions->CPP_COMPILER) == 0) {
            filters.removeAt(1);
            filters.insert(0,"C++ Project (*.cpp)");
        }
    #ifdef SPIN
        else if(comp.compare(projectOptions->SPIN_COMPILER) == 0) {
            filters.removeAt(2);
            filters.insert(0,"Spin Project (*.spin)");
        }
    #endif
        dialog.setAcceptMode(QFileDialog::AcceptSave);
        dialog.setNameFilters(filters);
        if(dialog.exec() == QDialog::Rejected)
            return;
        QStringList dstList = dialog.selectedFiles();
        if(dstList.length() < 1)
            return;
        QString dstPath = dstList.at(0);
        dstPath = dstPath.trimmed();
        if(dstPath.length() < 1)
           return;

        QString ftype = dialog.selectedNameFilter();
        QString dstName = dstPath.mid(dstPath.lastIndexOf("/")+1);
        dstName = dstName.trimmed();
        dstName = dstName.mid(0,dstName.lastIndexOf("."));
        dstPath = dstPath.mid(0,dstPath.lastIndexOf("/")+1);
        dstPath += dstName+"/";
        lastPath = dstPath;

        ftype = ftype.mid(ftype.lastIndexOf("."));
        ftype = ftype.mid(0,ftype.lastIndexOf(")"));
        if(ftype.endsWith(".c", Qt::CaseInsensitive)) {
            comp = projectOptions->C_COMPILER;
        }
        else if(ftype.endsWith(".cpp", Qt::CaseInsensitive)) {
            comp = projectOptions->CPP_COMPILER;
        }
    #ifdef SPIN
        else if(ftype.endsWith(".spin", Qt::CaseInsensitive)) {
            comp = projectOptions->SPIN_COMPILER;
        }
    #endif
        else {
            QMessageBox::critical(this,
                tr("Unknown Project Type"),
                tr("The New Project type is not recognized.")+"\n"+
                tr("Please try again with a suggested type."));
            return;
        }

        QString dstProjFile= dstPath+dstName+SIDE_EXTENSION;
        QFile sidefile(dstProjFile);
        QDir  sidedir(dstPath);

        /**
         * New Project creates a new folder. If the folder already exists, ask to use it.
         */
        if(sidedir.exists()) {
            int rc;
            rc = QMessageBox::question(this,
                        tr("Overwrite?"),
                        tr("The New Project folder and files already exist.")+"\n"+
                        tr("Would you like to replace the project folder?"),
                        QMessageBox::Yes, QMessageBox::No);
            if(rc == QMessageBox::No)
                return;
            Directory::recursiveRemoveDir(dstPath);
        }

        /**
         * By this point we know we can make a new project.
         * Let's clean out the old one before proceeding.
         */
        closeProject();

        sidedir.mkdir(dstPath);

        QString sidestr = dstName+ftype+"\n";
        if(sidefile.open(QFile::WriteOnly | QFile::Text)) {
            sidefile.write(sidestr.toLatin1());
            sidefile.close();
        }
        else {
            QString trs = tr("Can't save file: ")+dstProjFile;
            QMessageBox::critical(this,tr("Save Failed"), trs);
            return;
        }

        QString dstSourceFile = dstPath+dstName+ftype;
        QFile mainfile(dstSourceFile);

        QString mainstr = NULL;

        if(ftype.endsWith(".c")) {
            QString main("/**\n" \
              " * This is the main "+dstName+" program file.\n" \
              " */\n" \
              "int main(void)\n" \
              "{\n" \
              "  return 0;\n" \
              "}\n" \
              "\n");
            mainstr = main;
        }
        else if(ftype.endsWith(".cpp")) {
            QString main("/**\n" \
              " * This is the main "+dstName+" program file.\n" \
              " */\n" \
              "int main(void)\n" \
              "{\n" \
              "  return 0;\n" \
              "}\n" \
              "\n");
            mainstr = main;
        }
        else if(ftype.endsWith(".spin")) {
            QString main("pub main\n" \
                 "\n" \
                 "  repeat\n" \
                 "\n" \
                 "\n");
            mainstr = main;
        }

        if(mainstr != NULL) {
            if(mainfile.open(QIODevice::WriteOnly)) {
                mainfile.write(mainstr.toUtf8());
                mainfile.close();
            }
            else {
                QString trs = tr("Can't save file: ")+dstSourceFile;
                QMessageBox::critical(this,tr("Save Failed"), trs);
                return;
            }
        }
        else {
            QString trs = tr("Can't save file: ")+dstSourceFile;
            QMessageBox::critical(this,tr("Save Failed"), trs);
            return;
        }

        projectFile = dstProjFile;

        qDebug() << "Project File: " << projectFile;
        setCurrentProject(projectFile);
        qDebug() << "Update Project: " << projectFile;
        updateProjectTree(dstSourceFile);
        qDebug() << "Open Project File: " << projectFile;
        openFile(projectFile);
        qDebug() << "Set Compiler: " + comp;
        projectOptions->setCompiler(comp);
        qDebug() << "Set Memory Model: CMM";
        projectOptions->setMemModel(projectOptions->memTypeCMM);
        qDebug() << "Set All Warnings: " + comp;
        projectOptions->setWarnAll(true);
        qDebug() << "Set 32bit Doubles: " + comp;
        projectOptions->set32bitDoubles(true);

        // board type GENERIC for simpleview
        for(int n = 0; n < cbBoard->count(); n++) {
            QString board = cbBoard->itemText(n);
            if(board.compare(GENERIC_BOARD,Qt::CaseInsensitive) == 0) {
                cbBoard->setCurrentIndex(n);
                break;
            }
        }

        QString s = projectOptions->getCompOptions();
        if(s.contains("-std=c99", Qt::CaseInsensitive) == false) {
            // only apply -std=c99 to standard C, not C++
            if(projectOptions->getCompiler().contains("C++") == false)
                projectOptions->setCompOptions(s + "-std=c99");
        }
        qDebug() << "Save Project File: " << projectFile;
        saveProjectOptions();
    }
}

void MainSpinWindow::newProjectAccepted()
{
    QString name = newProjDialog->getName();
    QString path = newProjDialog->getPath();
    name = name.trimmed();
    path = path.trimmed();
    QDir dir(path);

    QString comp = newProjDialog->getCompilerType();

    qDebug() << "Project Name:" << name;
    qDebug() << "Project Compiler:" << comp;
    qDebug() << "Project Path:" << path;

    QString C_maintemplate("/**\n" \
         " * @file "+name+".c\n" \
         " * This is the main "+name+" program file.\n" \
         " */\n" \
         "\n" \
         "/**\n" \
         " * Main program function.\n" \
         " */\n" \
         "int main(void)\n" \
         "{\n" \
         "    return 0;\n" \
         "}\n" \
         "\n");

    QString Cpp_maintemplate("/**\n" \
         " * @file "+name+".cpp\n" \
         " * This is the main "+name+" program file.\n" \
         " */\n" \
         "\n" \
         "/**\n" \
         " * Main program function.\n" \
         " */\n" \
         "int main(void)\n" \
         "{\n" \
         "    return 0;\n" \
         "}\n" \
         "\n");
#ifdef SPIN
    QString SPIN_maintemplate("{{\n" \
         " * @file "+name+".spin\n" \
         " * This is the main "+name+" program file.\n" \
         "}} \n" \
         "\n" \
         "{{\n" \
         " * Main program function.\n" \
         "}}\n" \
         "pub main\n" \
         "\n" \
         "    repeat\n" \
         "\n" \
         "\n");
#endif
    QString mains;
    QString mainName(path+"/"+name);
    
    if(comp.compare("C", Qt::CaseInsensitive) == 0) {
        mains = C_maintemplate;
        mainName += ".c";
        projectOptions->setCompiler("C");
    }
    else if(comp.compare("C++", Qt::CaseInsensitive) == 0) {
        mains = Cpp_maintemplate;
        mainName += ".cpp";
        projectOptions->setCompiler("C++");
    }
#ifdef SPIN
    else if(comp.compare("Spin", Qt::CaseInsensitive) == 0) {
        mains = SPIN_maintemplate;
        mainName += SPIN_EXTENSION;
        projectOptions->setCompiler(SPIN_TEXT);
    }
#endif
    else
    {
        return;
    }

    qDebug() << "Project Start File: " << mainName;

    closeProject();

    if(dir.exists(path) == 0)
        dir.mkdir(path);

    QFile mainfile(mainName);
    if(mainfile.exists() == false) {
        QTextStream os(&mainfile);
        os.setCodec("UTF-8"); // for now save everything as UTF-8
        if(mainfile.open(QFile::ReadWrite)) {
            os << mains;
            mainfile.close();
        }
    }

    projectFile = path+"/"+name+SIDE_EXTENSION;
    qDebug() << "Project File: " << projectFile;
    setCurrentProject(projectFile);
    qDebug() << "Update Project: " << projectFile;
    updateProjectTree(mainName);
    qDebug() << "Open Project File: " << projectFile;
    openFile(projectFile);
    qDebug() << "Set Compiler: " << comp;
    projectOptions->setCompiler(comp);
    qDebug() << "Save Project File: " << projectFile;
    saveProjectOptions();
}

void MainSpinWindow::openProject(const QString &path)
{
    QString fileName = path;

    if(builder != NULL) builder->clearIncludeHash();

    this->compileStatus->setPlainText("");
    this->status->setText("");
    this->programSize->setText("");

#ifndef SPIN
        if(fileName.mid(fileName.lastIndexOf(".")+1).contains("spin",Qt::CaseInsensitive)) {
            QMessageBox::critical(
                    this,tr("SPIN Not Supported"),
                    tr("Spin projects are not supported with this version."),
                    QMessageBox::Ok);
            return;
        }
#endif

    if (fileName.isNull()) {
        fileName = fileDialog.getOpenFileName(this, tr("Open Project"), this->sourcePath(projectFile), "Project Files (*.side)");
        fileName = fileName.trimmed();
        if(fileName.length() > 0)
            lastPath = sourcePath(fileName);
    }
    if(fileName.indexOf(SIDE_EXTENSION) > 0) {
        // save and close old project options before loading new one
        closeProject();
        // load new project
        projectFile = fileName;
        setCurrentProject(projectFile);
        QFile proj(projectFile);
        if(proj.open(QFile::ReadOnly | QFile::Text)) {
            fileName = sourcePath(projectFile)+proj.readLine();
            fileName = fileName.trimmed();
            proj.close();
        }

#ifndef SPIN
        if(fileName.mid(fileName.lastIndexOf(".")+1).contains("spin",Qt::CaseInsensitive)) {
            QMessageBox::critical(
                    this,tr("SPIN Not Supported"),
                    tr("Spin projects are not supported with this version."),
                    QMessageBox::Ok);
            return;
        }
#endif

        updateProjectTree(fileName);
    }
    openFileName(fileName);

    if(ctags->enabled()) {
#ifndef ENABLE_SPIN_BROWSING
        if(this->isSpinProject()) {
            btnBrowseBack->setEnabled(false);
            btnFindDef->setEnabled(false);
        }
        else {
            btnFindDef->setEnabled(true);
        }
#endif
    }

    /* for old project manager method only
    if(projectFile.length() == 0) {
        setProject();
    }
    else if(editorTabs->count() == 1) {
        setProject();
    }
    */

}

// find the difference between s1 and s2. i.e. result = s2 - s1
QString MainSpinWindow::pathDiff(QString s2, QString s1)
{
    s1 = s1.replace("\\","/");
    s2 = s2.replace("\\","/");
    QString result(s2);
    while(s2.contains(s1,Qt::CaseInsensitive) == false)
        s1 = s1.mid(0,s1.lastIndexOf("/"));
    result = result.replace(s1,"");
    return result;
}

/*
 * fixup project links.
 *
 * 1. if link is a full path (not link), make it a relative link
 * 2. adjust link to target project folder
 */
QString MainSpinWindow::saveAsProjectLinkFix(QString srcPath, QString dstPath, QString link)
{
    QString fix = "";

    link = link.replace("\\","/");
    srcPath = srcPath.replace("\\","/");
    dstPath = dstPath.replace("\\","/");

    /*
     * Important cases:
     * 1. source of project is application path.
     * 2. link is relative ../
     * 3. link is absolute /
     */
    QString fs;
    QDir path(dstPath);

    /*
     * If we are using the application path to reference templates,
     * we must use the Library path for links.
     */
#ifdef APP_FOLDER_TEMPLATES
    QString appPath = QApplication::applicationDirPath();
    if(srcPath.startsWith(appPath)) {
        QString library = "";
        QVariant libv = settings->value(gccLibraryKey);
        if(libv.canConvert(QVariant::String)) {
            library = libv.toString();
        }
        if(library.isEmpty()){
            QMessageBox::critical(this, tr("No GCC Library"), tr("A GCC Library must be defined in properties."));
            return fix;
        }
        if(library.endsWith("/") == false)
            library += "/";
        if(link.left(3) == "../") {
            fs = path.relativeFilePath(library+"../"+link);
        }
        else {
            return fix;
        }
        fix = fs;
        return fix;
    }
#endif

    if(link.left(3) == "../") {
        if(QFile::exists(srcPath+link) != true) {
            return fix;
        }
        QFile file(srcPath+link);
        fs = file.fileName();
        fs = path.relativeFilePath(fs);
        fix = fs;
    }
    else if(QFile::exists(link)) {
        fs = link;
        fs = path.relativeFilePath(fs);
        fix = fs;
    }
    else {
        QFile file(srcPath+link);
        fs = file.fileName();
        fs = path.relativeFilePath(fs);
        fix = fs;
    }
    return fix;
}

QStringList MainSpinWindow::saveAsProjectNewList(QStringList projList, QString projFolder, QString projFile, QString dstPath, QString dstProjFile)
{
    /*
     * Make a new project list. preprend project main file after this.
     * fixes up any links in project file and writes file
     */
    QStringList newList;
    QStringList emptyList;
    for(int n = 1; n < projList.length(); n++) {
        QString item = projList.at(n);

        // special handling
        // TODO - fix -I and -L too
        if(item.indexOf(FILELINK) > 0) {
            QStringList list = item.split(FILELINK,QString::SkipEmptyParts);
            if(list.length() < 2) {
                QMessageBox::information(
                        this,tr("Project File Error."),
                        tr("Save Project As expected a link, but got:\n")+item+"\n\n"+
                        tr("Please manually adjust it by adding a correct link in the Project Manager list.")+" "+
                        tr("After adding the correct link, remove the bad link."));
                return emptyList;
            }
            else {
                QString als = list[1];
                als = als.trimmed();
                als = QDir::fromNativeSeparators(als);
                als = saveAsProjectLinkFix(projFolder, dstPath, als);
                if(als.length() > 0)
                    item = list[0]+FILELINK+als;
                else {
                    QMessageBox::information(
                            this,tr("Can't Fix Link"),
                            tr("Save Project As was not able to fix the link:\n")+item+"\n\n"+
                            tr("Please manually adjust it by adding the correct link in the Project Manager list.")+" "+
                            tr("After adding the correct link, remove the bad link."));
                    return emptyList;
                }
                newList.append(item);
            }
        }
        else if(item.indexOf("-I") == 0) {
            QStringList list = item.split("-I",QString::SkipEmptyParts);
            if(list.length() < 1) {
                QMessageBox::information(
                        this,tr("Project File Error."),
                        tr("Save Project As expected a -I link, but got:\n")+item+"\n\n"+
                        tr("Please manually adjust it by adding a correct link in the Project Manager list.")+" "+
                        tr("After adding the correct link, remove the bad link."));
                return emptyList;
            }
            else {
                QString als = list[0];
                als = als.trimmed();
                als = QDir::fromNativeSeparators(als);
                als = saveAsProjectLinkFix(projFolder, dstPath, als);
                if(als.length() > 0)
                    item = "-I "+als;
                else {
                    QMessageBox::information(
                            this,tr("Can't Fix Link"),
                            tr("Save Project As was not able to fix the link:\n")+item+"\n\n"+
                            tr("Please manually adjust it by adding the correct link in the Project Manager list.")+" "+
                            tr("After adding the correct link, remove the bad link."));
                    return emptyList;
                }
                newList.append(item);
            }
        }
        else if(item.indexOf("-L") == 0) {
            QStringList list = item.split("-L",QString::SkipEmptyParts);
            if(list.length() < 1) {
                QMessageBox::information(
                        this,tr("Project File Error."),
                        tr("Save Project As expected a -L link, but got:\n")+item+"\n\n"+
                        tr("Please manually adjust it by adding a correct link in the Project Manager list.")+" "+
                        tr("After adding the correct link, remove the bad link."));
                return emptyList;
            }
            else {
                QString als = list[0];
                als = QDir::fromNativeSeparators(als);
                als = als.trimmed();
                als = saveAsProjectLinkFix(projFolder, dstPath, als);
                if(als.length() > 0)
                    item = "-L "+als;
                else {
                    QMessageBox::information(
                            this,tr("Can't Fix Link"),
                            tr("Save Project As was not able to fix the link:\n")+item+"\n\n"+
                            tr("Please manually adjust it by adding the correct link in the Project Manager list.")+" "+
                            tr("After adding the correct link, remove the bad link."));
                    return emptyList;
                }
                newList.append(item);
            }
        }
        // project options
        else if(item[0] == '>') {
            newList.append(item);
        }
        // different destination
        else if(sourcePath(projFile) != sourcePath(dstProjFile)) {
            QString myitem = this->shortFileName(item);
            item = QDir::fromNativeSeparators(sourcePath(projFile)+item);
            item = saveAsProjectLinkFix(sourcePath(projFile), sourcePath(dstProjFile), item);
            item = myitem + FILELINK + item;
            newList.append(item);

// no more file copy for save project as
#if 0
            QString dst = sourcePath(dstProjFile)+item;
            if(QFile::exists(dst))
                QFile::remove(dst);
            QFile::copy(sourcePath(projFile)+item, dst);
#endif
        }
        // same destination, just copy item
        else {
            newList.append(item);
        }
    }

    newList.sort();

    return newList;
}

int MainSpinWindow::copyProjectAs(QString srcProjFile, QString dstProjFile, QString mainFile)
{
    QString projFile = srcProjFile;
    QString projFolder = sourcePath(srcProjFile);
    QString dstPath = sourcePath(dstProjFile);

    QString ftype;
    QFile proj(projFile);
    if(proj.open(QFile::ReadOnly | QFile::Text)) {
        ftype = proj.readLine();
        proj.close();
    }
    else {
        QString trs = tr("Can't find file: ")+projFile;
        QMessageBox::critical(this,tr("Source Project not found!"), trs);
        return -1;
    }

    ftype = ftype.trimmed();
    ftype = ftype.mid(ftype.lastIndexOf("."));

    if(mainFile.endsWith(ftype) == false) {
        QString trs = tr("Incompatible Source Project Type ")+ftype+tr(" inside ")+projFile;
        QMessageBox::critical(this,tr("Simple New Project Failed"), trs);
        return -1;
    }

    /*
     * 3. creates new project folder if necessary (project can be in original folder)
     */
    QDir dpath(dstPath);

    if(dpath.exists() == false)
        dpath.mkdir(dstPath);

    if(dpath.exists() == false) {
        QMessageBox::critical(
                this,tr("Save Project As Error."),
                tr("System can not create project in ")+dstPath,
                QMessageBox::Ok);
        return -1;
    }

    /*
     * 4. copy project file from source to destination as new project name
     */

    // find .side file
    // remove new file before copy or copy will fail
    if(QFile::exists(dstProjFile))
        QFile::remove(dstProjFile);

    // copy project file
    QFile::copy(projFile,dstProjFile);

    QString projSrc;
    QFile sproj(projFile);
    if(sproj.open(QFile::ReadOnly | QFile::Text)) {
        projSrc = sproj.readAll();
        sproj.close();
    }

    /*
     * 5. copies the project main file from source to destination as new project main file
     */
    QStringList projList = projSrc.split("\n", QString::SkipEmptyParts);
    QString srcMainFile = sourcePath(projFile)+projList.at(0);
    QString dstMainFile = mainFile;

    // remove new file before copy or copy will fail
    if(QFile::exists(dstMainFile))
        QFile::remove(dstMainFile);
    // copy project main file
    QFile::copy(srcMainFile,dstMainFile);

    QStringList newList = saveAsProjectNewList(projList, projFolder, projFile, dstPath, dstProjFile);
    if(newList.isEmpty()) {
        return -1;
    }

    projSrc = dstMainFile.mid(dstMainFile.lastIndexOf("/")+1)+"\n";

    for(int n = 0; n < newList.length(); n++) {
        projSrc+=newList.at(n)+"\n";
    }

    QFile dproj(dstProjFile);
    if(dproj.open(QFile::WriteOnly | QFile::Text)) {
        dproj.write(projSrc.toLatin1());
        dproj.close();
    }

    this->openProject(dstProjFile);

    return 0;
}

#ifndef USE_ZIP_SAVEAS
/*
 * Save As project: saves a copy of the project in another path
 *
 * 1. function assumes an empty projectFolder parameter means to copy existing project
 * 2. asks user for destination project name and folder (folder and name can be different)
 *    (this is done differently for projectview and simpleview).
 * 3. creates new project folder if necessary (project can be in original folder)
 * 4. copy project file from source to destination as new project name
 * 5. copies the project main file from source to destination as new project main file
 *    fixes up any links in project file and writes file
 *
 */

int MainSpinWindow::saveAsProject(const QString &inputProjFile)
{
    int rc = 0;
    QString projFolder(sourcePath(inputProjFile));
    QString projFile = inputProjFile;

    if(builder != NULL) builder->clearIncludeHash();

    this->compileStatus->setPlainText("");
    this->status->setText("");
    this->programSize->setText("");
    this->programSize->setText("");

    /*
     * 1. function assumes an empty projectFolder parameter means to copy existing project.
     * if projectFolder is empty saveAs from existing project.
     */
    if(projFolder.length() == 0) {
        projFolder = sourcePath(projectFile);
    }
    if(projFile.length() == 0) {
        projFile = projectFile;
    }

    if(projFile.length() == 0) {
        QMessageBox::critical(
                this,tr("No Project"),
                tr("Can't \"Save Project As\" from an empty project.")+"\n"+
                tr("Please create a new project or open an existing one."),
                QMessageBox::Ok);
        return -1;
    }

    QString dstName;
    QString srcPath  = projFolder;
    QDir spath(srcPath);

    if(spath.exists() == false) {
        QMessageBox::critical(
                this,tr("Project Folder not Found."),
                tr("Can't \"Save Project As\" from a non-existing folder."),
                QMessageBox::Ok);
        return -1;
    }


    /*
     * 2. asks user for destination project name and folder (folder and name can be different)
     * get project name
     */
    QString dstPath;
    QString dstProjFile;

    /*
     * Serialize a new filename for user.
     */
    dstName = projFile.mid(0,projFile.lastIndexOf("."));
    dstName = dstName.trimmed();
    QRegExp reg("(\\d+)");
    if(dstName.indexOf(reg)) {
        if(dstName.lastIndexOf(")") == dstName.length()-1) {
            if(dstName.lastIndexOf(reg) > -1) {
                dstName = dstName.mid(0,dstName.lastIndexOf("("));
                dstName = dstName.trimmed();
            }
        }
    }

    /*
     * Enumerate until it's unique.
     */
    int num = 0;
    QString serial("");
    if(QFile::exists(dstName+serial+".side")) {
        do {
            num++;
            serial = QString(" (%1)").arg(num);
        } while(QFile::exists(dstName+serial+".side"));
    }

    // simpleView version
    QFileDialog dialog(this, tr("Save Project As"), dstName+serial+".side", "");
    QStringList filters;
    filters << "SimpleIDE Project (*.side)";

    dialog.setNameFilters(filters);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    rc = dialog.exec();
    if(rc == QDialog::Rejected)
        return -1;
    QStringList dstList = dialog.selectedFiles();
    if(dstList.length() < 1)
        return -1;
    dstPath = dstList.at(0);
    if(dstPath.length() < 1)
       return -1;

    if(projectFile.compare(dstPath) == 0) {
        QMessageBox::critical(this,
            tr("Can't Save Project As"),
            tr("Can't Save Project As to the origin project.")+"\n"+
            tr("Use Save Project instead or use a new project destination."));
        return -1;
    }

    dstName = dstPath.mid(dstPath.lastIndexOf("/")+1);
    dstName = dstName.mid(0,dstName.lastIndexOf("."));
    dstPath = dstPath.mid(0,dstPath.lastIndexOf("/")+1);
    lastPath = dstPath;


    dstProjFile = dstPath+dstName+SIDE_EXTENSION;

#if 0
    /*
     * Keep this in case someone decides that overwriting a project
     * except the original is a bad idea.
     */
    QFile sidefile(dstProjFile);

    /**
     * Don't overwrite an existing sidefile at all
     */
    if(sidefile.exists()) {
        QMessageBox::critical(this,
            tr("Can't Save As"),
            tr("Can't Save As to an existing project.")+"\n"+
            tr("Please try again using a different filename."));
        return;
    }
#endif

    QString ftype;
    QFile proj(projFile);
    if(proj.open(QFile::ReadOnly | QFile::Text)) {
        ftype = proj.readLine();
        proj.close();
    }
    else {
        QString trs = tr("Can't find file: ")+projFile;
        QMessageBox::critical(this,tr("Source Project not found!"), trs);
        return -1;
    }

    ftype = ftype.trimmed();
    ftype = ftype.mid(ftype.lastIndexOf("."));

#if 0
    /*
     * Keep this in case someone decides that overwriting a project
     * except the original is a bad idea.
     */
    QString dstSourceFile = dstPath+dstName+ftype;
    QFile mainfile(dstSourceFile);

    /**
     * don't overwrite existing main file without permission
     */
    if(mainfile.exists()) {
        QMessageBox::critical(this,
            tr("Can't Save As"),
            tr("Can't Save As to an existing project.")+"\n"+
            tr("Please try again using a different filename."));
        return -1;
    }
#endif

    /*
     * 3. creates new project folder if necessary (project can be in original folder)
     */
    QDir dpath(dstPath);

    if(dpath.exists() == false)
        dpath.mkdir(dstPath);

    if(dpath.exists() == false) {
        QMessageBox::critical(
                this,tr("Save Project As Error."),
                tr("System can not create project in ")+dstPath,
                QMessageBox::Ok);
        return -1;
    }

    /*
     * 4. copy project file from source to destination as new project name
     */

    // find .side file
    // remove new file before copy or copy will fail
    if(QFile::exists(dstProjFile))
        QFile::remove(dstProjFile);

    // copy project file
    QFile::copy(projFile,dstProjFile);

    QString projSrc;
    QFile sproj(projFile);
    if(sproj.open(QFile::ReadOnly | QFile::Text)) {
        projSrc = sproj.readAll();
        sproj.close();
    }

    /*
     * 5. copies the project main file from source to destination as new project main file
     */
    QStringList projList = projSrc.split("\n", QString::SkipEmptyParts);
    //QString srcMainFile = sourcePath(projFile)+projList.at(0);
    QString dstMainFile = projList.at(0);
    dstMainFile = dstMainFile.trimmed();
    QString dstMainExt = dstMainFile.mid(dstMainFile.lastIndexOf("."));
    dstMainFile = dstPath+dstName+dstMainExt;

    // remove new file before copy or copy will fail
    if(QFile::exists(dstMainFile))
        QFile::remove(dstMainFile);
    // copy project main file
    //QFile::copy(srcMainFile,dstMainFile);

    int tab = editorTabs->currentIndex();
    QString fileName = dstMainFile;
    QString data = editors->at(tab)->toPlainText();
    if(fileName.length() < 1) {
        qDebug() << "saveFileByTabIndex filename invalid tooltip " << tab;
        return -1;
    }
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QFile::WriteOnly)) {
            file.write(data.toUtf8());
            file.close();
        }
    }

    /*
     * Make a new project list. preprend project main file after this.
     * fixes up any links in project file and writes file
     */
    QStringList newList;
    for(int n = 1; n < projList.length(); n++) {
        QString item = projList.at(n);

        // special handling
        // TODO - fix -I and -L too
        if(item.indexOf(FILELINK) > 0) {
            QStringList list = item.split(FILELINK,QString::SkipEmptyParts);
            if(list.length() < 2) {
                QMessageBox::information(
                        this,tr("Project File Error."),
                        tr("Save Project As expected a link, but got:\n")+item+"\n\n"+
                        tr("Please manually adjust it by adding a correct link in the Project Manager list.")+" "+
                        tr("After adding the correct link, remove the bad link."));
            }
            else {
                QString als = list[1];
                als = als.trimmed();
                als = QDir::fromNativeSeparators(als);
                als = saveAsProjectLinkFix(projFolder, dstPath, als);
                if(als.length() > 0)
                    item = list[0]+FILELINK+als;
                else
                    QMessageBox::information(
                            this,tr("Can't Fix Link"),
                            tr("Save Project As was not able to fix the link:\n")+item+"\n\n"+
                            tr("Please manually adjust it by adding the correct link in the Project Manager list.")+" "+
                            tr("After adding the correct link, remove the bad link."));
                newList.append(item);
            }
        }
        else if(item.indexOf("-I") == 0) {
            QStringList list = item.split("-I",QString::SkipEmptyParts);
            if(list.length() < 1) {
                QMessageBox::information(
                        this,tr("Project File Error."),
                        tr("Save Project As expected a -I link, but got:\n")+item+"\n\n"+
                        tr("Please manually adjust it by adding a correct link in the Project Manager list.")+" "+
                        tr("After adding the correct link, remove the bad link."));
            }
            else {
                QString als = list[0];
                als = als.trimmed();
                als = QDir::fromNativeSeparators(als);
                als = saveAsProjectLinkFix(projFolder, dstPath, als);
                if(als.length() > 0)
                    item = "-I "+als;
                else
                    QMessageBox::information(
                            this,tr("Can't Fix Link"),
                            tr("Save Project As was not able to fix the link:\n")+item+"\n\n"+
                            tr("Please manually adjust it by adding the correct link in the Project Manager list.")+" "+
                            tr("After adding the correct link, remove the bad link."));
                newList.append(item);
            }
        }
        else if(item.indexOf("-L") == 0) {
            QStringList list = item.split("-L",QString::SkipEmptyParts);
            if(list.length() < 1) {
                QMessageBox::information(
                        this,tr("Project File Error."),
                        tr("Save Project As expected a -L link, but got:\n")+item+"\n\n"+
                        tr("Please manually adjust it by adding a correct link in the Project Manager list.")+" "+
                        tr("After adding the correct link, remove the bad link."));
            }
            else {
                QString als = list[0];
                als = QDir::fromNativeSeparators(als);
                als = als.trimmed();
                als = saveAsProjectLinkFix(projFolder, dstPath, als);
                if(als.length() > 0)
                    item = "-L "+als;
                else
                    QMessageBox::information(
                            this,tr("Can't Fix Link"),
                            tr("Save Project As was not able to fix the link:\n")+item+"\n\n"+
                            tr("Please manually adjust it by adding the correct link in the Project Manager list.")+" "+
                            tr("After adding the correct link, remove the bad link."));
                newList.append(item);
            }
        }
        // project options
        else if(item[0] == '>') {
            newList.append(item);
        }
        // different destination
        else if(sourcePath(projFile) != sourcePath(dstProjFile)) {
            QString myitem = this->shortFileName(item);
            item = QDir::fromNativeSeparators(sourcePath(projFile)+item);
            item = saveAsProjectLinkFix(sourcePath(projFile), sourcePath(dstProjFile), item);
            item = myitem + FILELINK + item;
            newList.append(item);

// no more file copy for save project as
#if 0
            QString dst = sourcePath(dstProjFile)+item;
            if(QFile::exists(dst))
                QFile::remove(dst);
            QFile::copy(sourcePath(projFile)+item, dst);
#endif
        }
        // same destination, just copy item
        else {
            newList.append(item);
        }
    }

    newList.sort();

    projSrc = dstName+dstMainExt+"\n";

    for(int n = 0; n < newList.length(); n++) {
        projSrc+=newList.at(n)+"\n";
    }

    QFile dproj(dstProjFile);
    if(dproj.open(QFile::WriteOnly | QFile::Text)) {
        dproj.write(projSrc.toLatin1());
        dproj.close();
    }

    this->openProject(dstProjFile);

    return 0;
}

#else

/*
 * This function uses the saveAsProjectZip methodology.
 * Saving just in case someone changes thei mind :innocent:
 */

/*
 * Save As project: saves a copy of the project in another path
 *
 * 1. function assumes an empty projectFolder parameter means to copy existing project
 * 2. asks user for destination project name. Folder will be created.
 * 3. creates new project folder if necessary (project can be in original folder)
 * 4. copy project file from source to destination as new project name
 * 5. copies the project main file from source to destination as new project main file
 *    fixes up any links in project file and writes file
 */
void MainSpinWindow::saveAsProject(const QString &inputProjFile)
{
    QString projFolder(sourcePath(inputProjFile));
    QString projFile = inputProjFile;

    /*
     * 1. function assumes an empty projectFolder parameter means to copy existing project.
     * if projectFolder is empty saveAs from existing project.
     */
    if(projFolder.length() == 0) {
        projFolder = sourcePath(projectFile);
    }
    if(projFile.length() == 0) {
        projFile = projectFile;
    }

    if(projFile.length() == 0) {
        QMessageBox::critical(
                this,tr("No Project"),
                tr("Can't \"Save Project As\" from an empty project.")+"\n"+
                tr("Please create a new project or open an existing one."),
                QMessageBox::Ok);
        return;
    }

    QString dstName;
    QString srcPath  = projFolder;
    QDir spath(srcPath);

    if(spath.exists() == false) {
        QMessageBox::critical(
                this,tr("Project Folder not Found."),
                tr("Can't \"Save Project As\" from a non-existing folder."),
                QMessageBox::Ok);
        return;
    }


    /*
     * 2. asks user for destination project name. Folder will be created.
     */
    QString dstPath;
    QString dstProjFile;

    // simpleView version
    QString saveAsProjectPrompt = this->sourcePath(projectFile)+"../"+this->shortFileName(projectFile);
    QFileDialog dialog(this, tr("Save Project As"), saveAsProjectPrompt, "");

    if(dialog.exec() == QDialog::Rejected)
        return;
    QStringList dstList = dialog.selectedFiles();
    if(dstList.length() < 1)
        return;
    dstPath = dstList.at(0);
    if(dstPath.length() < 1)
       return;

    dstName = dstPath.mid(dstPath.lastIndexOf("/")+1);
    dstName = dstName.mid(0,dstName.lastIndexOf("."));
    dstPath = dstPath.mid(0,dstPath.lastIndexOf("/")+1)+dstName+"/";
    //lastPath = dstPath;

    /*
     * creates new project folder
     */
    QDir dpath(dstPath);

    /**
     * Don't Self-Save As
     */
    if(srcPath.compare(dstPath) == 0) {
        QMessageBox::critical(this,
            tr("Can't Save Project As"),
            tr("Can't Save As the current project to itself.") + "\n"+
            tr("Please try again with a new project name."));
        return;
    }

    /**
     * Don't overwrite an existing sidefile without permission
     */
    if(dpath.exists()) {
        int rc;
        rc = QMessageBox::question(this,
                    tr("Overwrite?"),
                    tr("The new project folder exists. Replace it and all contents?"),
                    QMessageBox::Yes, QMessageBox::No);
        if(rc == QMessageBox::No)
            return;
        Directory::recursiveRemoveDir(dstPath);
    }

    dpath.mkdir(dstPath);

    if(dpath.exists() == false) {
        QMessageBox::critical(
                this,tr("Save Project As Error."),
                tr("System can not create project in ")+dstPath,
                QMessageBox::Ok);
        return;
    }

    dstProjFile = dstPath+dstName+SIDE_EXTENSION;

    /*
     * 4. copy project file from source to destination as new project name
     */

    // find .side file
    // remove new file before copy or copy will fail
    if(QFile::exists(dstProjFile))
        QFile::remove(dstProjFile);

    // copy project file
    QFile::copy(projFile,dstProjFile);

    QString projSrc;
    QFile sproj(projFile);
    if(sproj.open(QFile::ReadOnly | QFile::Text)) {
        projSrc = sproj.readAll();
        sproj.close();
    }

    /*
     * 5. copies the project main file from source to destination as new project
     */
    QStringList projList = projSrc.split("\n", QString::SkipEmptyParts);
    QString srcMainFile = sourcePath(projFile)+projList.at(0);
    QString dstMainFile = projList.at(0);
    dstMainFile = dstMainFile.trimmed();
    QString dstMainExt = dstMainFile.mid(dstMainFile.lastIndexOf("."));
    dstMainFile = dstPath+dstName+dstMainExt;

    // remove new file before copy or copy will fail
    if(QFile::exists(dstMainFile))
        QFile::remove(dstMainFile);

    // copy project main file
    QFile::copy(srcMainFile,dstMainFile);

    /*
     * Make a new project list. preprend project main file after this.
     * Copy libraries to this folder as ./library/\*
     * Fix up any links in project file and writes file.
     */

    QStringList newList;

    if(this->isCProject()) {
        newList = zipCproject(projList, srcPath, projFile, dstPath, dstProjFile, tr("Save As"));
    }
#ifdef SPIN
    else if(this->isSpinProject()) {
        newList = zipSPINproject(projList, srcPath, projFile, dstPath, dstProjFile);
    }
#endif

    if(newList.isEmpty()) {
        return;
    }

    projSrc = dstName+dstMainExt+"\n";

    for(int n = 0; n < newList.length(); n++) {
        if(newList.at(n).compare(dstName+dstMainExt) != 0)
            projSrc+=newList.at(n)+"\n";
    }

    QFile dproj(dstProjFile);
    if(dproj.open(QFile::WriteOnly | QFile::Text)) {
        dproj.write(projSrc.toLatin1());
        dproj.close();
    }

    this->openProject(dstProjFile);
}
#endif

/*
 * clone project copies a project from a source to destination path for education.
 */
void MainSpinWindow::cloneProject()
{
    QString src;
    QString dst;
    QVariant vs;

    vs = settings->value(cloneSrcKey,QVariant(lastPath));
    if(vs.canConvert(QVariant::String)) {
        src = vs.toString();
    }

    QString srcFile = QFileDialog::getOpenFileName(this,tr("Project to Clone"), src, PROJECT_FILE_FILTER);
    srcFile = srcFile.trimmed();
    if(srcFile.length() == 0)
        return;

    lastPath = srcFile;
    saveAsProject(srcFile);
}

/*
 * close project runs through project file list and closes files.
 * finally it closes the project manager side bar.
 */
void MainSpinWindow::closeProject()
{
    /* ask to save options
     */
    int rc = QMessageBox::No;

    if(projectFile.length() == 0)
        return;

    if(projectModel == NULL)
        return;

    /* go through project file list and close files
     */
    QFile file(projectFile);
    QString proj = "";
    if(file.open(QFile::ReadOnly | QFile::Text)) {
        proj = file.readAll();
        file.close();
    }

    proj = proj.trimmed(); // kill extra white space
    QStringList list = proj.split("\n");

    QStringList options = projectOptions->getOptions();
    QStringList files = projectModel->getRowList();

    int n = files.count();
    while(--n >= 0) {
        options.insert(0,files[n]);
    }

    /*
     * remove ">" from list so we can compare.
     */
    for(n = 0; n < list.count(); n++) {
        QString s = list[n];
        if(s.indexOf(">") == 0) {
            list.replace(n,s.mid(1));
        }
    }

    options.sort();
    list.sort();

    n = list.count();
    int mismatch = 0;
    if(n != options.count()) {
        mismatch++;
    }
    else {
        while(--n >= 0) {
            if(list[n].compare(options[n]) == 0)
                continue;
            mismatch++;
        }
    }

    if(mismatch && isSpinProject() == false) {
        rc = QMessageBox::question(this,
                tr("Save Project?"),
                tr("Save project manager settings before close?"),
                QMessageBox::Yes, QMessageBox::No);

        /* save options
         */
        if(rc == QMessageBox::Yes)
            saveProjectOptions();
    }

    /* Run through file list and delete matches.
     */
    for(int n = 0; n < list.length(); n++) {
        for(int tab = editorTabs->count()-1; tab > -1; tab--) {
            QString s = sourcePath(projectFile)+list.at(n);
            if(s.length() == 0)
                continue;
            if(s.at(0) == '>')
                continue;
            // close exact tab
            if(editorTabs->tabToolTip(tab).compare(s) == 0)
                closeTab(tab);
            s = s.mid(0,s.lastIndexOf('.'));
            if(editorTabs->tabToolTip(tab).compare(s+SHOW_ASM_EXTENTION) == 0)
                closeTab(tab);
            if(editorTabs->tabToolTip(tab).compare(s+SHOW_ASM_EXTENTION) == 0)
                closeTab(tab);
            if(editorTabs->tabToolTip(tab).compare(s+SHOW_MAP_EXTENTION) == 0)
                closeTab(tab);
        }
    }

    /* This causes us to lose project information on next load.
     * Not sure why. Leave it out for now.
     */
    projectOptions->clearOptions();

    /* close project manager side bar
     */
    setWindowTitle(QString(ASideGuiKey));
    if(projectModel != NULL) {
        delete projectModel;
        projectModel = NULL;
    }
    projectFile.clear();
}

void MainSpinWindow::recursiveAddDir(QString dir, QStringList *files)
{
    QDir dpath(dir);
    QString file;

    QStringList dlist;
    QStringList flist;

    if(dir.length() < 1)
        return;

    if(dir.at(dir.length()-1) != '/')
        dir += '/';

    flist = dpath.entryList(QDir::AllEntries, QDir::DirsLast);
    foreach(file, flist) {
        if(file.compare(".") == 0)
            continue;
        if(file.compare("..") == 0)
            continue;
        files->append(dir+file);
    }
    dlist = dpath.entryList(QDir::AllDirs, QDir::DirsLast);
    foreach(file, dlist) {
        if(file.compare(".") == 0)
            continue;
        if(file.compare("..") == 0)
            continue;
        recursiveAddDir(dir+file, files);
        files->append(dir+file);
    }
}

void MainSpinWindow::flattenDstProject(QString path, QString project)
{
    QString projstr;
    QFile file(path+"/"+project);
    if(file.exists()) {
        if(file.open(QFile::ReadOnly | QFile::Text)) {
            projstr = file.readAll();
            file.close();
        }
    }
    else {
        qDebug() << path+"/"+project << "not found";
        return;
    }
    QStringList list = projstr.split("\n", QString::SkipEmptyParts);
    QString dst;
    foreach (QString s, list) {
        if(s.mid(0,2).compare("-I") == 0 && s.contains("/lib")) {
            dst = s.mid(s.lastIndexOf("/"));
            list.removeOne(s);
            list.append("-I .."+dst);
        }
        if(s.mid(0,2).compare("-L") == 0 && s.contains("/lib")) {
            dst = s.mid(s.lastIndexOf("/"));
            list.removeOne(s);
            list.append("-L .."+dst);
        }
    }
    projstr = "";
    QString main = list.at(0);
    list.removeFirst();
    list.sort();
    list.insert(0,main);
    foreach (QString s, list) {
        projstr += s+"\n";
    }
    if(file.open(QFile::WriteOnly | QFile::Text)) {
        file.write(projstr.toLatin1());
        file.close();
    }
}

/*
 * Zip project saves any referenced non-standard libraries into a single library folder in the existing project.
 * It then creates an archive project such as name_zip.side or name_archive.side having adjusted path names.
 * If project options "Create Project Library" is checked, we save mem-model/\*.a and \*.elf, else no mem-model folders are archived.
 * If Auto Include Simple Libraries is checked in Properties->General add included libraries to zip.
 * If leave zip folder option is checked, don't remove the intermediate zip folder.
 */
void MainSpinWindow::zipProject()
{
    compileStatus->setPlainText("");
    if(projectFile.isEmpty() || projectModel == NULL) {
        QMessageBox::critical(this, tr("Empty Project"),
            tr("Can't archive an empty project.")+"\n"+
            tr("Please open a project and try again."));
        return;
    }

    QString projFolder(sourcePath(projectFile));
    QString projFile = projectFile;

    /*
     * 1. function assumes an empty projectFolder parameter means to copy existing project.
     * if projectFolder is empty saveAs from existing project.
     */
    if(projFolder.length() == 0) {
        projFolder = sourcePath(projectFile);
    }
    if(projFile.length() == 0) {
        projFile = projectFile;
    }

    if(projFolder.length() == 0) projFolder = ".";

    if(projFile.length() == 0) {
        QMessageBox::critical(
                this,tr("No Project"),
                tr("Can't \"Zip Project\" from an empty project.")+"\n"+
                tr("Please create a new project or open an existing one."),
                QMessageBox::Ok);
        return;
    }

    QString dstName;
    QString srcPath  = projFolder;
    QDir spath(srcPath);

    if(spath.exists() == false) {
        QMessageBox::critical(
                this,tr("Project Folder not Found."),
                tr("Can't \"Zip Project\" from a non-existing folder."),
                QMessageBox::Ok);
        return;
    }

    /*
     * 2. asks user for destination folder ... use existing project name
     * get project name
     */
    QString dstPath;
    QString dstProjFile = this->shortFileName(projFile);

    dstName = projFile.mid(0,projFile.lastIndexOf("."));

    QFileDialog dialog;
    QString afilter("Zip File (*.zip)");
    //QString lpath = this->sourcePath(projectFile)+"../";

    int num = 0;
    QString serial("");
    if(QFile::exists(dstName+serial+".zip")) {
        do {
            num++;
            serial = QString("(%1)").arg(num);
        } while(QFile::exists(dstName+serial+".zip"));
    }

    //dstPath = dialog.getExistingDirectory(this, tr("Zip Project Folder"), lpath, QFileDialog::ShowDirsOnly);
    dstPath = dialog.getSaveFileName(this, tr("Zip Project"), dstName+serial+".zip", afilter);

    if(dstPath.length() < 1)
       return;

    //dstName = dstPath.mid(dstPath.lastIndexOf("/")+1);
    dstName = this->shortFileName(dstPath);
    dstName = dstName.mid(0,dstName.lastIndexOf("."));
    dstPath = dstPath.mid(0,dstPath.lastIndexOf("/")+1);

    //lastPath = dstPath;

    /**
     * Get permission to overwrite folder
     * We did this when asking to over-write zip.
    if(QFile::exists(dstPath+dstName)) {
        int rc =
        QMessageBox::question(this, tr("Archive Folder Already Exists"),
            tr("Over-write existing folder?")+"\n"+dstPath+dstName,
            QMessageBox::Yes, QMessageBox::No);

        if(rc == QMessageBox::No) {
                return;
        }
    }
     */

    /*
     * 3. create a new archive project folder
     */
    dstPath += dstName;

    /*
     * Don't allow users to zip to the source
     * because it would destroy the source.
     */
    if(dstPath.compare(srcPath) == 0) {
        QMessageBox::critical(this, tr("Bad Zip Name"),
            tr("Can't use the source name as the Zip name.")+"\n"+
            tr("Please choose a different name."));
        return;
    }

    /*
     * get the temp path for making zip folder.
     */
    QString dTmpPath = dstPath;
    if(dTmpPath.at(dTmpPath.length()-1) == '/')
        dTmpPath = dTmpPath.left(dTmpPath.length()-1);
    if(dTmpPath.length() == 0)
        return; // should never happen
    dTmpPath = dTmpPath.mid(dTmpPath.lastIndexOf("/"));
    if(dTmpPath.length() == 0)
        return; // should never happen
    dTmpPath = QDir::tempPath() + dTmpPath + "/";

    QDir dpath(dTmpPath);

    if(dpath.exists()) {
        Directory::recursiveRemoveDir(dTmpPath);
    }
    QApplication::processEvents();
    dpath.mkdir(dTmpPath);

    if(dpath.exists() == false) {
        QMessageBox::critical(
                this,tr("Zip Project Error."),
                tr("System can not create project in ")+dTmpPath,
                QMessageBox::Ok);
        return;
    }

    /*
     * 4. copy project file from source to destination as new project name
     */

    // copy project file
    dstProjFile = dTmpPath+dstName+SIDE_EXTENSION;
    QFile::copy(projFile,dstProjFile);

    QString projSrc;
    QFile sproj(projFile);
    if(sproj.open(QFile::ReadOnly | QFile::Text)) {
        projSrc = sproj.readAll();
        sproj.close();
    }

    if(projSrc.length() < 1) {
        QMessageBox::critical(
                this,tr("Zip Project Error."),
                tr("The project is empty. ")+dstProjFile,
                QMessageBox::Ok);
        return;
    }

    /*
     * 5. copies the project main file from source to destination as new project main file
     */
    QStringList projList = projSrc.split("\n", QString::SkipEmptyParts);
    QString srcMainFile = sourcePath(projFile)+projList.at(0);
    QString dstMainFile = projList.at(0);
    dstMainFile = dstMainFile.trimmed();
    QString dstMainExt = dstMainFile.mid(dstMainFile.lastIndexOf("."));
    dstMainFile = dTmpPath+dstName+dstMainExt;

    // remove new file before copy or copy will fail
    if(QFile::exists(dstMainFile))
        QFile::remove(dstMainFile);

    // copy project main file
    QFile::copy(srcMainFile,dstMainFile);

    /*
     * Make a new project list. preprend project main file after this.
     * Copy libraries to this folder as ./library/\*
     * Fix up any links in project file and writes file.
     */

    QStringList newList;

    if(this->isCProject()) {
        newList = zipCproject(projList, srcPath, projFile, dTmpPath, dstProjFile);
    }
#ifdef SPIN
    else if(this->isSpinProject()) {
        newList = zipSPINproject(projList, srcPath, projFile, dTmpPath, dstProjFile);
    }
#endif

    if(newList.isEmpty()) {
        return;
    }

    projSrc = dstName+dstMainExt+"\n";

    for(int n = 0; n < newList.length(); n++) {
        if(newList.at(n).compare(dstName+dstMainExt) != 0)
            projSrc+=newList.at(n)+"\n";
    }

    QFile dproj(dstProjFile);
    if(dproj.open(QFile::WriteOnly | QFile::Text)) {
        dproj.write(projSrc.toLatin1());
        dproj.close();
    }

    // zip folder
    if(this->isCProject()) {
        Zipper zip;
        this->compileStatus->setPlainText(tr("Archiving Project: ")+dstName+".zip");

        QStringList fileList = zip.directoryTreeList(dTmpPath);
        for(int n = 0; n < fileList.length(); n++) {
            if(fileList.at(n).indexOf(">") != 0)
                compileStatus->appendPlainText(tr("Adding ") + fileList.at(n));
        }
        zip.zipFileList(dTmpPath, fileList, dstPath+".zip");
    }
#ifdef SPIN
    else if(this->isSpinProject()) {
        zipIt(dTmpPath, dTmpPath);
    }
#endif
    if(dstPath.endsWith("/"))
        dstPath = dstPath.left(dstPath.lastIndexOf("/"));
    if(dTmpPath.endsWith("/"))
        dTmpPath = dTmpPath.left(dTmpPath.lastIndexOf("/"));
    //QFile::copy(dTmpPath+".zip", dstPath+".zip");
    QFile::remove(dTmpPath+".zip");

#ifdef ENABLE_KEEP_ZIP_FOLDER
    // and remove folder if save zip folder is not checked
    if(propDialog->getKeepZipFolder() == false) {
        QDir dst;
        if(dst.exists(dstPath))
            Directory::recursiveRemoveDir(dTmpPath);
    }
#else
    // and remove folder
    Directory::recursiveRemoveDir(dTmpPath);
#endif
}


void MainSpinWindow::zipIt(QString dir, QString dst)
{
#ifndef USE_QUAZIP

    dir = QDir::fromNativeSeparators(dir);
    if(dir.length() > 0 && dir.at(dir.length()-1) == '/')
        dir = dir.left(dir.length()-1);
    if(dst.length() > 0 && dst.at(dst.length()-1) == '/')
        dst = dst.left(dst.length()-1);
    QString folder = dst.mid(dst.lastIndexOf('/')+1);
    //dir = dir.mid(0,dir.lastIndexOf('/'));
    QString zipProgram("zip");
    if(QDir::separator() == '\\')
        zipProgram += ".exe";
    QFile::remove(dst+".zip");
    this->compileStatus->setPlainText(tr("Archiving Project: ")+dst+".zip");


    int n = this->editorTabs->currentIndex();
    QString fileName = editorTabs->tabToolTip(n);
    if (fileName.isEmpty()) {
        return;
    }
    QString spinLibPath; //     = propDialog->getSpinLibraryString();
    QStringList fileTree    = spinParser.spinFileTree(fileName, spinLibPath);
    if(fileTree.count() > 0) {
        zipper.makeSpinZip(fileName, fileTree, spinLibPath, statusDialog);
    }

#else
    dir = QDir::fromNativeSeparators(dir);
    if(dir.length() > 0 && dir.at(dir.length()-1) == '/')
        dir = dir.left(dir.length()-1);
    if(dst.length() > 0 && dst.at(dst.length()-1) == '/')
        dst = dst.left(dst.length()-1);
    QString folder = dst.mid(dst.lastIndexOf('/')+1);
    //dir = dir.mid(0,dir.lastIndexOf('/'));
    QString zipProgram("zip");
    if(QDir::separator() == '\\')
        zipProgram += ".exe";
    QFile::remove(dst+".zip");
    this->compileStatus->setPlainText(tr("Archiving Project: ")+dst+".zip");

#ifdef USE_ZIP_APP

    QStringList args;
    args.append(folder);
    args.append("-r");
    args.append(folder);
    builder->startProgram(zipProgram, dir, args, Build::DumpOff);

#else

    QuaZip zip(dst+".zip");
    if(!zip.open(QuaZip::mdCreate)) {
        QMessageBox::critical(this,tr("Can't Save Zip"), tr("Can't open .zip file to write: ")+folder+".zip");
        return;
    }
    QFile inFile;
    QStringList fileList;
    recursiveAddDir(dir, &fileList);

    QFileInfoList files;
    foreach (QString fn, fileList)
        files << QFileInfo(fn);

    QuaZipFile outFile(&zip, this);

    char c;
    foreach(QFileInfo fileInfo, files) {

        if (!fileInfo.isFile())
            continue;

        QString fileNameWithRelativePath = fileInfo.filePath();
        fileNameWithRelativePath = fileNameWithRelativePath.mid(dir.length()+1);

        compileStatus->appendPlainText(tr("Adding ") + fileNameWithRelativePath);
        inFile.setFileName(fileInfo.filePath());

        if (!inFile.open(QIODevice::ReadOnly)) {
            QMessageBox::critical(this, tr("Zip Error"), QString("testCreate(): inFile.open(): %1").arg(inFile.errorString().toLocal8Bit().constData()));
            return;
        }

        if (!outFile.open(QIODevice::WriteOnly, QuaZipNewInfo(fileNameWithRelativePath, fileInfo.filePath()))) {
            QMessageBox::critical(this, tr("Zip Error"), QString("testCreate(): outFile.open(): %1").arg(outFile.getZipError()));
            return;
        }

        while (inFile.getChar(&c) && outFile.putChar(c))
            ; // yes empty body

        QApplication::processEvents();

        if (outFile.getZipError() != UNZ_OK) {
            QMessageBox::critical(this, tr("Zip Error"), QString("testCreate(): outFile.putChar(): %1").arg(outFile.getZipError()));
            return;
        }

        outFile.close();

        if (outFile.getZipError() != UNZ_OK) {
            QMessageBox::critical(this, tr("Zip Error"), QString("testCreate(): outFile.close(): %1").arg(outFile.getZipError()));
            return;
        }

        inFile.close();
    }

    zip.close();


    if (zip.getZipError() != 0) {
        QMessageBox::critical(this, tr("Zip Error"), QString("testCreate(): zip.close(): %1").arg(zip.getZipError()));
        return;
    }

#endif

    compileStatus->appendPlainText(tr("Done."));
#endif
}

QStringList MainSpinWindow::zipCproject(QStringList projList, QString srcPath, QString projFile, QString dstPath, QString dstProjFile, QString trOpType)
{
    /*
     * Make a library directory only if necessary.
     */
    QString zipLib("library/");

    // WORK IN PROGRESS
    // need to call gcc -M to find other includes.

#ifdef ENABLE_AUTOLIB
    QString linkopts;
    QStringList libs;
    if(this->propDialog->getAutoLib()) {
        QStringList libadd;
        QStringList addList;
        /*
         * Get all related libraries and add them to the project list.
         * The zip as it is flattens the libraries, but Simple Libraries are not flat.
         * So we have a choice of using the Simple Library hierarchy or fixing the library projects.
         * It is impractical to use the hierarchy because of the way the zip code works.
         * Changing the zip code is a huge risk. Flattening the projects is a one line zip code change.
         */
        QDir path(srcPath);
        libadd = buildC->getLibraryList(addList,projFile);
        foreach(QString s, libadd) {
            s = path.relativeFilePath(s);
            QString tmps = s;
            tmps = tmps.remove("../");
            if(projList.contains(tmps) == false) {
                projList.append("-I "+s);
                projList.append("-L "+s);
            }

            s = s.mid(s.lastIndexOf("/")+1);

            if(s.indexOf("lib") == 0) {
                s = s.mid(3);
                s = "-l"+s;
                if(libs.contains(s) == false)
                    libs.append(s);
            }
        }

        linkopts = projectOptions->getLinkOptions();
        /* append libs final compile optimizes */
        for(int n = libs.count()-1; n > -1; n--) {
            linkopts += " " + libs[n];
        }
        linkopts = linkopts.trimmed();
        for(int n = 1; n < projList.length(); n++) {
            QString item = projList.at(n);
            if(item.contains(">linker::")) {
                linkopts += item;
                projList.removeAt(n);
            }
        }
        projList.append(">linker::"+linkopts);
    }
#endif

    QStringList newList;
    // start at 1 because main filename has already been copied
    for(int n = 1; n < projList.length(); n++) {
        QString item = projList.at(n);

        // special handling
        // in archive packaging, we need to copy the file
        if(item.indexOf(FILELINK) > 0) {
            QStringList list = item.split(FILELINK,QString::SkipEmptyParts);
            if(list.length() < 2) {
                QMessageBox::information(
                        this,tr("Project File Error."),
                        trOpType+tr(" Project expected a link, but got:\n")+item+"\n\n"+
                        tr("Please manually adjust it by adding a correct link in the Project Manager list.")+" "+
                        tr("After adding the correct link, remove the bad link."));
            }
            else {
                // list item 0 contains basic file name. item 2 contains link.
                // prepend link with sourcePath(projectFile) and basic file with destination
                QString als = QDir::fromNativeSeparators(sourcePath(projectFile)+list[1]);
                QString dst = QDir::fromNativeSeparators(dstPath+list[0]);
                QFile::copy(als, dst);
                if(QFile::exists(dst)) {
                    item = list[0];
                }
                else
                    QMessageBox::information(
                            this,tr("Can't Fix Link"),
                            trOpType+tr(" Project was not able to fix the link:\n")+item+"\n\n"+
                            tr("Please manually adjust it by adding the correct link in the Project Manager list.")+" "+
                            tr("After adding the correct link, remove the bad link."));
                newList.append(item);
            }
        }
        else if(item.indexOf("-I") == 0) {
            QStringList list = item.split("-I",QString::SkipEmptyParts);
            if(list.length() < 1) {
                QMessageBox::information(
                        this,tr("Project File Error."),
                        trOpType+tr(" Project expected a -I link, but got:\n")+item+"\n\n"+
                        tr("Please manually adjust it by adding a correct link in the Project Manager list.")+" "+
                        tr("After adding the correct link, remove the bad link."));
            }
            else {
                QString als = list[0];
                als = als.trimmed();
                als = QDir::fromNativeSeparators(als);
                if(als.endsWith("/"))
                    als = als.mid(0,als.length()-1);
                QString dls = als.mid(als.lastIndexOf("/")+1);
                als = "./"+zipLib+dls;
                if(als.length() > 0)
                    item = "-I "+als;

                else
                    QMessageBox::information(
                            this,tr("Can't Fix Link"),
                            trOpType+tr(" Project was not able to fix the link:\n")+item+"\n\n"+
                            tr("Please manually adjust it by adding the correct link in the Project Manager list.")+" "+
                            tr("After adding the correct link, remove the bad link."));
                newList.append(item);
            }
        }
        else if(item.indexOf("-L") == 0) {
            QStringList list = item.split("-L",QString::SkipEmptyParts);
            if(list.length() < 1) {
                QMessageBox::information(
                        this,tr("Project File Error."),
                        trOpType+tr(" Project expected a -L link, but got:\n")+item+"\n\n"+
                        tr("Please manually adjust it by adding a correct link in the Project Manager list.")+" "+
                        tr("After adding the correct link, remove the bad link."));
            }
            else {
                QString als = list[0];
                als = als.trimmed();
                als = QDir::fromNativeSeparators(als);
                if(als.endsWith("/"))
                    als = als.mid(0,als.length()-1);
                if(als.indexOf("./") == 0) {
                    als = als.mid(2);
                    als = this->sourcePath(projectFile)+als;
                }
                else if(als.indexOf("../") == 0) {
                    als = this->sourcePath(projectFile)+als;
                }
                else {
                    als = this->sourcePath(projectFile)+als;
                }
                QString dls = als.mid(als.lastIndexOf("/")+1);
                QDir libd;
                if(libd.exists(dstPath+zipLib) == false)
                    libd.mkdir(dstPath+zipLib);
                if(Directory::isPossibleInfiniteFolder(als, dstPath+zipLib+dls)) {
                    QMessageBox::information(
                            this,tr("Invalid Project Structure."),
                            trOpType+tr(" Project structure would result in infinite folder.")+"\n"+
                            dstPath+zipLib+dls + " " + tr("includes") + " " + als +"\n"+
                            tr("Please do not create projects that include projects in folders within projects."));
                    return QStringList();
                }
                Directory::recursiveCopyDir(als, dstPath+zipLib+dls, QString("*.o *.elf"));
                // TODO fix projects to flatten here ?
                if(dls.endsWith(".h"))
                    continue;
                flattenDstProject(dstPath+zipLib+dls, dls+".side");
                als = "./"+zipLib+dls;
                if(als.length() > 0)
                    item = "-L "+als;
                else
                    QMessageBox::information(
                            this,tr("Can't Fix Link"),
                            trOpType+tr(" Project was not able to fix the link:\n")+item+"\n\n"+
                            tr("Please manually adjust it by adding the correct link in the Project Manager list.")+" "+
                            tr("After adding the correct link, remove the bad link."));
                newList.append(item);
            }
        }
        // project options
        else if(item[0] == '>') {
            newList.append(item);
        }
        // different destination
        else if(sourcePath(projFile) != sourcePath(dstProjFile)) {
            newList.append(item);
            QString dst = sourcePath(dstProjFile)+item;
            if(QFile::exists(dst))
                QFile::remove(dst);
            QFile::copy(sourcePath(projFile)+item, dst);
        }
        // same destination, just copy item
        else {
            newList.append(item);
        }
    }

    if(projectOptions->getMakeLibrary().isEmpty() == false) {
        QDir dir(srcPath);
        QStringList models = projectOptions->getMemModelList();
        foreach(QString model, models) {
            if(model.length() > 0) {
                model = model.toLower();
                if(dir.exists(srcPath+model)) {
                    dir.mkdir(dstPath+model);
                    if(Directory::isPossibleInfiniteFolder(srcPath+model,dstPath+model)) {
                        QMessageBox::information(
                                this,tr("Invalid Project Structure."),
                                trOpType+tr(" Project structure would result in infinite folder.")+"\n"+
                                dstPath+model + " " + tr("includes") + " " + srcPath+model +"\n"+
                                tr("Please do not create projects that include projects in folders within projects."));
                        return QStringList();
                    }
                    Directory::recursiveCopyDir(srcPath+model,dstPath+model,QString("*.o *.elf"));
                    //Directory::recursiveRemoveDir(dstPath+model);
                }
            }
        }
    }

    newList.sort();
    return newList;
}

QStringList MainSpinWindow::zipSPINproject(QStringList projList, QString srcPath, QString projFile, QString dstPath, QString dstProjFile)
{
    QStringList newList;
    //QMessageBox::information(this, tr("Feature Not Ready."), tr("The SPIN zip/archive feature is not available yet."));
    QString fileName = projList.at(0);
    if(fileName.isEmpty())
        return newList;
    if(fileName.endsWith(".spin", Qt::CaseInsensitive) == false) {
        QMessageBox::critical(this, tr("Not a SPIN project."), tr("The main file must be a SPIN file in a SPIN project."));
        return newList;
    }
    QString spinlib;
    spinlib = propDialog->getSpinLibraryStr();

    newList = spinParser.spinFileTree(fileName, spinlib);
    // start at 1 because main filename has already been copied
    for(int n = 1; n < newList.count(); n ++) {
        QString item = newList[n];
        qDebug() << srcPath+item;

        item = findFileNoCase(srcPath+item);
        if(QFile::exists(item)) {
            // ok, copy source
            if(item.indexOf("/") > -1)
                item = item.mid(item.lastIndexOf("/")+1);
            QFile::copy(srcPath+item, dstPath+item);
        }
        else {
            // find in spin library
            item = findFileNoCase(spinlib+newList[n]);
            if(QFile::exists(item)) {
                if(item.indexOf("/") > -1)
                    item = item.mid(item.lastIndexOf("/")+1);
                QFile::copy(spinlib+item, dstPath+item);
            }
        }
    }
    newList.append(">compiler=spin");
    return newList;
}

/*
 * spin causes case problems and other grief
 */
QString MainSpinWindow::findFileNoCase(QString file)
{
    QString fileret = file;
    if(file.isEmpty())
        return fileret;
    if(QFile::exists(fileret))
        return fileret;
    if(file.lastIndexOf("/") == file.length())
        file = file.mid(file.length()-1);
    if(file.indexOf("/") < 0)
        return fileret;
    QString path = file.mid(0,file.lastIndexOf("/")+1);
    QDir dir(path);
    QStringList list = dir.entryList(QDir::AllEntries);
    file = file.mid(file.lastIndexOf("/")+1);
    foreach(QString item, list) {
        if(file.compare(item, Qt::CaseInsensitive) == 0) {
            return path+item;
        }
    }
    return fileret; // no match, just return file
}

void MainSpinWindow::openRecentProject()
{
    QAction *action = qobject_cast<QAction *>(sender());
    QVariant qv = action->data();
    if(qv.isNull()) { // Was qv == NULL. Thanks David Betz.
        return;
    }
    if(qv.canConvert(QVariant::String) == false) {
        return;
    }
    QString data = qv.toString();
    if (action != NULL && data != NULL) {
        closeProject();
        openProject(data);
    }
}


void MainSpinWindow::setCurrentFile(const QString &fileName)
{
    if(this->simpleViewType) {
        // don't show files in simpleview
        return;
    }

    QStringList files = settings->value(recentFilesKey).toStringList();
    files.removeAll(fileName);
    files.prepend(fileName);
    while (files.size() > MaxRecentFiles)
        files.removeLast();

    settings->setValue(recentFilesKey, files);

    foreach (QWidget *widget, QApplication::topLevelWidgets()) {
#ifdef SPINSIDE
        MainSpinWindow *mainWin = qobject_cast<MainSpinWindow *>(widget);
#else
        MainWindow *mainWin = qobject_cast<MainWindow *>(widget);
#endif
        if (mainWin)
            mainWin->updateRecentFileActions();
    }
}

void MainSpinWindow::updateRecentFileActions()
{
    if(this->simpleViewType) {
        // don't show files in simpleview
        return;
    }

    QStringList files = settings->value(recentFilesKey).toStringList();

    int numRecentFiles = qMin(files.size(), (int)MaxRecentFiles);

    for (int i = 0; i < numRecentFiles; ++i) {
        QString estr = files.at(i);
        if(estr.length() == 0)
            continue;
        //QString filename = QFileInfo(projects[i]).fileName();
        QString text = tr("&%1 %2").arg(i + 1).arg(estr);
        recentFileActs[i]->setText(text);
        recentFileActs[i]->setData(estr);
        recentFileActs[i]->setVisible(true);
    }
    for (int j = numRecentFiles; j < MaxRecentFiles; ++j)
        recentFileActs[j]->setVisible(false);

    separatorFileAct->setVisible(numRecentFiles > 0);
}

void MainSpinWindow::openRecentFile()
{
    QAction *action = qobject_cast<QAction *>(sender());
    if (action)
        openFile(action->data().toString());
}


void MainSpinWindow::setCurrentProject(const QString &fileName)
{
    projectFile = fileName;

    QStringList files = settings->value(recentProjectsKey).toStringList();
    files.removeAll(fileName);
    files.prepend(fileName);
    while (files.size() > MaxRecentProjects)
        files.removeLast();

    settings->setValue(recentProjectsKey, files);

    foreach (QWidget *widget, QApplication::topLevelWidgets()) {
#ifdef SPINSIDE
        MainSpinWindow *mainWin = qobject_cast<MainSpinWindow *>(widget);
#else
        MainWindow *mainWin = qobject_cast<MainWindow *>(widget);
#endif
        if (mainWin) {
            mainWin->updateRecentProjectActions();
            break;
        }
    }
}

void MainSpinWindow::updateRecentProjectActions()
{
    QStringList projects = settings->value(recentProjectsKey).toStringList();

    int numRecentProjects = qMin(projects.size(), (int)MaxRecentProjects);

    for (int i = 0; i < numRecentProjects; ++i) {
        QString estr = projects.at(i);
        if(estr.length() == 0)
            continue;
        QString text = tr("&%1 %2").arg(i + 1).arg(estr);
        recentProjectActs[i]->setText(text);
        recentProjectActs[i]->setData(estr);
        recentProjectActs[i]->setVisible(true);
    }
    for (int j = numRecentProjects; j < MaxRecentProjects; ++j)
        recentProjectActs[j]->setVisible(false);

    separatorProjectAct->setVisible(numRecentProjects > 0);
}

void MainSpinWindow::saveFile()
{
    bool saveas = false;
    int n = this->editorTabs->currentIndex();
    QString fileName = editorTabs->tabToolTip(n);
    QString data = editors->at(n)->toPlainText();
    if(fileName.isEmpty()) {
        fileName = fileDialog.getSaveFileName(this, tr("Save As File"), lastPath, tr(SOURCE_FILE_TYPES));
        saveas = true;
    }
    if (fileName.isEmpty())
        return;
    if(fileName.length() > 0)
        lastPath = sourcePath(fileName);
    editorTabs->setTabText(n,shortFileName(fileName));
    editorTabs->setTabToolTip(n,fileName);
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        QTextStream os(&file);
        os.setCodec("UTF-8"); // for now save everything as UTF-8
        if (file.open(QFile::WriteOnly)) {
            os << data;
            file.close();
        }
        if(saveas) {
            this->closeTab(n);
            this->openFileName(fileName);
        }
    }
    saveProjectOptions();
    openProjectFileMatch(fileName);
}

void MainSpinWindow::saveEditor()
{
    saveFile();
}

void MainSpinWindow::saveFileByTabIndex(int tab)
{
    QString fileName = editorTabs->tabToolTip(tab);
    QString data = editors->at(tab)->toPlainText();
    if(fileName.length() < 1) {
        qDebug() << "saveFileByTabIndex filename invalid tooltip " << tab;
        return;
    }
    editorTabs->setTabText(tab,shortFileName(fileName));
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QFile::WriteOnly)) {
            file.write(data.toUtf8());
            file.close();
        }
    }
}

QStringList MainSpinWindow::getAsFilters()
{
    QStringList filters;

    filters << "C File (*.c)";
    filters << "C++ File (*.cpp)";
// #ifdef SPIN // always allow spin filters
    filters << "Spin File (*.spin)";
// #endif
    filters << "C Header File (*.h)";
    filters << "COG C File (*.cogc)";

    if(this->simpleViewType == false) {
        filters << "E-COG C File (*.ecogc)";
// #ifdef SPIN // always allow spin filters
        filters << "E-Spin File (*.espin)";
// #endif
    }

    QString comp = projectOptions->getCompiler();
    if(comp.compare(projectOptions->CPP_COMPILER) == 0) {
        filters.removeAt(1);
        filters.insert(0,"C++ File (*.cpp)");
    }
// #ifdef SPIN // always allow spin filters
    else if(comp.compare(projectOptions->SPIN_COMPILER) == 0) {
        filters.removeAt(2);
        filters.insert(0,"Spin File (*.spin)");
    }
// #endif

    filters << "Any File (*)";

    return filters;
}

QString MainSpinWindow::filterAsFilename(QString name, QString filter)
{
    QString filtype = filter;
    filtype = filtype.mid(filtype.lastIndexOf("(")+1);
    filtype = filtype.mid(0,filtype.lastIndexOf(")"));
    filtype = filtype.trimmed();
    if(filtype[0] == '*') filtype = filtype.mid(1);

    if(name.endsWith(filtype) != true && name.indexOf(".") > -1)
        return name;

    QString dstName = name.mid(name.lastIndexOf("/")+1);
    dstName = dstName.mid(0,dstName.lastIndexOf("."));
    QString dstPath = name.mid(0,name.lastIndexOf("/")+1);

    if(filter.indexOf(".") > -1) {
        filter = filter.mid(filter.lastIndexOf("."));
        filter = filter.mid(0,filter.lastIndexOf(")"));
    }
    else {
        filter = "";
    }

    name = dstPath + dstName + filter;
    return name;
}

QString MainSpinWindow::getSaveAsFile(const QString &path)
{
    int n = this->editorTabs->currentIndex();
    //QString data = editors->at(n)->toPlainText();
    QString fileName = path;

    if (fileName.isEmpty()) {

        // simpleView version
        if(this->simpleViewType) {
            QFileDialog dialog(this,tr("Save As File"), lastPath, "");
            QStringList filters = getAsFilters();

            dialog.setNameFilters(filters);
            dialog.setAcceptMode(QFileDialog::AcceptSave);
            if(dialog.exec() == QDialog::Rejected)
                return QString("");
            QStringList dstList = dialog.selectedFiles();
            if(dstList.length() < 1)
                return QString("");
            QString dstPath = dstList.at(0);
            dstPath = dstPath.trimmed();
            if(dstPath.length() < 1)
                return QString("");

            QString ftype = dialog.selectedNameFilter();
            fileName = filterAsFilename(dstPath, ftype);
            fileName = fileName.trimmed();
            dstPath = dstPath.mid(0,dstPath.lastIndexOf("/")+1);
            lastPath = dstPath;
        }
        else {
            fileName = fileDialog.getSaveFileName(this,
                          tr("Save As File"), lastPath, tr(SOURCE_FILE_TYPES));
            fileName = fileName.trimmed();
        }
    }
    if(fileName.length() > 0)
        lastPath = sourcePath(fileName);

    if (fileName.isEmpty())
        return QString("");

    this->editorTabs->setTabText(n,shortFileName(fileName));
    editorTabs->setTabToolTip(n,fileName);

    return fileName;
}

void MainSpinWindow::saveAsFile(const QString &path)
{
    QString fileName = getSaveAsFile(path);
    int n = this->editorTabs->currentIndex();
    QString data = editors->at(n)->toPlainText();

    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QFile::WriteOnly)) {
            file.write(data.toUtf8());
            file.close();
        }
        // close and reopen to make sure syntax highlighting works.
        this->closeTab(n);
        this->openFileName(fileName);
        setCurrentFile(fileName);
    }
}


QString MainSpinWindow::getOpenAsFile(const QString &path)
{
    QString fileName;

    QFileDialog dialog(this,tr("Open File"), path, "");
    dialog.setConfirmOverwrite(false);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);

    QStringList filters = getAsFilters();

    dialog.setNameFilters(filters);
    if(dialog.exec() == QDialog::Rejected)
        return QString("");

    QStringList dstList = dialog.selectedFiles();
    if(dstList.length() < 1)
        return QString("");
    QString dstPath = dstList.at(0);
    if(dstPath.length() < 1)
        return QString("");

    QString ftype = dialog.selectedNameFilter();
    fileName = filterAsFilename(dstPath, ftype);

    dstPath = dstPath.mid(0,dstPath.lastIndexOf("/")+1);
    lastPath = dstPath;

    if(fileName.length() > 0)
        lastPath = sourcePath(fileName);

    if (fileName.isEmpty())
        return QString("");

    return fileName;
}

void MainSpinWindow::downloadSdCard()
{
#ifdef ENABLE_FILETO_SDCARD
    if(projectModel == NULL || projectFile.isNull()) {
        QMessageBox mbox(QMessageBox::Critical, tr("Error No Project"),
            "Please select a tab and press F4 to set main project file.", QMessageBox::Ok);
        mbox.exec();
        return;
    }

    if (runBuild(""))
        return;

    QString fileName = fileDialog.getOpenFileName(this, tr("Send File"), sourcePath(projectFile), "Any File (*)");
    if(fileName.length() > 0)
        lastPath = sourcePath(fileName);

    if (fileName.isEmpty())
        return;

    btnConnected->setChecked(false);
    portListener->close(); // disconnect uart before use
    //term->hide();
    term->setPortEnabled(false);
    cbPort->setEnabled(true);

    progress->show();
    progress->setValue(0);
    status->setText("");

    getApplicationSettings();

    // don't add fileName here since it can have spaces
    QStringList args = getLoaderParameters("", "");

    portName = serialPort();
    if(portName.compare(AUTO_PORT) == 0) {
        compileStatus->appendPlainText("Propeller not found on any port.");
        return;
    }
    if (getWxPortIpAddr(portName).length()) {
        args.append("-i");
        args.append(getWxPortIpAddr(portName));
    } else {
        args.append("-p");
        args.append(portName);
    }

    QString s = projectOptions->getMemModel();
    if(s.contains(" "))
        s = s.mid(0,s.indexOf(" "));
    s += "/"+this->shortFileName(projectFile);
    s = s.mid(0,s.lastIndexOf(".side"));
    s += ".elf";
    builder->removeArg(args, s);
    s = QDir::toNativeSeparators(s);
    builder->removeArg(args, s);

    //QString s = QDir::toNativeSeparators(fileName);
    args.append("-f");
    args.append(this->shortFileName(fileName));

    btnConnected->setChecked(false);
    portListener->close(); // disconnect uart before use

    builder->showBuildStart(aSideLoader,args);


    process->setProperty("Name", QVariant(aSideLoader));
    process->setProperty("IsLoader", QVariant(true));

    connect(process, SIGNAL(readyReadStandardOutput()),this,SLOT(procReadyRead()));
    connect(process, SIGNAL(finished(int,QProcess::ExitStatus)),this,SLOT(procFinished(int,QProcess::ExitStatus)));
    connect(process, SIGNAL(error(QProcess::ProcessError)), this, SLOT(procError(QProcess::ProcessError)));

    process->setProcessChannelMode(QProcess::MergedChannels);
    process->setWorkingDirectory(sourcePath(fileName));

    procMutex.lock();
    procDone = false;
    procMutex.unlock();

    process->start(aSideLoader,args);

    status->setText(status->text()+tr(" Loading ... "));

    while(procDone == false)
        QApplication::processEvents();

    if(process->state() == QProcess::Running) {
        process->kill();
        Sleeper::ms(500);
        compileStatus->appendPlainText(tr("File to SD Card killed by user."));
        status->setText(status->text() + tr(" Done."));
    }
#endif
}

void MainSpinWindow::procError(QProcess::ProcessError error)
{
    QVariant name = process->property("Name");
    compileStatus->appendPlainText(name.toString() + tr(" error ... (%1)").arg(error));
    compileStatus->appendPlainText(process->readAllStandardOutput());
    procMutex.lock();
    procDone = true;
    procMutex.unlock();
}

void MainSpinWindow::procFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if(procDone == true) return;

    procMutex.lock();
    procDone = true;
    procMutex.unlock();

    QVariant name = process->property("Name");
    builder->buildResult(exitStatus, exitCode, name.toString(), process->readAllStandardOutput());

    int len = status->text().length();
    QString s = status->text().mid(len-8);
    if(s.contains("done.",Qt::CaseInsensitive) == false)
        status->setText(status->text()+" done.");
}

void MainSpinWindow::wxProcReadyLoad(void)
{
    QString str = wxProcess->readAllStandardOutput();
    compileStatus->appendPlainText(str);

    qDebug() << "wxProcReadyLoad" << str;

    if (str.length() == 0) {
        return;
    }

    wxPortString += str;
}

void MainSpinWindow::wxProcReadyRead(void)
{
    QVariant namev = wxProcess->property("Name");
    QString name = namev.toString();
    QString str;

    if(procDone == true) return;

    qDebug() << "wxProcReadyRead" << str;

    if (name.compare(aSideLoader) == 0) {
        wxProcReadyLoad();
    }
    else {
        str = wxProcess->readAllStandardOutput();
        compileStatus->appendPlainText(str);
    }
}

void MainSpinWindow::wxProcError(QProcess::ProcessError error)
{
    QVariant name = wxProcess->property("Name");

    if(procDone == true) return;

    qDebug() << "wxProcError" << error;

    compileStatus->appendPlainText(name.toString() + tr(" error ... (%1)").arg(error));
    compileStatus->appendPlainText(wxProcess->readAllStandardOutput());

    procMutex.lock();
    procDone = true;
    procMutex.unlock();
}

#ifdef ESP8266_MODULE
void MainSpinWindow::wxProcFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if(procDone == true)
        return;

    qDebug() << "wxProcFinished" << wxPortString << exitCode << exitStatus;

    QString str = wxPortString;

    if (!str.contains("Name:",Qt::CaseInsensitive)) {
        procMutex.lock();
        procDone = true;
        procMutex.unlock();
        return;
    }

    QStringList ms = str.split("\n", QString::SkipEmptyParts, Qt::CaseInsensitive);
    QRegExp ipre("IP:");
    QRegExp idre("Name:");
    QRegExp mcre("MAC:");
    QStringList fs;

    foreach (QString mod, ms) {
        WxPortInfo info;
        info.ipAddr = "";
        info.macUpper = "";
        info.macAddr = "";
        info.portName = "";
        info.VendorName = "";

        if (mod.contains(ipre) && mod.contains(idre)) {
            QStringList sl = mod.split(",", QString::SkipEmptyParts);
            foreach (QString s, sl) {
                if (s.contains(ipre)) {
                    fs = s.split(":", QString::SkipEmptyParts);
                    if (fs.length() > 0) {
                        info.ipAddr = fs[1].trimmed();
                    }
                }
                else if (s.contains(idre)) {
                    fs = s.split(":", QString::SkipEmptyParts);
                    if (fs.length() > 0) {
                        fs[1] = fs[1].toUpper();
                        info.portName = fs[1].trimmed();
                        while (info.portName.contains("'")) {
                            info.portName = info.portName.replace("'","");
                        }
                        info.portName = info.portName.trimmed();

                        /*
                        if (info.portName.length() == 0) {
                            info.portName = "X-"+info.macUpper+"-"+info.macAddr;
                        }
                        */
                    }
                }
                else if (s.contains(mcre)) {
                    fs = s.split(":", QString::SkipEmptyParts);
                    if (fs.length() > 0) {
                        fs[1] = fs[1].toUpper();
                        info.macUpper = fs[1].trimmed()+fs[2].trimmed()+fs[3].trimmed();
                        info.macAddr  = fs[4].trimmed()+fs[5].trimmed()+fs[6].trimmed();
                    }
                }
            }
        }
        if (info.ipAddr.length()) {

            if (info.portName.length() == 0) {
                info.portName = "X-"+info.macAddr;
            }
            QString name = info.portName;
            foreach (WxPortInfo myinfo, wxPorts) {
                if (name.compare(myinfo.portName, Qt::CaseInsensitive) == 0) {
                    name += "-" + info.ipAddr;
                    info.portName = name;
                }
            }
            wxPorts.append(info);
        }
#ifdef REMOVE
        if (info.ipAddr.length() &&
            info.macAddr.length() &&
            info.macUpper.length()) {

            if (info.portName.length() == 0) {
                //info.portName = "X-"+info.portName;
                QString macser = info.macAddr.mid(10).trimmed();
                if (info.macUpper.compare("00409d") == 0) { // One Digi vendor ID
                    info.VendorName = "Digi";
                    info.portName = "X-"+info.VendorName+"-"+macser;
                }
                else {
                    info.portName = "X-"+info.macUpper+info.macAddr;
                }
            }
            wxPorts.append(info);
        }
#endif
    }

    procMutex.lock();
    procDone = true;
    procMutex.unlock();
}
#endif

#ifdef XBEE_SB6_MODULE
void MainSpinWindow::wxProcFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if(procDone == true)
        return;

    qDebug() << "wxProcFinished" << exitCode << exitStatus;

    QString str = wxPortString;

    if (!str.contains("========",Qt::CaseInsensitive)) {
        procMutex.lock();
        procDone = true;
        procMutex.unlock();
        return;
    }

    QStringList ms = str.split("========", QString::SkipEmptyParts, Qt::CaseInsensitive);
    QRegExp ipre("ipAddr:");
    QRegExp idre("nodeId:");
    QRegExp mcre("macAddr:");
    QStringList fs;

    foreach (QString mod, ms) {
        WxPortInfo info;
        info.ipAddr = "";
        info.macUpper = "";
        info.macAddr = "";
        info.portName = "";
        info.VendorName = "";

        if (mod.contains(ipre) && mod.contains(idre) && mod.contains(mcre)) {
            QStringList sl = mod.split("\n", QString::SkipEmptyParts);
            foreach (QString s, sl) {
                if (s.contains(ipre)) {
                    fs = s.split(":", QString::SkipEmptyParts);
                    if (fs.length() > 0) {
                        info.ipAddr = fs[1].trimmed();
                    }
                }
                else if (s.contains(idre)) {
                    fs = s.split(":", QString::SkipEmptyParts);
                    if (fs.length() > 0) {
                        fs[1] = fs[1].toUpper();
                        info.portName = fs[1].trimmed();
                        while (info.portName.contains("'")) {
                            info.portName = info.portName.replace("'","");
                        }
                        info.portName = info.portName.trimmed();

                        /*
                        if (info.portName.length() == 0) {
                            info.portName = "X-"+info.macUpper+"-"+info.macAddr;
                        }
                        */
                    }
                }
                else if (s.contains(mcre)) {
                    fs = s.split(":", QString::SkipEmptyParts);
                    if (fs.length() > 0) {
                        fs[1] = fs[1].toUpper();
                        info.macUpper = fs[1].trimmed().mid(4,6);
                        info.macAddr = fs[1].trimmed();
                    }
                }
            }
        }
        if (info.ipAddr.length() &&
            info.macAddr.length() &&
            info.macUpper.length()) {

            if (info.portName.length() == 0) {
                //info.portName = "X-"+info.portName;
                QString macser = info.macAddr.mid(10).trimmed();
                if (info.macUpper.compare("00409d") == 0) { // One Digi vendor ID
                    info.VendorName = "Digi";
                    info.portName = "X-"+info.VendorName+"-"+macser;
                }
                else {
                    info.portName = "X-"+info.macUpper+info.macAddr;
                }
            }
            wxPorts.append(info);
        }
    }

    procMutex.lock();
    procDone = true;
    procMutex.unlock();
}
#endif

/*
 * save for cat dumps
 */
void MainSpinWindow::procReadyReadCat()
{

}

/*
 * read program sizes from objdump -h
 */

void MainSpinWindow::procReadyRead()
{
    QByteArray bytes = process->readAllStandardOutput();
    if(bytes.length() == 0)
        return;

#if defined(Q_OS_WIN)
    QString eol("\r");
#else
    QString eol("\n");
#endif

    qDebug() << bytes;
    while (QString(bytes).contains("\r\n"))
        bytes = bytes.replace("\r\n","\n");
    // bstc doesn't return good exit status
    QString progname;
    QVariant pvar = process->property("Name");
    if(pvar.canConvert(QVariant::String)) {
        progname = pvar.toString();
    }
    if(progname.contains("bstc",Qt::CaseInsensitive)) {
        if(QString(bytes).contains("Error",Qt::CaseInsensitive)) {
            procResultError = true;
        }
    }

    if(QString(bytes).contains("error",Qt::CaseInsensitive)) {
         compileStatus->insertPlainText(bytes);
         procResultError = true;
    }

    QStringList lines = QString(bytes).split("\n",QString::SkipEmptyParts);
    if(bytes.contains("bytes")) {
        for (int n = 0; n < lines.length(); n++) {
            QString line = lines[n];
            if(line.length() > 0) {
                if(line.indexOf("\r") > -1) {
                    QStringList more = line.split("\r",QString::SkipEmptyParts);
                    lines.removeAt(n);
                    if(line.contains("Propeller Version",Qt::CaseInsensitive)) {
                        compileStatus->insertPlainText(eol+line);
                        progress->setValue(0);
                    }
                    for(int m = more.length()-1; m > -1; m--) {
                        QString ms = more.at(m);
                        if(ms.contains("bytes",Qt::CaseInsensitive))
                            lines.insert(n,more.at(m));
                        if(ms.contains("loading",Qt::CaseInsensitive))
                            lines.insert(n,more.at(m));
                    }
                }
            }
        }
    }

    for (int n = 0; n < lines.length(); n++) {
        QString line = lines[n];
        if(line.length() > 0) {
            compileStatus->moveCursor(QTextCursor::End);
            if(line.contains("Propeller Version",Qt::CaseInsensitive)) {
                compileStatus->insertPlainText(line+eol);
                progress->setValue(0);
            }
            else
            if(line.contains("loading",Qt::CaseInsensitive)) {
                progMax = 0;
                progress->setValue(0);
                compileStatus->insertPlainText(line+eol);
            }
            else
            if(line.contains("writing",Qt::CaseInsensitive)) {
                progMax = 0;
                progress->setValue(0);
            }
            else
            if(line.contains("Download OK",Qt::CaseInsensitive)) {
                progress->setValue(100);
                compileStatus->insertPlainText(line+eol);
            }
            else
            if(line.contains("sent",Qt::CaseInsensitive)) {
                compileStatus->moveCursor(QTextCursor::StartOfLine,QTextCursor::KeepAnchor);
                compileStatus->insertPlainText(line+eol);
            }
            else
            if(line.contains("remaining",Qt::CaseInsensitive)) {
                if(progMax == 0) {
                    QString bs = line.mid(0,line.indexOf(" "));
                    progMax = bs.toInt();
                    progMax /= 1024;
                    progMax++;
                    progCount = 0;
                    if(progMax == 0) {
                        progress->setValue(100);
                    }
                }
                if(progMax != 0) {
                    progCount++;
                    progress->setValue(100*progCount/progMax);
                }
                compileStatus->moveCursor(QTextCursor::StartOfLine,QTextCursor::KeepAnchor);
                compileStatus->insertPlainText(line);
            }
            else {
                compileStatus->insertPlainText(eol);
                compileStatus->insertPlainText(line);
            }
        }
    }

}

/*
 * make star go away if no changes.
 */
void MainSpinWindow::fileChanged()
{
    if(fileChangeDisable)
        return;

    int index = editorTabs->currentIndex();
    QString name = editorTabs->tabText(index);
    Editor *ed = editors->at(index);

    /* tab controls have been moved to the editor class */

    QString curtext = ed->toPlainText();
    if(curtext.length() == 0)
        return;
    QString fileName = editorTabs->tabToolTip(index);
    if(fileName.length() == 0)
        return;
    QFile file(fileName);
    if(file.exists() == false) {
        if(curtext.length() > 0) {
            if(name.at(name.length()-1) != '*') {
                name += tr(" *");
                editorTabs->setTabText(index, name);
            }
        }
        return;
    }
    QString text;
    int ret = 0;

    QChar ch = name.at(name.length()-1);
    if(file.open(QFile::ReadOnly))
    {
        QTextStream in(&file);
        if(this->isFileUTF16(&file))
            in.setCodec("UTF-16");
        else
            in.setCodec("UTF-8");

        text = in.readAll();
        file.close();

        QString ctext = curtext;
        QPlainTextEdit myed(this);
        myed.setPlainText(text);
        text = myed.toPlainText();

        ret = ctext.compare(text);
        if(ret == 0) {
            if( ch == QChar('*'))
                editorTabs->setTabText(index, this->shortFileName(fileName));
            return;
        }
    }
    if( ch != QChar('*')) {
        name += tr(" *");
        editorTabs->setTabText(index, name);
    }
}

void MainSpinWindow::printFile()
{
    int tab = editorTabs->currentIndex();
    Editor *ed = editors->at(tab);
    QString name = editorTabs->tabText(tab);
    name = name.mid(0,name.lastIndexOf("."));

#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    printer.setDocName(name);
    printer.setOutputFormat(QPrinter::NativeFormat);
#else
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(this->lastPath+name+".pdf");
#endif

    QPrintDialog *dialog = new QPrintDialog(this);
    qDebug() << "QPrintDialog" << dialog;

    dialog->setWindowTitle(tr("Print Document"));
    //if (ed->textCursor().hasSelection()) dialog->addEnabledOption(QAbstractPrintDialog::PrintSelection);
    int rc = dialog->exec();

    qDebug() << "QPrintDialog rc" << rc;
    if(rc == QDialog::Accepted) {
        ed->print(dialog->printer());
    }
    else {
        qDebug() << "QPrintDialog rc != Accepted" << rc;
    }
    delete dialog;
}

void MainSpinWindow::copyFromFile()
{
    Editor *editor = editors->at(editorTabs->currentIndex());
    if(editor) {
        editor->copy();
    }
    editor->clearCtrlPressed();
}
void MainSpinWindow::cutFromFile()
{
    Editor *editor = editors->at(editorTabs->currentIndex());
    if(editor)
        editor->cut();
    editor->clearCtrlPressed();
}
void MainSpinWindow::pasteToFile()
{
    Editor *editor = editors->at(editorTabs->currentIndex());
    if(editor)
        editor->paste();
    editor->clearCtrlPressed();
}

void MainSpinWindow::editCommand()
{

}
void MainSpinWindow::systemCommand()
{

}

void MainSpinWindow::fontDialog()
{
    bool ok = false;

    QFont font;
    QFontDialog fd(this);
    Editor *editor = editors->at(editorTabs->currentIndex());
    if(editor) {
        QFont edfont = editor->font();
        font = fd.getFont(&ok, edfont, this);
    }
    else {
        font = fd.getFont(&ok, editorFont, this);
    }

    if(ok) {
        for(int n = editors->count()-1; n >= 0; n--) {
            Editor *ed = editors->at(n);
            ed->setFont(font);
            QString fs = QString("font: %1pt \"%2\";").arg(font.pointSize()).arg(font.family());
            ed->setStyleSheet(fs);
        }
        editorFont = font;
    }
}

void MainSpinWindow::fontBigger()
{
    Editor *editor = editors->at(editorTabs->currentIndex());
    if(editor) {
        QFont font = editor->font();
        int size = font.pointSize()*10/8;
        if(size < 90)
            font.setPointSize(size);
        for(int n = editors->count()-1; n >= 0; n--) {
            Editor *ed = editors->at(n);
            ed->setFont(font);
            QString fs = QString("font: %1pt \"%2\";").arg(font.pointSize()).arg(font.family());
            ed->setStyleSheet(fs);

        }
        editorFont = font;
    }
}

void MainSpinWindow::fontSmaller()
{
    Editor *editor = editors->at(editorTabs->currentIndex());
    if(editor) {
        QFont font = editor->font();
        int size = font.pointSize()*8/10;
        if(size > 3)
            font.setPointSize(size);
        for(int n = editors->count()-1; n >= 0; n--) {
            Editor *ed = editors->at(n);
            ed->setFont(font);
            QString fs = QString("font: %1pt \"%2\";").arg(font.pointSize()).arg(font.family());
            ed->setStyleSheet(fs);
        }
        editorFont = font;
    }
}

void MainSpinWindow::replaceInFile()
{
    if(!replaceDialog)
        return;

    Editor *editor = editors->at(editorTabs->currentIndex());
    editor->clearCtrlPressed();

    replaceDialog->clearFindText();
    QString text = editors->at(editorTabs->currentIndex())->textCursor().selectedText();
    if(text.isEmpty() == false)
        replaceDialog->setFindText(text);
    replaceDialog->clearReplaceText();

    QPalette savePalette = editor->palette();
    replaceDialog->setEditor(editor);
    replaceDialog->exec();
    editor->setPalette(savePalette);
    editor->setFocus();
}

/*
 * FindHelp
 *
 * Takes a word from the Editor's user cursor or mouse
 * and looks for it in the documentation.
 */
void MainSpinWindow::findSymbolHelp(QString text)
{
    QVariant lib = settings->value(gccWorkspaceKey);
    // settings->value(spinLibraryKey); nothing for spin really
    QString s = lib.toString()+"Learn/";
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    helpDialog->show(s, text);
    QApplication::restoreOverrideCursor();
}

void MainSpinWindow::findDeclaration(QPoint point)
{
    Editor *editor = editors->at(editorTabs->currentIndex());
    QTextCursor cur = editor->cursorForPosition(point);
    findDeclaration(cur);
}

void MainSpinWindow::findDeclaration()
{
    Editor *editor = editors->at(editorTabs->currentIndex());
    QTextCursor cur = editor->textCursor();
    findDeclaration(cur);
}

void MainSpinWindow::findDeclaration(QTextCursor cur)
{
#ifndef ENABLE_SPIN_BROWSING
    if(this->isSpinProject()) {
        QMessageBox::warning(this, tr("Can't Browse Spin"), tr("Spin source browsing is not available yet."));
        return;
    }
#endif

    Editor *editor = editors->at(editorTabs->currentIndex());
    if(editor) {
        /* find word */
        QString text = cur.selectedText();
        qDebug() << "findDeclaration " << text;

        if(text.length() == 0) {
            cur.select(QTextCursor::WordUnderCursor);
            text = cur.selectedText();
        }
        qDebug() << "findDeclaration " << text;
        editor->setTextCursor(cur);

        if(text.length() > 0 && text.at(0).isLetter()) {
            if(this->isCProject())
                ctags->runCtags(projectFile);
#ifdef SPIN
            else if(this->isSpinProject())
                ctags->runSpinCtags(projectFile, propDialog->getSpinLibraryStr());
#endif
        }
        else {
            return;
        }

        /* find line for traceback */
        cur.select(QTextCursor::LineUnderCursor);

        int index = editorTabs->currentIndex();
        QString fileName = editorTabs->tabToolTip(index);
        int currentLine  = cur.blockNumber();

        /* we append :currentLine to filename for good lookup on return from stack
         */
        QString currentTag = "/\t" +
            fileName + ":" + QString("%1").arg(currentLine) +
            "\t" + cur.selectedText();

        if(text.length() == 0) {
            findDeclarationInfo();
            return;
        }
        QString tagLine = ctags->findTag(text);

        /* if we have a good declaration, push stack and enable back button
         */
        if(showDeclaration(tagLine) > -1) {
            ctags->tagPush(currentTag);
            btnBrowseBack->setEnabled(true);
        }
        else
            findDeclarationInfo();
    }
}

bool MainSpinWindow::isTagged(QString text)
{
    bool rc = false;

    if(text.length() == 0) {
        findDeclarationInfo();
        return rc;
    }
    if(text.length() > 0 && text.at(0).isLetter()) {
        if(this->isCProject())
            ctags->runCtags(projectFile);
#ifdef SPIN
        else if(this->isSpinProject())
            ctags->runSpinCtags(projectFile, propDialog->getSpinLibraryStr());
#endif
    }
    else {
        return rc;
    }

    QString tagline = ctags->findTag(text);

    if(tagline.length() == 0)
        return rc;
    QString file = ctags->getFile(tagline);
    if(file.length() == 0)
        return rc;
    int  linenum = ctags->getLine(tagline);
    if(linenum < 0)
        return rc;

    return true;
}

void MainSpinWindow::findDeclarationInfo()
{
#if defined(Q_OS_MAC)
     QMessageBox::information(this,
         tr("Browse Code"),
         tr("Use \"Command+]\" to find a declaration.\n" \
            "Also \"Command+Left Click\" finds a declaration.\n" \
            "Use \"Command+[\" to go back.\n\n" \
            "Library declarations will not be found.\n"),
         QMessageBox::Ok);
#else
    QMessageBox::information(this,
        tr("Browse Code"),
        tr("Use \"Alt+Right Arrow\" to find a declaration.\n" \
           "Also \"Ctrl+Left Click\" finds a declaration.\n" \
           "Use \"Alt+Left Arrow\" to go back.\n\n" \
           "Library declarations will not be found.\n"),
        QMessageBox::Ok);
#endif
}

void MainSpinWindow::prevDeclaration()
{
    if(ctags->tagCount() == 0) {
        btnBrowseBack->setEnabled(false);
        return;
    }
    QString tagline = ctags->tagPop();
    if(tagline.length() > 0) {
        showDeclaration(tagline);
    }
    if(ctags->tagCount() == 0) {
        btnBrowseBack->setEnabled(false);
    }
    Editor *editor = editors->at(editorTabs->currentIndex());
    QStringList list = tagline.split("\t");
    QString word;
    if(list.length() > 1) {
        word = list.at(2);
        word = word.trimmed();
        editor->find(word);
        if(word.contains("("))
            word = word.mid(0,word.indexOf("("));
        word = word.trimmed();
        if(word.contains(" "))
            word = word.mid(0,word.indexOf(" "));
        word = word.trimmed();
    }
    if(word.length()>0) {
        QTextCursor cur = editor->textCursor();
        word = cur.selectedText();
        cur.setPosition(cur.anchor());
        editor->setTextCursor(cur);
    }
}

int MainSpinWindow::showDeclaration(QString tagline)
{
    int rc = -1;
    int  linenum = 0;

    if(tagline.length() == 0)
        return rc;
    QString file = ctags->getFile(tagline);
    if(file.length() == 0)
        return rc;

    /* if the file has a line number in it, use it.
     */
    QString line = "";
    if(file.lastIndexOf(':') > 3)
        line = file.mid(file.lastIndexOf(':')+1);

    if(line.length() > 0) {
        /* got a line number. convert and trunc file */
        linenum = line.toInt();
        file = file.mid(0,file.lastIndexOf(':'));
    }
    else {
        /* get regex line from tags */
        linenum = ctags->getLine(tagline);
        if(linenum < 0)
            return rc;
    }

    this->openFileName(file);
    Editor *editor = editors->at(editorTabs->currentIndex());
    QTextCursor cur = editor->textCursor();
    cur.setPosition(0,QTextCursor::MoveAnchor);
    cur.movePosition(QTextCursor::Down,QTextCursor::MoveAnchor,linenum);
    cur.movePosition(QTextCursor::EndOfLine,QTextCursor::MoveAnchor);
    cur.movePosition(QTextCursor::StartOfLine,QTextCursor::KeepAnchor);
    QString res = cur.selectedText();
    qDebug() << linenum << res;
    editor->setTextCursor(cur);
    QApplication::processEvents();
    return linenum;
}

void MainSpinWindow::redoChange()
{
    Editor *editor = editors->at(editorTabs->currentIndex());
    if(editor)
        editor->redo();
}

void MainSpinWindow::undoChange()
{
    Editor *editor = editors->at(editorTabs->currentIndex());
    if(editor)
        editor->undo();
}

void MainSpinWindow::setProject()
{
    int index = editorTabs->currentIndex();
    QString fileName = editorTabs->tabToolTip(index);

    if(fileName.length() == 0) {
        int rc = QMessageBox::question(this,
            tr("Save As"),
            tr("Would you like to save this file and set a project using it?"),
            QMessageBox::Yes, QMessageBox::No);
        if(rc == QMessageBox::Yes) {
            fileName = getSaveAsFile();
            if(fileName.isEmpty() == false) {
                saveFile();
                updateProjectTree(fileName);
                setCurrentProject(projectFile);
                this->openProject(projectFile); // for syntax highlighting
            }
        }
    }
    fileName = editorTabs->tabToolTip(index);
    if(fileName.length() > 0)
    {
        if(fileName.endsWith(SPIN_EXTENSION)) {
    #ifdef SPIN
            projectOptions->setCompiler(SPIN_TEXT);
    #else
            if(fileName.mid(fileName.lastIndexOf(".")+1).contains("spin",Qt::CaseInsensitive)) {
                QMessageBox::critical(
                        this,tr("SPIN Not Supported"),
                        tr("Spin projects are not supported with this version."),
                        QMessageBox::Ok);
                return;
            }
    #endif
        }
        else if(fileName.endsWith(".cpp")) {
            projectOptions->setCompiler("C++");
        }
        else if(fileName.endsWith(".c")) {
            projectOptions->setCompiler("C");
        }
        updateProjectTree(fileName);
        setCurrentProject(projectFile);
        sdCardDownloadEnable();
    }
#ifdef SPIN
    QString extension = fileName.mid(fileName.lastIndexOf(".")+1);
    if(extension.compare(SPIN_TEXT,Qt::CaseInsensitive) == 0) {
        projectOptions->setCompiler(SPIN_TEXT);
        this->closeFile(); // do this so spin highlighter works.
        this->openFileName(fileName);
#ifdef ENABLE_FILETO_SDCARD
        btnDownloadSdCard->setEnabled(false);
#endif
    }
#endif
}

void MainSpinWindow::hardware()
{
    //hardwareDialog->loadBoards();
    //hardwareDialog->show();
    initBoardTypes();
}

void MainSpinWindow::properties()
{
    propDialog->showProperties();
}

void MainSpinWindow::propertiesAccepted()
{
    getApplicationSettings();
    initBoardTypes();
    for(int n = 0; n < editorTabs->count(); n++) {
        Editor *e = editors->at(n);
        e->setTabStopWidth(propDialog->getTabSpaces()*10);
        e->setHighlights(editorTabs->tabText(n));
    }
}

/**
 * @brief MainSpinWindow::updateWorkspace
 * Process:
 *   1) Prompt the user
 *   2) If the user acknowledges positively...
 *      a) Backup the entire existing workspace (as indicated above)
 *      b) Do a full copy from new updated workspace (archive download) into the existing workspace... effectively
 *         replacing any individual files (that pre-existed), adding any new files (that didn't pre-exist),
 *         and leaving all the non-colliding files (that the user created or are otherwise not included
 *         in the new updated workspace archive.
 *      c) Prompt the user at the end (Info dialog) to show the path and name of the backup that was made.
 */
void MainSpinWindow::updateWorkspace()
{
    // link was:
    //"<a href=http://learn.parallax.com/propeller-c-set-simpleide/update-your-learn-folder>Parallax Learn</a>"+

    int rc = 0;
#if 1
    /*
     * Click Browse to select the library or workspace archive you downloaded.
     *
     * All open files will be closed and the current workspace will be backed up before the update.
     *                        [ Browse ]    [ Cancel ]
     */
    rc = QMessageBox::question(this, tr("Update SimpleIDE Workspace?"),
       tr("This updates the SimpleIDE workspace folder's examples, libraries, and documentation.")+"\n\n"+
       tr("Click Browse to select the library or workspace archive you downloaded.")+"\n\n"+
       tr("All open files will be closed and the current workspace will be backed up before the update."),
       "Browse","Cancel");
#else
    rc = QMessageBox::question(this, tr("Update SimpleIDE Workspace?"),
       tr("This updates the SimpleIDE workspace folder's examples, libraries, and documentation. ")+
       tr("Get the latest workspace from the ")+
       "<a href=http://learn.parallax.com/propeller-c-set-simpleide/update-simpleide-workspace>Parallax Learn</a>"+
       tr(" site, ")+tr("then click Update to find and select that archive.")+"\n"+
       tr("Current workspace folder will be backed up.")+" "+
       tr("All open files will be closed before update."),"Update","Cancel");
 #endif
    if(rc == 0) {
        QString projFileSave = projectFile;
        closeAll();
        propDialog->updateLearnWorkspace();
        propDialog->reloadDefaultSettings();
        this->openProject(projFileSave);
        //projectFile = projFileSave;
    }
}

void MainSpinWindow::programBuildAllLibraries()
{
    compileStatus->setPlainText("Build All Libraries?");

    QString workspace = propDialog->getCurrentWorkspace();
    QStringList files;
    if (workspace.endsWith("/") == false) workspace += "/";
    int rc = Directory::recursiveFindFileList(workspace+"Learn/Simple Libraries", "*.side", files);
    if (rc == 0) return;

    QStringList newfiles;
    for (int n = 0; n < files.length(); n++) {
        QString file = files[n];
        if (file.indexOf("simpletext") > -1) {
            newfiles.insert(0,file);
        }
        else if (file.indexOf("fdserial") > -1) {
            newfiles.insert(1,file);
        }
        else if (file.indexOf("Protocol") > -1) {
            newfiles.insert(1,file);
        }
        else if (file.indexOf("simpletools") > -1) {
            newfiles.append(file);
        }
    }

    for (int n = 0; n < files.length(); n++) {
        QString file = files[n];
        if (file.indexOf("simpletext") > -1) {
        }
        else if (file.indexOf("fdserial") > -1) {
        }
        else if (file.indexOf("Protocol") > -1) {
        }
        else if (file.indexOf("simpletools") > -1) {
        }
        else {
            newfiles.append(file);
        }
    }

    for (int n = 0; n < newfiles.length(); n++) {
        compileStatus->appendPlainText(newfiles[n]);
    }

    int question = QMessageBox::question(this,tr("Build All Libraries?"), tr("Building all libraries can take hours.")+
                          "\n"+tr("Do you really want to build all libraries?"),QMessageBox::Yes,QMessageBox::No);
    if(question != QMessageBox::Yes) {
        return;
    }

    QStringList memtype;
    memtype.append("lmm");
    memtype.append("cmm");
    foreach (QString model, memtype) {
        foreach (QString file, newfiles) {
            openProject(file);
            int rc = runBuild(QString(BUILDALL_MEMTYPE)+"="+model);
            if (rc > 0) return;
        }
    }
}

void MainSpinWindow::programStopBuild()
{
    if(builder != NULL)
        builder->abortProcess();

    if(this->procDone != true) {
        this->procMutex.lock();
        this->procDone = true;
        this->procMutex.unlock();
        QApplication::processEvents();
        //process->kill(); // don't kill here. let the user process that is waiting kill it.
    }
}

void MainSpinWindow::programBuild()
{
    runBuild("");
}

void MainSpinWindow::programBurnEE()
{
    bool connected = this->btnConnected->isChecked();
    if(runBuild(""))
        return;

    portListener->close();
    btnConnected->setChecked(false);
    term->setPortEnabled(false);

    runLoader("-e -r");
    if(connected) {
        term->getEditor()->setPlainText("");
        portListener->init(portName, term->getBaud(), getWxPortIpAddr(serialPort()));
        portListener->open();
        btnConnected->setChecked(true);
        term->setPortEnabled(true);
    }
}

void MainSpinWindow::programRun()
{
    // don't allow run if button is disabled
    if(btnProgramRun->isEnabled() == false)
        return;

    bool connected = this->btnConnected->isChecked();
    if(runBuild(""))
        return;

    portListener->close();
    btnConnected->setChecked(false);
    term->setPortEnabled(false);

    runLoader("-r");
    if(connected) {
        term->getEditor()->setPlainText("");
        portListener->init(portName, term->getBaud(), getWxPortIpAddr(serialPort()));
        portListener->open();
        btnConnected->setChecked(true);
        term->setPortEnabled(true);
    }
}

void MainSpinWindow::programDebug()
{
    // don't allow run if button is disabled
    if(btnProgramDebugTerm->isEnabled() == false)
        return;

    if(runBuild(""))
        return;

    portListener->close();

    term->getEditor()->setPortEnable(false);
    btnConnected->setChecked(false);

    if(runLoader("-r -t")) {
        portListener->close();
        return;
    }

    portListener->init(portName, term->getBaud(), getWxPortIpAddr(serialPort()));
    portListener->open();

    btnConnected->setChecked(true);
    term->getEditor()->setPlainText("");
    term->getEditor()->setPortEnable(true);
    term->setPortName(portName);
    term->activateWindow();
    term->setPortEnabled(true);
    term->show();
    term->getEditor()->setFocus();
    cbPort->setEnabled(false);
}

void MainSpinWindow::debugCompileLoad()
{
    QString gdbprog("propeller-elf-gdb");
#if defined(Q_OS_WIN)
    gdbprog += ".exe";
#else
    gdbprog = aSideCompilerPath + gdbprog;
#endif

    /* compile for debug */
    if(runBuild("-g"))
        return;

    /* compile for debug */
    portListener->close();
    btnConnected->setChecked(false);
    term->setPortEnabled(false);

    if(runLoader("-g -r"))
        return;

    /* start debugger */
    QString port = serialPort();

    /* set gdb tab */
    for(int n = statusTabs->count(); n >= 0; n--) {
        if(statusTabs->tabText(n).compare(GDB_TABNAME) == 0) {
            statusTabs->setCurrentIndex(n);
            break;
        }
    }
    QString s = projectOptions->getMemModel();
    if(s.contains(" "))
        s = s.mid(0,s.indexOf(" "));
    s += "/"+this->shortFileName(projectFile);
    s = s.mid(0,s.lastIndexOf(".side"));
    s += ".elf";

    gdb->load(gdbprog, sourcePath(projectFile), aSideCompilerPath+"gdbstub", s, port);
}

void MainSpinWindow::gdbShowLine()
{
    QString fileName = gdb->getResponseFile();
    int number = gdb->getResponseLine();
    qDebug() << "gdbShowLine" << fileName << number;

    openFileName(sourcePath(projectFile)+fileName);
    Editor *ed = editors->at(editorTabs->currentIndex());
    if(ed) ed->setLineNumber(number);
}

void MainSpinWindow::gdbKill()
{
    gdb->kill();
}

void MainSpinWindow::gdbBacktrace()
{
    gdb->backtrace();
}

void MainSpinWindow::gdbContinue()
{
    gdb->runProgram();
}

void MainSpinWindow::gdbNext()
{
    gdb->next();
}

void MainSpinWindow::gdbStep()
{
    gdb->step();
}

void MainSpinWindow::gdbFinish()
{
    gdb->finish();
}

void MainSpinWindow::gdbUntil()
{
    gdb->until();
}

void MainSpinWindow::gdbBreak()
{
    gdbShowLine();
}

void MainSpinWindow::gdbInterrupt()
{
    gdb->interrupt();
}

void MainSpinWindow::terminalClosed()
{
    portListener->close();
    btnConnected->setChecked(false);
    cbPort->setEnabled(true);
}

void MainSpinWindow::disablePortCombo()
{
    cbPort->setEnabled(false);
}

void MainSpinWindow::enablePortCombo()
{
    cbPort->setEnabled(true);
}

void MainSpinWindow::setupHelpMenu()
{
    QMenu *helpMenu = new QMenu(tr("&Help"), this);

    aboutLanding = "<html><body><br/>"+tr("Visit ") +
        //"<a href=\"http://www.parallax.com/propellergcc/\">"+
        "<a href=\"http://learn.parallax.com/propeller-c-set-simpleide\">"+
        ASideGuiKey+"</a>"+
        tr(" for more information.")+"<br/>"+
        tr("Email bug reports to")+" <a href=\"mailto:SimpleIDE@parallax.com\">SimpleIDE@parallax.com</a>"+
        "<br/></body></html>";

    menuBar()->addMenu(helpMenu);
    aboutDialog = new AboutDialog(aboutLanding, this);

    helpMenu->addAction(QIcon(":/images/SimpleManual.png"), tr("SimpleIDE User Guide (PDF)"), this, SLOT(userguideShow()));
    helpMenu->addAction(QIcon(":/images/CTutorials.png"), tr("Propeller C Tutorials (Online)"), this, SLOT(tutorialShow()));
    helpMenu->addAction(QIcon(":/images/SimpleLibrary.png"), tr("Simple Library Reference"), this, SLOT(simpleLibraryShow()));
    helpMenu->addAction(QIcon(":/images/Reference.png"), tr("PropGCC &Reference (Online)"), this, SLOT(referenceShow()));
    helpMenu->addAction(QIcon(":/images/UserHelp.png"), tr("&Build Error Rescue"), this, SLOT(buildRescueShow()));
    helpMenu->addAction(QIcon(":/images/about.png"), tr("&About"), this, SLOT(aboutShow()));
    helpMenu->addAction(QIcon(":/images/Credits.png"), tr("&Credits"), this, SLOT(creditShow()));
    //helpMenu->addAction(QIcon(":/images/Library.png"), tr("&Library"), this, SLOT(libraryShow()));

    /* new Help class */
    helpDialog = new Help();
}

void MainSpinWindow::aboutShow()
{
    aboutDialog->show();
}

void MainSpinWindow::creditShow()
{
    QString license(ASideGuiKey+tr(" was developed by Steve Denson for Parallax Inc.<br/><br/>")+ASideGuiKey+
                    tr(" is an open source GNU GPL V3 Licensed IDE (see package file COPYING3 or http://www.gnu.org/licenses) developed with Qt and uses Qt shared libraries (LGPLv2.1).<br/><br/>"));
    QString propgcc(ASideGuiKey+tr(" uses <a href=\"http://propgcc.googlecode.com\">Propeller GCC tool chain</a> based on GCC 4.6.1 (GPLV3). ")+"<p>");
    QString ctags(tr("It uses the <a href=\"http://ctags.sourceforge.net\">ctags</a> binary program built from sources under GPLv2 for source browsing. ")+"<p>");
    QString icons(tr("Most icons used are from <a href=\"http://www.small-icons.com/packs/24x24-free-application-icons.htm\">www.aha-soft.com 24x24 Free Application Icons</a> " \
                     "and used according to Creative Commons Attribution 3.0 License.<br/><br/>"));
    QString sources(tr("All IDE sources are available at <a href=\"http://propside.googlecode.com\">repository</a>. " \
                       "Licenses are in this package."));

    QString translations("<p><b>"+tr("Translations:")+"</b><p>");
    QString simplechinese(tr("Simplified Chinese by ")+tr("Kenichi Kato, designer of Matanya") + " <a href=\"mailto:MacTuxLin@gmail.com\">MacTuxLin@gmail.com</a>");

    QMessageBox::information(this, ASideGuiKey+tr(" Credits"),"<html><body>"+
        license+propgcc+ctags+icons+sources+translations+simplechinese+"</body></html>",
        QMessageBox::Ok);

}

void MainSpinWindow::helpShow()
{
    QString address = "file:///"+aSideDocPath+"/index.html";
    QString helplib("<a href=\""+address+"\">"+tr("Library help")+"</a> "+
                 tr("is context sensitive.")+" "+tr("Press F1 with mouse over words.")+"<br/>");
    QMessageBox::information(this, ASideGuiKey+tr(" Help"),
        tr("<p><b>")+ASideGuiKey+tr("</b> is an integrated C development environment "\
           "which can build and load Propeller GCC " \
           "programs to Propeller for many board types.") + "<p>" +
        helplib+aboutLanding, QMessageBox::Ok);
}

void MainSpinWindow::buildRescueShow()
{
    if(compileStatus->toPlainText().length() > 0) {
        rescueDialog->setEditText(compileStatus->toPlainText());
    }
    else {
        rescueDialog->setEditText(tr("Build rescue information not available yet.")+"\n"+tr("Build with hammer first before opening this window."));
    }
    rescueDialog->show();
}

void MainSpinWindow::libraryShow()
{
    findSymbolHelp("");
}

void MainSpinWindow::simpleLibraryShow()
{
    QVariant libv = settings->value(gccLibraryKey, QDir::homePath()+"Documents/SimpleIDE/Learn/Simple Libraries");
    QString s = libv.toString();
    int len = s.length()-1;
    if(s.at(len) == '\\' || s.at(len) == '/')
        s = s.left(len);
    s = "file:///"+s+" Index.html";
    if(s.isEmpty() == false) {
        try {
            QDesktopServices::openUrl(QUrl(s, QUrl::TolerantMode));
        } catch(int e) {
            qDebug() << "Cant find Library Index " + s;
        }
    }
}

void MainSpinWindow::referenceShow()
{
    QDesktopServices::openUrl(QUrl("http://www.parallax.com/propellergcc", QUrl::TolerantMode));
}

void MainSpinWindow::tutorialShow()
{
    QDesktopServices::openUrl(QUrl("http://learn.parallax.com/propeller-c-tutorials", QUrl::TolerantMode));
}

void MainSpinWindow::userguideShow()
{
    QString userguide = aSideCompilerPath+"SimpleIDE-User-Guide.pdf";
    QDesktopServices::openUrl(QUrl(QString("file:///")+userguide, QUrl::TolerantMode));
}

void MainSpinWindow::sdCardDownloadEnable()
{
#ifdef ENABLE_FILETO_SDCARD
    QString name = cbBoard->currentText();
    ASideBoard* board = aSideConfig->getBoardData(name);
    if(board != NULL) {
        QString sddriver = board->get("sd-driver");
        if(sddriver.isEmpty() == false) {
            btnDownloadSdCard->setEnabled(true);
        }
        else {
            btnDownloadSdCard->setEnabled(false);
        }
    }
    else {
        btnDownloadSdCard->setEnabled(false);
    }
#endif
}

void MainSpinWindow::setCurrentBoard(int index)
{
    boardName = cbBoard->itemText(index);
    projectOptions->setBoardType(boardName);
    cbBoard->setCurrentIndex(index);
    sdCardDownloadEnable();
}

void MainSpinWindow::setCurrentPort(int index)
{
    if(index < 0)
        return;

    cbPort->setCurrentIndex(index);
    if(friendlyPortName.length() > 0)
        cbPort->setToolTip(friendlyPortName.at(index));

    portName = cbPort->itemText(index);
    if(portName.length()) {
        if(portName.compare(AUTO_PORT) != 0) {
#ifdef ENUM_INIT_PORTNAME
            portListener->init(portName, term->getBaud(), getWxPortIpAddr(serialPort()));  // signals get hooked up internally
#endif
        }
    }
    lastCbPort = portName;
}

void MainSpinWindow::checkAndSaveFiles()
{
    if(projectModel == NULL)
        return;

    /* check for project file changes
     */
    QString title = projectModel->getTreeName();
    QString modTitle = title + " *";
    for(int tab = editorTabs->count()-1; tab > -1; tab--)
    {
        QString tabName = editorTabs->tabText(tab);
        if(tabName == modTitle)
        {
            saveFileByTabIndex(tab);
            editorTabs->setTabText(tab,title);
        }
    }

    /* check for project files name changes
     */
    int len = projectModel->rowCount();
    for(int n = 0; n < len; n++)
    {
        QModelIndex root = projectModel->index(n,0);
        QVariant vs = projectModel->data(root, Qt::DisplayRole);
        if(!vs.canConvert(QVariant::String))
            continue;
        QString name = vs.toString();
        if(name.indexOf(FILELINK) > 0)
            name = name.mid(0,name.indexOf(FILELINK));
        QString modName = name + " *";
        for(int tab = editorTabs->count()-1; tab > -1; tab--)
        {
            QString tabName = editorTabs->tabText(tab);
            if(tabName == modName)
            {
                saveFileByTabIndex(tab);
                editorTabs->setTabText(tab,name);
            }
        }
    }

    /* check for tabs out of project scope that are
     * not saved and warn user.
     */
    for(int tab = editorTabs->count()-1; tab > -1; tab--)
    {
        QString tabName = editorTabs->tabText(tab);
        if(tabName.at(tabName.length()-1) == '*')
        {
#ifdef SPIN
            if(!isSpinProject()) {
                QMessageBox::information(this,
                    tr("Not a Project File"),
                    tr("The file \"")+tabName+tr("\" is not part of the current project.\n") +
                    tr("Please save and add the file to the project to build it."),
                    QMessageBox::Ok);
            }
            else
#endif
            {
                QMessageBox::information(this,
                    tr("Not a Project File"),
                    tr("The file \"")+tabName+tr("\" is not part of the current project.\n"),
                    QMessageBox::Ok);
            }
        }
    }

    saveProjectOptions();

}

QString MainSpinWindow::sourcePath(QString srcpath)
{
    srcpath = QDir::fromNativeSeparators(srcpath);
    srcpath = srcpath.mid(0,srcpath.lastIndexOf(aSideSeparator)+1);
    return srcpath;
}

bool MainSpinWindow::isSpinProject()
{
#ifdef SPIN
    QString compiler = projectOptions->getCompiler();
    if(compiler.compare(SPIN_TEXT, Qt::CaseInsensitive) == 0) {
#ifdef ENABLE_FILETO_SDCARD
        btnDownloadSdCard->setEnabled(false);
#endif
        return true;
    }
#endif
    sdCardDownloadEnable();
    return false;
}

bool MainSpinWindow::isCProject()
{
    QString compiler = projectOptions->getCompiler();
    if(compiler.compare("C", Qt::CaseInsensitive) == 0) {
        sdCardDownloadEnable();
        return true;
    }
    else if(compiler.compare("C++", Qt::CaseInsensitive) == 0) {
        sdCardDownloadEnable();
        return true;
    }
#ifdef ENABLE_FILETO_SDCARD
    btnDownloadSdCard->setEnabled(false);
#endif
    return false;
}

void MainSpinWindow::selectBuilder()
{
#ifdef SPIN
    if(isSpinProject())
        builder = buildSpin;
    else if(isCProject())
#endif
        builder = buildC;
}

/**
 * @brief makeBuildProjectFile
 * Creates a project file for .c or .cpp files with main.
 * @param fileName is the file name of the tab currently visible.
 * @return 0 if project file not created.
 */
int  MainSpinWindow::makeBuildProjectFile(QString fileName)
{
    int rc = 0; // if return zero, the function did not create a project file
    if(!(fileName.endsWith(".c") || fileName.endsWith(".cpp") || fileName.endsWith(".spin"))) {
        return rc;
    }

    QString text;
    QString orgText;

    QFile file(fileName);
    if (file.open(QFile::ReadOnly))
    {
        QTextStream in(&file);
        if(this->isFileUTF16(&file))
            in.setCodec("UTF-16");
        else
            in.setCodec("UTF-8");

        text = in.readAll();
        orgText = text;
        file.close();

        if(text.length() == 0) {
            return rc;
        }
    }
    else {
        return rc;
    }


    if(fileName.endsWith(".c") || fileName.endsWith(".cpp")) {
        /* if we get here, text has valid C or C++ file content */
        QRegExp mainr("main[ \\t\\r\\n]*\\(");
        QRegExp blkc("\\/\\*.*\\*\\/");
        QRegExp comm("\/\/");

        while(text.indexOf(blkc) > -1) {
            text = text.replace(blkc,"");
        }

        QStringList list = text.split("\n");
        for(int n = 0; n < list.length(); n++) {
            QString s = list[n];
            if(s.contains(comm)) {
                list[n] = s.mid(0,s.indexOf(comm));
            }
        }
        QString t = list.join("\n");
        if(t.indexOf(mainr) > -1) {
            qDebug() << "Got main" + t;
        }
        else {
            return rc;
        }
    }
    int question = QMessageBox::question(this,tr("Create Project?"), tr("A project is required to compile the file")+"\n"+fileName+
                          "\n\n"+tr("Would you like to create a project now?"),QMessageBox::Yes,QMessageBox::No);
    if(question != QMessageBox::Yes) {
        return rc;
    }

    QString projName(fileName);
    projName = projName.mid(0,projName.lastIndexOf("."));
    projName += ".side";

    QString workspace = propDialog->getApplicationWorkspace();

    int rctype = 0;
    if(fileName.endsWith(".c")) {
        workspace = workspace + "My Projects/Blank Simple Project.side";
        rctype = 1;
    }
    else if(fileName.endsWith(".cpp")) {
        workspace = workspace + "My Projects/Blank Simple C++ Project.side";
        rctype = 2;
    }
    else if(fileName.endsWith(".spin")) {
        workspace = workspace + "My Projects/Blank Spin Project.side";
        QFile spin(workspace);
        if(spin.exists() == false) {
            if(spin.open(QFile::WriteOnly)) {
                spin.write("Blank Spin File.spin\r\n>compiler=SPIN");
                spin.close();
            }
        }
        rctype = 3;
    }

    if(QFile::exists(workspace)) {
        if(copyProjectAs(workspace, projName, fileName) < 0) {
            QMessageBox::information(this,tr("Project Error"), tr("Error creating a project for the file")+"\n"+fileName);
            return rc; // error, don't open bad project
        }
        projectFile = projName;

        if (file.open(QFile::WriteOnly)) {
            file.write(orgText.toUtf8());
            file.close();
        }

        this->openProject(projName);
        rc = rctype;
    }
    else {
        QMessageBox::critical(this, tr("Project Not Found"),
            tr("Blank Simple Project was not found."));
        return rc;
    }

    return rc;
}

int  MainSpinWindow::runBuild(QString option)
{
    if(projectModel == NULL || projectFile.isEmpty()) {
        QMessageBox::critical(this, tr("Can't Build"), tr("A project must be loaded to build programs."));
        return 1;
    }

    statusDialog->init("Build", "Building Program");

    int index = editorTabs->currentIndex();
    QString file = editorTabs->tabText(index);
    if(file.endsWith("*")) {
        saveFile();
    }

    checkAndSaveFiles();
    selectBuilder();

    /*
     * If we have a file that has an associated project, choose that project.
     */
    index = editorTabs->currentIndex();
    if(index > -1) {
        QString tabname = this->editorTabs->tabText(index);
        QString mainFile;
        QStringList list;
        QString projstr;
        QFile file(projectFile);
        if(file.exists()) {
            if(file.open(QFile::ReadOnly | QFile::Text)) {
                projstr = file.readAll();
                file.close();
            }
            list = projstr.split("\n");
            mainFile = list[0];
        }

        int inlist = 0;
        for(int n = 0; n < list.length(); n++) {
            if(list[n].contains(tabname)) {
                inlist = 1;
                break;
            }
        }

        // Don't worry about Untitled files.
        if(editorTabs->tabText(index).compare("Untitled") == 0) {
            inlist = 1;
        }

        if(!inlist) {
            /* A .c or .c++ file that has a qualified main() and no project should have a project created. */
            inlist = makeBuildProjectFile(this->editorTabs->tabToolTip(index));
            if(!inlist) {
                HintDialog::hint("NotInProject", tr("The file being displayed is not part of the project. It will not be included in the program being built. Use the project button to check the project contents."), this);
            }
            else if(inlist == 1) {
                builder = this->buildC;
            }
            else if(inlist == 2) {
                builder = this->buildC;
            }
            else if(inlist == 3) {
                builder = this->buildSpin;
            }

        }
        else if(!mainFile.contains(tabname)) {
            HintDialog::hint("NotProjectMainFile", tr("The file being displayed is not the main project file. It may not represent the program to build. Use the project button to check the project name if necessary. For making a main file for building, press F4 to set the project."), this);
        }
    }
#if defined(GDBENABLE)
    /* stop debugger */
    gdb->stop();
#endif

    /*
     * The status bar contents can grow the window width.
     * We want too keep the window size the same if possible.
     * Lets just set the status label max size here so it doesn't grow.
     * Do it here because the user may have changed the width.
     */
    QSize fs = this->frameSize();
    int maxw = fs.width() - status->x()-100;
    status->setMaximumWidth(maxw);

    statusDialog->init("Build", "Building Propeller application.");
    int rc = builder->runBuild(option, projectFile, aSideCompiler);
    statusDialog->stop();

    return rc;
}

void MainSpinWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    QSize fs = this->frameSize();
    int maxw = fs.width() - status->x()-100;
    status->setMaximumWidth(maxw);
}

QStringList MainSpinWindow::getLoaderParameters(QString copts, QString file)
{
    // use the projectFile instead of the current tab file
    // QString srcpath = sourcePath(projectFile);

    int compileType = ProjectOptions::TAB_OPT; // 0
#ifdef SPIN
    if(isSpinProject()) {
        builder = buildSpin;
        compileType = ProjectOptions::TAB_SPIN_COMP;
    }
    else if(isCProject())
#endif
    {
        builder = buildC;
        compileType = ProjectOptions::TAB_C_COMP;
    }

#ifdef ENABLE_WXLOADER
    QStringList args;
#endif

#ifdef ENABLE_PROPELLER_LOAD
    //portName = serialPort(); //cbPort->itemText(cbPort->currentIndex());

    QString bname = this->cbBoard->currentText();
    boardName = bname;
    QString sdrun = ASideConfig::UserDelimiter+ASideConfig::SdRun;
    QString sdload = ASideConfig::UserDelimiter+ASideConfig::SdLoad;

    sdrun = sdrun.toUpper();
    sdload = sdload.toUpper();
    if(boardName.contains(sdrun))
        boardName = boardName.mid(0,boardName.indexOf(sdrun));
    if(boardName.contains(sdload,Qt::CaseInsensitive))
        boardName = boardName.mid(0,boardName.indexOf(sdload));

    QStringList args;

    if(this->propDialog->getLoadDelay() > 0) {
        args.append(QString("-S%1").arg(this->propDialog->getLoadDelay()));
    }
    ASideBoard* board = aSideConfig->getBoardData(bname);
    QString reset("DTR");
    if(board != NULL)
        reset = board->get(ASideBoard::reset);

    if(this->propDialog->getResetType() == Properties::CFG) {
        if(reset.contains("RTS",Qt::CaseInsensitive))
            args.append("-Dreset=rts");
        else
        if(reset.contains("DTR",Qt::CaseInsensitive))
            args.append("-Dreset=dtr");
    }
    else
    if(this->propDialog->getResetType() == Properties::RTS) {
        args.append("-Dreset=rts");
    }
    else
    if(this->propDialog->getResetType() == Properties::DTR) {
        args.append("-Dreset=dtr");
    }
    // always include user's propeller-load path.
    if(compileType == ProjectOptions::TAB_C_COMP) {
        args.append("-I");
        args.append(aSideIncludes);
        if(bname.compare(GENERIC_BOARD) != 0) {
            args.append("-b");
            args.append(boardName);
        }
    }
    //args.append("-p");
    //args.append(portName);
#endif

    builder->appendLoaderParameters(copts, file, &args);

    qDebug() << args;
    return args;
}

void MainSpinWindow::statusNone()
{
    status->setStyleSheet("QLabel { background-color: palette(window) }");
}

void MainSpinWindow::statusFailed()
{
    status->setStyleSheet("QLabel { background-color: rgb(255,0,0) }");
}

void MainSpinWindow::statusPassed()
{
    status->setStyleSheet("QLabel { background-color: rgb(0,200,0); }");
}

int  MainSpinWindow::runLoader(QString copts)
{

    // don't allow if no port available
    if(copts.length() > 0 && cbPort->count() < 1) {
        status->setText(tr("Serial port not available.")+" "+tr("Connect a USB Propeller board and turn it on."));
        statusFailed();
        blinker->start();
        return 1;
    }

    if(projectModel == NULL || projectFile.isNull()) {
        QMessageBox mbox(QMessageBox::Critical, "Error No Project",
            "Please select a tab and press F4 to set main project file.", QMessageBox::Ok);
        mbox.exec();
        return -1;
    }

    bool rename_only = false;
    if (copts.indexOf("-n") == 0) {
        rename_only = true;
    }

    progress->show();
    progress->setValue(0);

    getApplicationSettings();

    if(copts.indexOf(" -t") > 0) {
        copts = copts.mid(0,copts.indexOf(" -t"));
    }

    QString file;

#ifdef ENABLE_PROPELLER_LOAD
    QString loadtype = cbBoard->currentText();
    if(loadtype.isEmpty() || loadtype.length() == 0) {
        QMessageBox::critical(this,tr("Can't Load"),tr("Can't load an empty board type."),QMessageBox::Ok);
        return -1;
    }

    if(loadtype.contains(ASideConfig::UserDelimiter+ASideConfig::SdRun, Qt::CaseInsensitive)) {
        QString s = projectOptions->getMemModel();
        if(s.contains(" "))
            s = s.mid(0,s.indexOf(" "));
        s += "/"+this->shortFileName(projectFile);
        s = s.mid(0,s.lastIndexOf(".side"));
        s += ".elf";
        file = s;
        copts.append(" -z ");
        qDebug() << loadtype << copts;
    }
    else if(loadtype.contains(ASideConfig::UserDelimiter+ASideConfig::SdLoad, Qt::CaseInsensitive)) {
        QString s = projectOptions->getMemModel();
        if(s.contains(" "))
            s = s.mid(0,s.indexOf(" "));
        s += "/"+this->shortFileName(projectFile);
        s = s.mid(0,s.lastIndexOf(".side"));
        s += ".elf";
        file = s;
        copts.append(" -l ");
        qDebug() << loadtype << copts;
    }
    else if(this->isSpinProject()) {
        QString s = this->shortFileName(projectFile);
        s = s.mid(0,s.lastIndexOf(".side"));
        file = s;
        qDebug() << loadtype << copts;
    }
#endif

    QStringList args = getLoaderParameters(copts, file);

    portName = serialPort();
    if(portName.compare(AUTO_PORT) == 0) {
        compileStatus->appendPlainText("error: Propeller not found on any port.");
        return 1;
    }

#ifdef ENABLE_WXLOADER
    // picky wxloader J
    for (int n = args.count()-1; n > -1; n--) {
        if (args[n].length() == 0)
            args.removeAt(n);
    }

    QString loadtype = cbBoard->currentText();
    if(loadtype.compare("GENERIC") == 0) loadtype = "RCFAST";
    if(!loadtype.isEmpty() && loadtype.length() > 0 && rename_only == false) {
        args.append("-I");
        args.append(aSideIncludes);
        args.append("-b");
        args.append(loadtype.toLower());
    }

    if (getWxPortIpAddr(portName).length()) {
        //args.removeOne("-r");
        args.append("-i");
        args.append(getWxPortIpAddr(portName));
    } else {
        //args.append("-s"); // autodetect serial port
        args.append("-p");
        args.append(portName);
    }

    // Do this if syntax is enforced. I.E. proploader [options] file
    QString tmp = args[0];
    args.removeAt(0);
    if (rename_only == false) {
        args.append(tmp);
    }

#endif

#ifdef ENABLE_PROPELLER_LOAD
    args.append("-p");
    args.append(portName);
#endif

    builder->showBuildStart(aSideLoader,args);

    process->setProperty("Name", QVariant(aSideLoader));
    process->setProperty("IsLoader", QVariant(true));

    connect(process, SIGNAL(readyReadStandardOutput()),this,SLOT(procReadyRead()));
    connect(process, SIGNAL(finished(int,QProcess::ExitStatus)),this,SLOT(procFinished(int,QProcess::ExitStatus)));
    connect(process, SIGNAL(error(QProcess::ProcessError)), this, SLOT(procError(QProcess::ProcessError)));

    process->setProcessChannelMode(QProcess::MergedChannels);
    process->setWorkingDirectory(this->sourcePath(projectFile));

    procMutex.lock();
    procDone = false;
    procMutex.unlock();

    if (rename_only) {
        // do nothing
    }
    else if (copts.indexOf("-R") >= 0) {
        statusDialog->init("Reset", "Resetting Port");
    }
    else {
        statusDialog->init("Loading", "Loading Program");
    }

    portListener->close();

    process->start(aSideLoader,args);
    compileStatus->insertPlainText("\n");

    if (rename_only) {
        status->setText(tr(" Renaming ... "));
    }
    else if (copts.indexOf("-R") >= 0) {
        status->setText(tr(" Resetting ... "));
    }
    else {
        status->setText(status->text()+tr(" Loading ... "));
    }

    while(procDone == false) {
        QApplication::processEvents();
        Sleeper::ms(20);
    }

    int killed = 0;
    if(process->state() == QProcess::Running) {
        process->kill();
        Sleeper::ms(33);
        compileStatus->appendPlainText(tr("Loader killed by user."));
        status->setText(status->text() + tr(" Done."));
        killed = -1;
    }

    QTextCursor cur = compileStatus->textCursor();
    cur.movePosition(QTextCursor::End,QTextCursor::MoveAnchor);
    compileStatus->setTextCursor(cur);

    statusDialog->stop();

    if (rename_only) {
        Sleeper::ms(2000);
        this->enumeratePorts();
    }
    progress->hide();
    return process->exitCode() | killed;
}

void MainSpinWindow::compilerError(QProcess::ProcessError error)
{
    qDebug() << error;
}

void MainSpinWindow::compilerFinished(int exitCode, QProcess::ExitStatus status)
{
    qDebug() << exitCode << status;
}

void MainSpinWindow::closeTab(int tab)
{
    if(editors->count() < 1)
        return;

    saveTab(tab);

    fileChangeDisable = true;

    editors->at(tab)->setPlainText("");
    editors->remove(tab);
    if(editorTabs->count() == 1)
        newFile();
    editorTabs->removeTab(tab);

    fileChangeDisable = false;
    currentTabChanged();
}

void MainSpinWindow::saveTab(int tab, bool ask)
{
    if(editors->count() < 1)
        return;

    QMessageBox mbox(QMessageBox::Question, "Save File?", "",
                     QMessageBox::Discard | QMessageBox::Save, this);

    fileChangeDisable = true;

    QString tabName = editorTabs->tabText(tab);
    if(tabName.at(tabName.length()-1) == '*')
    {
        if(editorTabs->tabText(tab).contains(untitledstr)) {
            saveAsFile(editors->at(tab)->toolTip());
        }
        else {
            if(ask) {
                mbox.setInformativeText(tr("Save File? ") + tabName.mid(0,tabName.indexOf(" *")));
                int ret = mbox.exec();
                if(ret == QMessageBox::Save)
                    saveFileByTabIndex(tab);
            }
            else {
                saveFileByTabIndex(tab);
            }
        }
    }

    fileChangeDisable = false;
}

/* no menu yet, just right click to reload
*/
void MainSpinWindow::editorTabMenu(QPoint pt)
{
    if(pt.isNull())
        return;
    int tab = editorTabs->currentIndex();
    if(tab < 0)
        return;
    QString file = editorTabs->tabToolTip(tab);
    QString name = editorTabs->tabText(tab);
    if(file.length() > 0 && name.indexOf("*") < 0)
        openFile(file);
}


void MainSpinWindow::changeTab(bool checked)
{
    /* checked is a QAction menu state.
     * we don't really care about it
     */

    if(checked) return; // compiler happy :)

    int n = editorTabs->currentIndex();
    if(n < editorTabs->count()-1)
        editorTabs->setCurrentIndex(n+1);
    else
        editorTabs->setCurrentIndex(0);
}

QStringList MainSpinWindow::projectList(QString projFile)
{
    QString projstr = "";
    QStringList list;

    QFile file(projFile);
    if(file.exists()) {
        if(file.open(QFile::ReadOnly | QFile::Text)) {
            projstr = file.readAll();
            file.close();
        }
        list = projstr.split("\n");
    }
    return list;
}

void MainSpinWindow::currentTabChanged()
{
    // Disable for now.
    //return;

    //qDebug() << "currentTabChanged";
    if(tabChangeDisable) return;
    if(fileChangeDisable) return;
    int n = editorTabs->currentIndex();
    //qDebug() << "currentTabChanged tab" << n;
    if(n < 0) return;

    QString file = editorTabs->tabText(n);
    if(file.endsWith("*")) {
        // file being edited, don't load another copy.
        return;
    }
    file = editorTabs->tabToolTip(n);
    if(file.length() == 0) {
        return;
    }
    QString proj = file.left(file.lastIndexOf("."));
    qDebug() << "currentTabChanged" << file << simpleViewType << proj;

    openProjectFileMatch(file);
}

/**
 * @brief openProjectFileMatch allows opening a project with the same base file name.
 * @param file
 */
void MainSpinWindow::openProjectFileMatch(QString file)
{
    // There is some concern that this won't be productive with Open Tab or Add Tab
    // If project list contains the tab and tab doesn't match the project, don't change the project.

    QString proj = file.left(file.lastIndexOf("."));

    if(projectFile.compare("none") == 0) {
        projectFile = proj+SIDE_EXTENSION;
    }
    QStringList plist = projectList(projectFile);
    QString pbase = projectFile;
    pbase = pbase.left(pbase.lastIndexOf("."));
    if(plist.contains(shortFileName(file)) && proj != pbase) {
        return;
    }

    // if simpleview mode, change the project
    // if the file is a main project file.
    // A project file name is the same as
    // the program file excluding the suffix.
    //
    if(this->simpleViewType && proj.length() > 0) {
        tabChangeDisable = true;
        proj += SIDE_EXTENSION;
        if(QFile::exists(proj)) {
            openFile(proj);
        }
        tabChangeDisable = false;
    }
}

void MainSpinWindow::clearTabHighlight()
{
    qDebug() << "clearTabHighlight" << "Total Tabs" << editorTabs->count() << "Total Editors" << editors->count() << "Current" << editorTabs->currentIndex();
    emit highlightCurrentLine(QColor(255, 255, 255));
}

void MainSpinWindow::addToolButton(QToolBar *bar, QToolButton *btn, QString imgfile)
{
    const QSize buttonSize(24, 24);
    btn->setIcon(QIcon(QPixmap(imgfile.toLatin1())));
    btn->setMinimumSize(buttonSize);
    btn->setMaximumSize(buttonSize);
    btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    bar->addWidget(btn);
}

void MainSpinWindow::addToolBarAction(QToolBar *bar, QAction *btn, QString imgfile)
{
    btn->setIcon(QIcon(QPixmap(imgfile.toLatin1())));
    bar->addAction(btn);
}

QSize MainSpinWindow::sizeHint() const
{
    const QSize size(APPWINDOW_START_WIDTH, APPWINDOW_START_HEIGHT);
    return size;
}

void MainSpinWindow::resetRightSplitSize()
{
    QList<int> rsizes = rightSplit->sizes();
    rsizes[0] = rightSplit->height()*80/100;
    rsizes[1] = rightSplit->height()*20/100;
    rightSplit->setSizes(rsizes);
    rightSplit->adjustSize();
}

void MainSpinWindow::resetVerticalSplitSize()
{
    QList<int> sizes = vsplit->sizes();
    sizes[0] = vsplit->width()*20/100;
    sizes[1] = vsplit->width()*80/100;
    vsplit->setSizes(sizes);
    vsplit->adjustSize();
}

void MainSpinWindow::setupProjectTools(QSplitter *vsplit)
{
    /* container for project, etc... */
    leftSplit = new QSplitter(this);
    leftSplit->setMinimumHeight(APPWINDOW_START_HEIGHT/2);
    leftSplit->setOrientation(Qt::Vertical);
    vsplit->addWidget(leftSplit);

    /* project tree */
    QTabWidget *projectTab = new QTabWidget(this);
    projectTree = new ProjectTree(this);
    projectTree->setMinimumWidth(PROJECT_WIDTH);
    projectTree->setMaximumWidth(PROJECT_WIDTH);
    projectTree->setToolTip(tr("Current Project"));
    connect(projectTree, SIGNAL(clicked(QModelIndex)),this,SLOT(projectTreeClicked()));
    connect(projectTree, SIGNAL(activated(QModelIndex)),this,SLOT(projectTreeClicked()));
    connect(projectTree, SIGNAL(deleteProjectItem()),this,SLOT(deleteProjectFile()));
    connect(projectTree, SIGNAL(showPopup()),this,SLOT(showProjectPopup()));
    projectTab->addTab(projectTree,tr("Project Manager"));
    projectTab->setMaximumWidth(PROJECT_WIDTH);
    projectTab->setMinimumWidth(PROJECT_WIDTH);
    leftSplit->addWidget(projectTab);

    // projectMenu is popup for projectTree
    projectMenu = new QMenu(QString("Project Menu"), projectTree);
    projectMenu->addAction(tr(AddFileCopy), this,SLOT(addProjectFile()));
#ifdef ENABLE_ADD_LINK
    projectMenu->addAction(tr(AddFileLink), this,SLOT(addProjectLink()));
#endif
    projectMenu->addAction(tr(AddIncludePath), this,SLOT(addProjectIncPath()));
    projectMenu->addAction(tr(AddLibraryPath), this,SLOT(addProjectLibPath()));

    projectMenu->addAction(tr("Delete"), this,SLOT(deleteProjectFile()));
    projectMenu->addAction(tr("Show Assembly"), this,SLOT(showAssemblyFile()));
    projectMenu->addAction(tr("Show Map File"), this,SLOT(showMapFile()));
    projectMenu->addAction(tr("Show File"), this,SLOT(showProjectFile()));

#ifdef SIMPLE_BOARD_TOOLBAR
    projectOptions = new ProjectOptions(this, cbBoard);
#else
    projectOptions = new ProjectOptions(this, NULL);
#endif
    projectOptions->setMinimumWidth(PROJECT_WIDTH);
    projectOptions->setMaximumWidth(PROJECT_WIDTH);
    projectOptions->setToolTip(tr("Project Options"));
    connect(projectOptions,SIGNAL(compilerChanged()),this,SLOT(compilerChanged()));

#ifndef SIMPLE_BOARD_TOOLBAR
    cbBoard = projectOptions->getHardwareComboBox();
    cbBoard->setToolTip(tr("Board Type Select"));
    connect(cbBoard,SIGNAL(currentIndexChanged(int)),this,SLOT(setCurrentBoard(int)));
    QToolButton *hardwareButton = projectOptions->getHardwareButton();
    connect(hardwareButton,SIGNAL(clicked()),this,SLOT(reloadBoardTypes()));
#endif

    leftSplit->addWidget(projectOptions);

    QList<int> lsizes = leftSplit->sizes();
    lsizes[0] = leftSplit->height()*50/100;
    lsizes[1] = leftSplit->height()*50/100;
    leftSplit->setSizes(lsizes);
    leftSplit->adjustSize();

    rightSplit = new QSplitter(this);
    rightSplit->setMinimumHeight(APPWINDOW_START_HEIGHT/2);
    rightSplit->setOrientation(Qt::Vertical);
    vsplit->addWidget(rightSplit);

    resetVerticalSplitSize();

    /* project editors */
    editors = new QVector<Editor*>();

    /* project editor tabs */
    editorTabs = new QTabWidget(this);
    editorTabs->setTabsClosable(true);
    editorTabs->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(editorTabs,SIGNAL(currentChanged(int)),this,SLOT(clearTabHighlight()));
    connect(editorTabs,SIGNAL(tabCloseRequested(int)),this,SLOT(closeTab(int)));
    connect(editorTabs,SIGNAL(customContextMenuRequested(QPoint)),this,SLOT(editorTabMenu(QPoint)));
    rightSplit->addWidget(editorTabs);

    statusTabs = new QTabWidget(this);

    compileStatus = new BuildStatus(this);
    compileStatus->setLineWrapMode(QPlainTextEdit::NoWrap);
    compileStatus->setReadOnly(true);
    connect(compileStatus,SIGNAL(selectionChanged()),this,SLOT(compileStatusClicked()));
    statusTabs->addTab(compileStatus,tr("Build Status"));

#if defined(IDEDEBUG)
    statusTabs->addTab(debugStatus,tr("IDE Debug Info"));
    ideDebugTabIndex = statusTabs->count()-1;
    //statusTabs->removeTab(ideDebugTabIndex);
    ideDebugTabIndex = 0;
    QAction *ideDebugAction = new QAction(tr("Show IDE Debug Info"), this);
    ideDebugAction->setShortcut(Qt::CTRL+Qt::ShiftModifier+Qt::Key_D);
    connect(ideDebugAction,SIGNAL(triggered()),this,SLOT(ideDebugShow()));
    this->addAction(ideDebugAction);
#endif

#if defined(GDBENABLE)
    gdbStatus = new QPlainTextEdit(this);
    gdbStatus->setLineWrapMode(QPlainTextEdit::NoWrap);
    /* setup the gdb class */
    gdb = new GDB(gdbStatus, this);

    statusTabs->addTab(gdbStatus,tr(GDB_TABNAME));
#endif

    rightSplit->addWidget(statusTabs);

    resetRightSplitSize();

    connect(vsplit,SIGNAL(splitterMoved(int,int)),this,SLOT(splitterChanged()));
    connect(leftSplit,SIGNAL(splitterMoved(int,int)),this,SLOT(splitterChanged()));
    connect(rightSplit,SIGNAL(splitterMoved(int,int)),this,SLOT(splitterChanged()));

    /* status bar for progressbar */
    QStatusBar *statusBar = new QStatusBar(this);
    this->setStatusBar(statusBar);
    progress = new QProgressBar();
    progress->setMaximumSize(90,20);
    progress->hide();

    btnShowProjectPane = new QPushButton(QIcon(":/images/ProjectShow.png"),"",this);
    btnShowProjectPane->setCheckable(true);
    btnShowProjectPane->setMaximumWidth(32);
    btnShowProjectPane->setToolTip(tr("Show Project Manager"));
    connect(btnShowProjectPane,SIGNAL(clicked(bool)),this,SLOT(showProjectPane(bool)));

    btnShowStatusPane = new QPushButton(QIcon(":/images/BuildShow.png"), "", this);
    btnShowStatusPane->setCheckable(true);
    btnShowStatusPane->setMaximumWidth(32);
    btnShowStatusPane->setToolTip(tr("Show Build Status"));
    connect(btnShowStatusPane,SIGNAL(clicked(bool)),this,SLOT(showStatusPane(bool)));

    programSize = new QLabel();
    programSize->setMaximumWidth(PROJECT_WIDTH);
    programSize->setMinimumWidth(PROJECT_WIDTH);

    status = new QLabel();

    blinker = new Blinker(status);
    connect(blinker, SIGNAL(statusNone()), this, SLOT(statusNone()));
    connect(blinker, SIGNAL(statusFailed()), this, SLOT(statusFailed()));

    statusBar->addPermanentWidget(progress);
    statusBar->addWidget(btnShowProjectPane);
    statusBar->addWidget(programSize);
    statusBar->addWidget(btnShowStatusPane);
    statusBar->addWidget(status);
    statusBar->setMaximumHeight(32);

}

void MainSpinWindow::cStatusClicked(QString line)
{
    int n = 0;
    QRegExp regx(":[0-9]");
    QStringList fileList = line.split(regx);
    if(fileList.count() < 2)
        return;

    QString file = fileList[0];
    file = file.mid(file.lastIndexOf(":")+1);

    /* open file in tab if not there already */
    for(n = 0; n < editorTabs->count();n++) {
        if(editorTabs->tabText(n).indexOf(file) == 0) {
            editorTabs->setCurrentIndex(n);
            break;
        }
        if(editors->at(n)->toolTip().endsWith(file)) {
            editorTabs->setCurrentIndex(n);
            break;
        }
    }

    if(n > editorTabs->count()-1) {
        if(QFile::exists(fileList[0])) {
            file = fileList[0];
            openFileName(file);
        }
        else
        if(QFile::exists(sourcePath(projectFile))) {
            file = fileList[0];
            openFileName(sourcePath(projectFile)+file);
        }
        else {
            return;
        }
    }

    line = line.mid(file.length());
    if(line.length() == 0)
        return;
    QStringList list = line.split(":",QString::SkipEmptyParts);

    Editor *editor = editors->at(editorTabs->currentIndex());
    if(editor != NULL)
    {
        n = QString(list[0]).toInt();
        QTextCursor c = editor->textCursor();
        c.movePosition(QTextCursor::Start);
        if(n > 0) {
            c.movePosition(QTextCursor::Down,QTextCursor::MoveAnchor,n-1);
            c.movePosition(QTextCursor::StartOfLine);
            c.movePosition(QTextCursor::EndOfLine,QTextCursor::KeepAnchor,1);
        }
        editor->setTextCursor(c);
        editor->setFocus();

        // clear old formatting ... wasteful but i don't have time for risk at the moment
        c.movePosition(QTextCursor::StartOfLine, QTextCursor::MoveAnchor);
        editor->setTextCursor(c);

        // highlight error
        emit highlightCurrentLine(QColor(255, 255, 0));
    }
}

void MainSpinWindow::spinStatusClicked(QString line)
{
    int n = 0;

    if(line.contains("error",Qt::CaseInsensitive) == false)
        return;
    if(line.indexOf("(") < 0)
        return;

    QString file = line.mid(0,line.indexOf("("));
    if(file.contains("..."))
        file = file.mid(file.indexOf("...")+3);

    // this is useful for C and SPIN
    if(file.contains(SPIN_EXTENSION, Qt::CaseInsensitive) == false)
        file += SPIN_EXTENSION;

    /* open file in tab if not there already */
    for(n = 0; n < editorTabs->count();n++) {
        if(editorTabs->tabText(n).contains(file)) {
            editorTabs->setCurrentIndex(n);
            break;
        }
        if(editors->at(n)->toolTip().contains(file)) {
            editorTabs->setCurrentIndex(n);
            break;
        }
    }

    if(n > editorTabs->count()-1) {
        if(QFile::exists(file)) {
            openFileName(file);
        }
        else
        if(QFile::exists(sourcePath(projectFile))) {
            file = sourcePath(projectFile)+file;
            openFileName(file);
        }
        else {
            return;
        }
    }

    QStringList list = line.split(QRegExp("[(,:)]"));
    // list should contain
    // 0 filename
    // 1 row
    // 2 column
    // rest of line
    Editor *editor = editors->at(editorTabs->currentIndex());
    if(editor != NULL)
    {
        n = QString(list[1]).toInt();
        QTextCursor c = editor->textCursor();
        c.movePosition(QTextCursor::Start);
        if(n > 0) {
            c.movePosition(QTextCursor::Down,QTextCursor::MoveAnchor,n-1);
            c.movePosition(QTextCursor::StartOfLine);
            c.movePosition(QTextCursor::EndOfLine,QTextCursor::KeepAnchor,1);
        }
        editor->setTextCursor(c);
        editor->setFocus();

        // clear old formatting ... wasteful but i don't have time for risk at the moment
        c.movePosition(QTextCursor::StartOfLine, QTextCursor::MoveAnchor);
        editor->setTextCursor(c);

        // highlight error
        emit highlightCurrentLine(QColor(255, 255, 0));
    }
}

/*
 * Find error or warning in a file
 */
void MainSpinWindow::compileStatusClicked(void)
{
    if(compileStatusClickEnable == false)
        return;
    compileStatusClickEnable = false;
    QTextCursor cur = compileStatus->textCursor();
    QString line = cur.selectedText();
    /* if more than one line, we have a select all */
    QStringList lines = line.split(QChar::ParagraphSeparator);
    if(lines.length()>1)
        return;
    cur.movePosition(QTextCursor::StartOfLine,QTextCursor::MoveAnchor);
    cur.movePosition(QTextCursor::EndOfLine,QTextCursor::KeepAnchor);
    compileStatus->setTextCursor(cur);
    line = cur.selectedText();

    if(isCProject()) {
        cStatusClicked(line);
    }
#ifdef SPIN
    else if(isSpinProject()) {
        spinStatusClicked(line);
    }
#endif
    compileStatusClickEnable = true;
}

void MainSpinWindow::showCompileStatusError()
{
    if(this->simpleViewType) {
        // only show message box in simple view
        QMessageBox::critical(this,
            tr("Build Error"),
            tr("Build Failed. Check Build Status for errors."));
    }
    showStatusPane(true);
    btnShowStatusPane->setChecked(true);

    // find first error line
    QTextDocument *doc = compileStatus->document();

    QString line;
    int len = doc->lineCount();
    for(int n = 0; n < len; n++) {
        QTextBlock block = doc->findBlockByLineNumber(n);
        line = block.text();
        if(line.contains("error:", Qt::CaseInsensitive))
            break;
    }

    if(line.contains("error: no propeller", Qt::CaseInsensitive))
        return;

    if(line.contains("error: opening", Qt::CaseInsensitive))
        return;

    // open file and set line
    /* if more than one line, we have a select all */
    if(line.length() < 1)
        return;

    if(isCProject()) {
        cStatusClicked(line);
    }
#ifdef SPIN
    else if(isSpinProject()) {
        spinStatusClicked(line);
    }
#endif

    // clear old formatting
    Editor *ed = editors->at(editorTabs->currentIndex());
    QTextCursor edcur = ed->textCursor();
    edcur.movePosition(QTextCursor::StartOfLine, QTextCursor::MoveAnchor);
    ed->setTextCursor(edcur);

    // highlight error ... Yellow per Andy's request
    emit highlightCurrentLine(QColor(Qt::yellow));
}

void MainSpinWindow::compilerChanged()
{
    if(isCProject()) {
        projectMenu->setEnabled(true);
        cbBoard->setEnabled(true);
    }
#ifdef SPIN
    else if(isSpinProject()) {
        projectMenu->setEnabled(false);
        cbBoard->setEnabled(false);
        int n = cbBoard->findText(GENERIC_BOARD);
        if(n > -1) cbBoard->setCurrentIndex(n);
    }
#endif
}

void MainSpinWindow::projectTreeClicked()
{
    if(projectModel == NULL)
        return;
    showProjectFile();
}

void MainSpinWindow::showProjectPopup()
{
    projectMenu->popup(QCursor::pos());
}

/*
 * don't allow adding output files
 */
bool MainSpinWindow::isOutputFile(QString file)
{
    bool rc = false;

    QString ext = file.mid(file.lastIndexOf("."));
    if(ext.length()) {
        ext = ext.toLower();
        if(ext == ".cog") {
            // don't copy .cog files
            rc = true;
        }
        else if(ext == ".dat") {
            // don't copy .dat files
            rc = true;
        }
        else if(ext == ".o") {
            // don't copy .o files
            rc = true;
        }
        else if(ext == ".out") {
            // don't copy .out files
            rc = true;
        }
        else if(ext == SIDE_EXTENSION) {
            // don't copy .side files
            rc = true;
        }
    }
    return rc;
}

/*
 * fileName can be short name or link name
 */
void MainSpinWindow::addProjectListFile(QString fileName)
{
    QString projstr = "";
    QStringList list;
    QString mainFile;

    QFile file(projectFile);
    if(file.exists()) {
        if(file.open(QFile::ReadOnly | QFile::Text)) {
            projstr = file.readAll();
            file.close();
        }
        list = projstr.split("\n");
        mainFile = list[0];
        projstr = "";
        for(int n = 0; n < list.length(); n++) {
            QString arg = list[n];
            if(!arg.length())
                continue;
            if(arg.at(0) == '>')
                continue;
            projstr += arg + "\n";
        }
        projstr += fileName + "\n";
        list.clear();
#ifdef SPIN
        if(isSpinProject())
            list = projectOptions->getSpinOptions();
        else if(isCProject())
#endif
            list = projectOptions->getOptions();

        foreach(QString arg, list) {
            projstr += ">"+arg+"\n";
        }
        // save project file in english
        if(file.open(QFile::WriteOnly | QFile::Text)) {
            file.write(projstr.toLatin1());
            file.close();
        }
    }
    updateProjectTree(sourcePath(projectFile)+mainFile);
}

/*
 * add a new project file
 * save new filelist and options to project.side file
 */
void MainSpinWindow::addProjectFile()
{
    if(projectFile.isEmpty() || projectModel == NULL) {
        return;
    }

    fileDialog.setDirectory(sourcePath(projectFile));

    // this is on the wish list and not finished yet
    fileDialog.setFileMode(QFileDialog::ExistingFiles);
    QStringList files = fileDialog.getOpenFileNames(this, tr("Add File"), lastPath, tr(SOURCE_FILE_TYPES));

    foreach(QString fileName, files) {
        //fileName = fileDialog.getOpenFileName(this, tr("Add File"), lastPath, tr(SOURCE_FILE_TYPES));
        /*
         * Cancel makes filename blank. If fileName is blank, don't add.
         */
        if(fileName.length() == 0)
            return;
        /*
         * Don't let users add *.* as a file name.
         */
        if(fileName.contains('*'))
            return;

        lastPath = sourcePath(fileName);

        /* hmm, should be check for source files */
        if(isOutputFile(fileName) == false) {
            QFile reader(fileName);
            reader.copy(sourcePath(projectFile)+this->shortFileName(fileName));
            addProjectListFile(this->shortFileName(fileName));
        }
    }
}

/*
 * add a new project link
 * save new filelist and options to project.side file
 */
void MainSpinWindow::addProjectLink()
{
    if(projectFile.isEmpty() || projectModel == NULL) {
        return;
    }

    fileDialog.setDirectory(sourcePath(projectFile));

    // this is on the wish list and not finished yet
    fileDialog.setFileMode(QFileDialog::ExistingFiles);
    QStringList files = fileDialog.getOpenFileNames(this, tr("Add Link to File"), lastPath, tr(SOURCE_FILE_TYPES));

    foreach(QString fileName, files) {
        //fileName = fileDialog.getOpenFileName(this, tr("Add File"), lastPath, tr(SOURCE_FILE_TYPES));
        /*
         * Cancel makes filename blank. If fileName is blank, don't add.
         */
        if(fileName.length() == 0)
            return;
        /*
         * Don't let users add *.* as a file name.
         */
        if(fileName.contains('*'))
            return;

        QDir path(projectFile.mid(0,projectFile.lastIndexOf("/")));
        fileName = path.relativeFilePath(fileName);
        lastPath = sourcePath(fileName);

        if(isOutputFile(fileName) == false) {
            if(sourcePath(fileName).compare(sourcePath(this->projectFile)) == 0)
                fileName = this->shortFileName(fileName);
            else
                fileName = this->shortFileName(fileName)+FILELINK+fileName;
            addProjectListFile(fileName);
        }
    }
}


void MainSpinWindow::addProjectLibFile()
{
    // this is on the wish list and not finished yet
    fileDialog.setFileMode(QFileDialog::ExistingFiles);
    QStringList files = fileDialog.getOpenFileNames(this, tr("Add Library File"), lastPath, tr("Library *.a Files (*.a)"));

    foreach(QString fileName, files) {
        //fileName = fileDialog.getOpenFileName(this, tr("Add File"), lastPath, tr(SOURCE_FILE_TYPES));
        /*
         * Cancel makes filename blank. If fileName is blank, don't add.
         */
        if(fileName.length() == 0)
            return;
        /*
         * Don't let users add *.* as a file name.
         */
        if(fileName.contains('*'))
            return;

        QDir path;
        fileName = path.relativeFilePath(fileName);
        lastPath = sourcePath(fileName);

        if(isOutputFile(fileName) == false) {
            QString ext = fileName.mid(fileName.lastIndexOf("."));
            if(ext.length()) {
                ext = ext.toLower();
                if(ext.compare(".a") == 0) {
                    if(isOutputFile(fileName) == false) {
                        if(sourcePath(fileName).compare(sourcePath(this->projectFile)) == 0)
                            fileName = this->shortFileName(fileName);
                        else
                            fileName = this->shortFileName(fileName)+FILELINK+fileName;
                    }
                    //fileName = this->shortFileName(fileName)+FILELINK+fileName;
                    addProjectListFile(fileName);
                }
            }
        }
    }
}

void MainSpinWindow::addProjectIncPath(const QString &inpath)
{
    QString fileName = inpath;
    if(fileName.isEmpty()) {
        fileName = QFileDialog::getExistingDirectory(this,tr("Select Include Folder"),lastPath,QFileDialog::ShowDirsOnly);
    }
    if(fileName.length() < 1)
        return;

    /*
     * Cancel makes filename blank. If fileName is blank, don't add.
     */
    if(fileName.length() == 0)
        return;
    /*
     * Don't let users add *.* as a file name.
     */
    if(fileName.contains('*'))
        return;

    QDir path(projectFile.mid(0,projectFile.lastIndexOf("/")));
    fileName = path.relativeFilePath(fileName);
    QDir test(path.path()+"/"+fileName);
    if(test.exists() && fileName.mid(2).compare("./") != 0)
        fileName = "./"+fileName;
    lastPath = sourcePath(fileName);

    QString s = QDir::fromNativeSeparators(fileName);
    if(s.length() == 0)
        return;
    if(s.indexOf('/') > -1) {
        if(s.mid(s.length()-1) != "/")
            s += "/";
    }
    //fileName = QDir::convertSeparators(s);

    if(isOutputFile(fileName) == false) {
        //if(fileName.indexOf(".") < 0)  {
            fileName = "-I " + fileName;
            addProjectListFile(fileName);
        //}
    }
}

void MainSpinWindow::addProjectLibPath(const QString &inpath)
{
    QString fileName = inpath;
    if(fileName.isEmpty()) {
        fileName = QFileDialog::getExistingDirectory(this,tr("Select Library Folder"),lastPath,QFileDialog::ShowDirsOnly);
    }
    if(fileName.length() < 1)
        return;
    /*
     * Cancel makes filename blank. If fileName is blank, don't add.
     */
    if(fileName.length() == 0)
        return;
    /*
     * Don't let users add *.* as a file name.
     */
    if(fileName.contains('*'))
        return;

    QDir path(projectFile.mid(0,projectFile.lastIndexOf("/")));
    fileName = path.relativeFilePath(fileName);
    QDir test(path.path()+"/"+fileName);
    if(test.exists() && fileName.mid(2).compare("./") != 0)
        fileName = "./"+fileName;
    lastPath = sourcePath(fileName);

    QString s = QDir::fromNativeSeparators(fileName);
    if(s.length() == 0)
        return;
    if(s.indexOf('/') > -1) {
        if(s.mid(s.length()-1) != "/")
            s += "/";
    }
    //fileName = QDir::convertSeparators(s);

    if(isOutputFile(fileName) == false) {
        //if(fileName.indexOf(".") < 0)  {
            fileName = "-L "+fileName;
            addProjectListFile(fileName);
        //}
    }
}

/*
 * delete project source file.
 * save new filelist and options to project.side file
 */
void MainSpinWindow::deleteProjectFile()
{
    QString projstr = "";
    QString fileName = "";
    QStringList list;

    if(projectModel == NULL) return;
    if(projectTree == NULL) return;

    int row = projectTree->currentIndex().row();
    if(row < 1) return;

    QVariant vs = projectModel->data(projectTree->currentIndex(), Qt::DisplayRole);
    if(vs.canConvert(QVariant::String))
    {
        fileName = vs.toString();
    }
    if(fileName.isEmpty())
        return;

    QString mainFile;

    QFile file(projectFile);
    if(file.exists()) {
        if(file.open(QFile::ReadOnly | QFile::Text)) {
            projstr = file.readAll();
            file.close();
        }
        list = projstr.split("\n");
        mainFile = list[0];
        if(fileName.compare(mainFile) == 0) {
            qDebug() << "Can't delete mainfile.";
            return;
        }
        projstr = "";
        for(int n = 0; n < list.length(); n++) {
            QString arg = list[n];
            if(!arg.length())
                continue;
            if(arg.at(0) == '>')
                continue;

            /* if we don't have a match, include the name in the project.
             * else ....
             */
            if(!n || fileName != arg) {
                projstr += arg + "\n";
            }
            /* if we have a match, check if it's a library path "-L",
             * and remove -lname from linker
             */
            else {
                if(fileName.indexOf("-L") == 0) {
                    QString s = projectOptions->getLinkOptions();
                    // arg = arg.replace("/","\\"); // test of replace only
                    /* get length of arg */
                    int alen = arg.length();
                    /* if arg has \ in it replace with / */
                    if(arg.contains("\\"))
                        arg = arg.replace("\\","/");
                    /* remove trailing / */
                    if(arg.at(alen-1) == '/')
                        arg = arg.mid(0,alen-1);
                    /* get last word in path */
                    arg = arg.mid(arg.lastIndexOf("/")+1);
                    /* if start with lib, remove lib */
                    if(arg.indexOf("lib") == 0)
                        arg = arg.mid(3);
                    /* prepend -l */
                    arg = "-l"+arg;
                    /* if the link options has our -lname, remove it. set new link options. */
                    if(s.contains(arg)) {
                        s = s.remove(arg);
                        projectOptions->setLinkOptions(s.trimmed());
                    }
                }
            }
        }
        list.clear();
#ifdef SPIN
        if(isSpinProject())
            list = projectOptions->getSpinOptions();
        else if(isCProject())
#endif
            list = projectOptions->getOptions();

        foreach(QString arg, list) {
            projstr += ">"+arg+"\n";
        }
        // save project file in english
        if(file.open(QFile::WriteOnly | QFile::Text)) {
            file.write(projstr.toLatin1());
            file.close();
        }
        else {
            qDebug() << "Project file is read only:" << file.fileName();
            file.setPermissions(QFile::WriteGroup|QFile::WriteOwner|QFile::WriteUser);
            if(file.open(QFile::WriteOnly | QFile::Text)) {
                file.write(projstr.toLatin1());
                file.close();
            }
        }

    }
    updateProjectTree(sourcePath(projectFile)+mainFile);

    for(int n = editorTabs->count(); n >= 0; n--) {
        QString s = editorTabs->tabText(n);
        if(s.endsWith("*")) {
            s = s.remove("*");
            s = s.trimmed();
        }
        if(s.compare(fileName) == 0) {
            closeTab(n);
            break;
        }
    }
}

void MainSpinWindow::showProjectFile()
{
    QString fileName;
    qDebug() << "showProjectFile" << "Total Tabs" << editorTabs->count() << "Total Editors" << editors->count();

    QVariant vs = projectModel->data(projectTree->currentIndex(), Qt::DisplayRole);
    if(vs.canConvert(QVariant::String))
    {
        fileName = vs.toString();
        fileName = fileName.trimmed();

        /* .a libraries are allowed in project list, but not .o, etc...
         */
        if(fileName.contains(".a"))
            goto showProjectFile_exit;

        /*
         * if tab is already opened, just display it
         */
        int tabs = editorTabs->count();
        for(int n = 0; n < tabs; n++) {
            QString tab = editorTabs->tabText(n);
            if(tab.contains("*")) {
                tab = tab.mid(0,tab.indexOf("*"));
                tab = tab.trimmed();
            }
            if(tab.compare(this->shortFileName(fileName)) == 0) {
                editorTabs->setCurrentIndex(n);
                projectTree->setFocus();
                goto showProjectFile_exit;
            }
        }

        /* openFileName knows how to read spin files
         * If name has FILELINK it's a link.
         */
        if(fileName.contains(FILELINK)) {
            fileName = fileName.mid(fileName.indexOf(FILELINK)+QString(FILELINK).length());
            QString relpath = sourcePath(projectFile);
            if(fileName.contains(relpath) == false) {
                fileName = sourcePath(projectFile)+fileName;
            }
            else if(fileName.indexOf("../") == 0) {
                fileName = sourcePath(projectFile)+fileName;
            }
            openFileName(fileName);
            projectTree->setFocus();
        }
        else {
            if(isCProject()) {
                openFileName(sourcePath(projectFile)+fileName);
                projectTree->setFocus();
            }
#ifdef SPIN
            else if(isSpinProject()) {

                /* Spin files can be case-insensitive. Deal with it.
                 */
                QString lib = propDialog->getSpinLibraryStr();
                QString fs = sourcePath(projectFile)+fileName;

                if(QFile::exists(fs))
                    openFileName(sourcePath(projectFile)+fileName);
                else if(QFile::exists(lib+fileName))
                    openFileName(lib+fileName);
                else {
                    QDir dir;
                    QStringList list;
                    QString shortfile = this->shortFileName(fs);
                    dir.setPath(sourcePath(projectFile));
                    list = dir.entryList();
                    foreach(QString s, list) {
                        if(s.contains(shortfile,Qt::CaseInsensitive)) {
                            openFileName(sourcePath(projectFile)+s);
                            projectTree->setFocus();
                            goto showProjectFile_exit;
                        }
                    }
                    dir.setPath(lib);
                    list = dir.entryList();
                    foreach(QString s, list) {
                        if(s.contains(shortfile,Qt::CaseInsensitive)) {
                            openFileName(lib+s);
                            projectTree->setFocus();
                            goto showProjectFile_exit;
                        }
                    }
                }

            }
#endif
        }
    }
    showProjectFile_exit:
    qDebug() << "showProjectFile_exit" << "Total Tabs" << editorTabs->count() << "Total Editors" << editors->count();
}

/*
 * save project.
 */
void MainSpinWindow::saveProject()
{
    if(projectFile.length() == 0 || projectModel == NULL) {
        QMessageBox::critical(
                this,tr("No Project"),
                tr("Can't \"Save Project\" from an empty project.")+"\n"+
                tr("Please create a new project or open an existing one."),
                QMessageBox::Ok);
        return;
    }

    /* go through project file list and save files
     */
    QFile file(projectFile);
    QString proj = "";
    if(file.open(QFile::ReadOnly | QFile::Text)) {
        proj = file.readAll();
        file.close();
    }

    proj = proj.trimmed(); // kill extra white space
    QStringList list = proj.split("\n");

    saveProjectOptions();

    for(int n = 0; n < list.length(); n++) {
        for(int tab = editorTabs->count()-1; tab > -1; tab--) {
            QString s = sourcePath(projectFile)+list.at(n);
            if(s.length() == 0)
                continue;
            if(s.at(0) == '>')
                continue;
            // save exact tab
            if(editorTabs->tabToolTip(tab).compare(s) == 0)
                saveTab(tab, false);
        }
    }
}

/*
 * save project file with options.
 */
void MainSpinWindow::saveProjectOptions()
{
    if(projectModel == NULL)
        return;

    if(projectFile.length() > 0)
        setWindowTitle(QString(ASideGuiKey)+" "+QDir::toNativeSeparators(projectFile));

#ifdef SPIN
    if(isSpinProject())
        saveSpinProjectOptions();
    else if(isCProject())
#endif
        saveManagedProjectOptions();
}

void MainSpinWindow::saveSpinProjectOptions()
{
    QString projstr = "";
    QStringList list;

    QFile file(projectFile);
    if(file.exists()) {

        /* read project file
         */
        if(file.open(QFile::ReadOnly | QFile::Text)) {
            projstr = file.readAll();
            file.close();
        }

        /* get the spin project */
        QString fileName = sourcePath(projectFile)+projstr.mid(0,projstr.indexOf("\n"));
        projstr = "";

        QString projName = shortFileName(projectFile);
        if(projectModel != NULL) delete projectModel;
        projectModel = new CBuildTree(projName, this);
#ifdef SPIN
        /* for spin-side we always parse the program and stuff the file list */
        list = spinParser.spinFileTree(fileName, propDialog->getSpinLibraryStr());
        for(int n = 0; n < list.count(); n ++) {
            QString arg = list[n];
            qDebug() << arg;
            projstr += arg + "\n";
            projectModel->addRootItem(arg);
        }

        list.clear();
        list = projectOptions->getSpinOptions();
#endif
        /* add options */
        foreach(QString arg, list) {
            if(arg.contains(ProjectOptions::board+"::"))
                projstr += ">"+ProjectOptions::board+"::"+cbBoard->currentText()+"\n";
            else
                projstr += ">"+arg+"\n";
        }

        /* save project file in english only ok */
        if(file.open(QFile::WriteOnly | QFile::Text)) {
            file.write(projstr.toLatin1());
            file.close();
        }

        projectTree->setWindowTitle(projName);
        projectTree->setModel(projectModel);
        projectTree->hide();
        projectTree->show();
    }
}

void MainSpinWindow::saveManagedProjectOptions()
{
    QString projstr = "";
    QStringList list;
    QFile file(projectFile);
    if(file.exists()) {
        /* read project file
         */
        if(file.open(QFile::ReadOnly | QFile::Text)) {
            projstr = file.readAll();
            file.close();
        }
        /* make a new project file text string.
         * first add the source files, then add options.
         * finally save file.
         */
        list = projstr.split("\n");
        list.removeDuplicates();

        projstr = "";
        /* add source files */
        for(int n = 0; n < list.length(); n++) {
            QString arg = list[n];
            if(!arg.length())
                continue;
            if(arg.at(0) == '>')
                continue;
            else
                projstr += arg + "\n";
        }

        list.clear();
        list = projectOptions->getOptions();

        /* add options */
        foreach(QString arg, list) {
            if(arg.contains(ProjectOptions::board+"::"))
                projstr += ">"+ProjectOptions::board+"::"+cbBoard->currentText()+"\n";
            else
                projstr += ">"+arg+"\n";
        }

        /* save project file in english only ok */
        if(file.open(QFile::WriteOnly | QFile::Text)) {
            file.write(projstr.toLatin1());
            file.close();
        }
    }
}
/*
 * update project tree and options by reading from project.side file
 */
void MainSpinWindow::updateProjectTree(QString fileName)
{
    QString projName = this->shortFileName(fileName);
    projName = projName.mid(0,projName.lastIndexOf("."));
    projName += SIDE_EXTENSION;

    basicPath = sourcePath(fileName);
    projectFile = basicPath+projName;
    setWindowTitle(QString(ASideGuiKey)+" "+QDir::toNativeSeparators(projectFile));

    if(projectModel != NULL) delete projectModel;
    projectModel = new CBuildTree(projName, this);
#ifdef SPIN
    if(fileName.contains(SPIN_EXTENSION,Qt::CaseInsensitive)) {
        projectOptions->setCompiler(SPIN_TEXT);
    }
    else {
        QFile proj(projectFile);
        QString type;
        if(proj.open(QFile::ReadOnly | QFile::Text)) {
            type = proj.readAll();
            proj.close();
            if(type.contains(">compiler=SPIN",Qt::CaseInsensitive))
                projectOptions->setCompiler(SPIN_TEXT);
            else if(type.contains(">compiler=C++",Qt::CaseInsensitive))
                projectOptions->setCompiler("C++");
            else
                projectOptions->setCompiler("C");
        }
    }
#else
    QFile proj(projectFile);
    QString type;
    if(proj.open(QFile::ReadOnly | QFile::Text)) {
        type = proj.readAll();
        proj.close();
        if(type.contains(">compiler=C++",Qt::CaseInsensitive))
            projectOptions->setCompiler("C++");
        else
            projectOptions->setCompiler("C");
    }
#endif

    /* in the case of a spin project, we need to parse a tree.
     * in other project cases, we use the project manager.
     */
#ifdef SPIN
    if(isSpinProject())
        updateSpinProjectTree(fileName, projName);
    else if(isCProject())
#endif
        updateManagedProjectTree(fileName, projName);
}

void MainSpinWindow::updateSpinProjectTree(QString fileName, QString projName)
{
#ifdef SPIN
    QFile file(projectFile);

    /* for spin-side we always parse the program and stuff the file list */

    QStringList flist = spinParser.spinFileTree(fileName, propDialog->getSpinLibraryStr());
    for(int n = 0; n < flist.count(); n ++) {
        QString s = flist[n];
        qDebug() << s;
        projectModel->addRootItem(s);
    }

    if(!file.exists()) {
        /* no pre-existing file, make one with source as top/main entry.
         * project file is in english.
         */
        if (file.open(QFile::WriteOnly | QFile::Text)) {
            for(int n = 0; n < flist.count(); n ++) {
                QString s = QString(flist[n]).trimmed();
                file.write((shortFileName(s)+"\n").toLatin1());
            }
            QStringList list = projectOptions->getSpinOptions();
            for(int n = 0; n < list.count(); n ++) {
                file.write(">"+QString(list[n]).toLatin1()+"\n");
            }
            file.close();
            //projectModel->addRootItem(this->shortFileName(fileName));
        }
    }
    else {

        /* pre-existing side file. read and modify it.
         * project file is in english
         */
        QString s = "";
        if (file.open(QFile::ReadOnly | QFile::Text)) {
            s = file.readAll();
            file.close();
        }

        QStringList list = s.split("\n");

        /*
         * add sorting feature - parameters get sorted too, but placement is not important.
         */
        QString main = list.at(0);
        QString mains = main.mid(0,main.lastIndexOf("."));
        QString projs = projName.mid(0,projName.lastIndexOf("."));

        if(mains.compare(projs) == 0 && list.length() > 1) {
            for(int n = list.count()-1; n > -1; n--) {
                QString s = list[n];
                if(s.indexOf(">") != 0)
                    list.removeAt(n);
            }
            for(int n = flist.count()-1; n > -1; n--) {
                QString s = QString(flist[n]).trimmed();
                list.insert(0,s);
            }
        }

        /* replace options */
        projectOptions->clearOptions();
        foreach(QString arg, list) {
            if(!arg.length())
                continue;
            if(arg.at(0) == '>') {
                if(arg.contains(ProjectOptions::board+"::")) {
                    QStringList barr = arg.split("::");
                    if(barr.count() > 0) {
                        QString board = barr.at(1);
                        projectOptions->setBoardType(board);
                        for(int n = cbBoard->count()-1; n >= 0; n--) {
                            QString cbstr = cbBoard->itemText(n);
                            if(board.compare(cbstr) == 0) {
                                cbBoard->setCurrentIndex(n);
                                break;
                            }
                        }
                    }
                }
                else {
                    projectOptions->setSpinOptions(arg);
                }
            }
        }

        file.remove();
        if(file.open(QFile::WriteOnly | QFile::Append | QFile::Text)) {
            for(int n = 0; n < list.count(); n++) {
                QString s = list[n];
                file.write(s.toLatin1()+"\n");
            }
            file.close();
        }
    }

    projectTree->setWindowTitle(projName);
    projectTree->setModel(projectModel);
    projectTree->hide();
    projectTree->show();
#endif
}

void MainSpinWindow::updateManagedProjectTree(QString fileName, QString projName)
{
    QFile file(projectFile);
    if(!file.exists()) {
        /* no pre-existing file, make one with source as top/main entry.
         * project file is in english.
         */
        if (file.open(QFile::WriteOnly | QFile::Text)) {
            file.write(this->shortFileName(fileName).toLatin1());
            projectModel->addRootItem(this->shortFileName(fileName));
            file.close();
        }
    }
    else {
        /* pre-existing side file. read and modify it.
         * project file is in english
         */
        QString s = "";
        if (file.open(QFile::ReadOnly | QFile::Text)) {
            s = file.readAll();
            file.close();
        }
        QStringList list = s.split("\n");

        /*
         * add sorting feature - parameters get sorted too, but placement is not important.
         */
        QString main = list.at(0);
        QString mains = main.mid(0,main.lastIndexOf("."));
        QString projs = projName.mid(0,projName.lastIndexOf("."));
        if(mains.compare(projs) == 0 && list.length() > 1) {
            QString s;
            list.removeAt(0);
            list.sort();
            s = list.at(0);
            if(s.length() == 0)
                list.removeAt(0);
            list.insert(0,main);
        }
        /* replace options */
        projectOptions->clearOptions();
        foreach(QString arg, list) {
            if(!arg.length())
                continue;
            if(arg.at(0) == '>') {
                if(arg.contains(ProjectOptions::board+"::")) {
                    QStringList barr = arg.split("::");
                    if(barr.count() > 0) {
                        QString board = barr.at(1);
                        projectOptions->setBoardType(board);
                        for(int n = cbBoard->count()-1; n >= 0; n--) {
                            QString cbstr = cbBoard->itemText(n);
                            if(board.compare(cbstr) == 0) {
                                cbBoard->setCurrentIndex(n);
                                break;
                            }
                        }
                    }
                }
                else {
                    projectOptions->setOptions(arg);
                }
            }
            else {
                projectModel->addRootItem(arg);
            }
        }
    }

    projectTree->setWindowTitle(projName);
    projectTree->setModel(projectModel);
    projectTree->hide();
    projectTree->show();
}

/*
 * show assembly for a .c file
 */
void MainSpinWindow::showAssemblyFile()
{
    QString outputPath = builder->sourcePath(projectFile)+builder->getOutputPath(projectFile);
    QString fileName;
    QVariant vs = projectModel->data(projectTree->currentIndex(), Qt::DisplayRole);
    if(vs.canConvert(QVariant::String))
        fileName = vs.toString();

    if(makeDebugFiles(fileName))
        return;

    QString outfile = fileName.mid(0,fileName.lastIndexOf("."));
    if(outfile.contains(FILELINK)) {
        outfile = outfile.mid(outfile.indexOf(FILELINK)+QString(FILELINK).length());
        if(outfile.contains("/")) {
            outfile = outfile.mid(outfile.lastIndexOf("/")+1);
        }
        openFileName(outputPath+outfile+SHOW_ASM_EXTENTION);
    }
    else {
        openFileName(outputPath+outfile+SHOW_ASM_EXTENTION);
    }
    qDebug() << "outputPath:" << outputPath << "outfile:" << outfile;
}

void MainSpinWindow::showMapFile()
{
    QString outputPath = builder->sourcePath(projectFile)+builder->getOutputPath(projectFile);
    QString fileName;
    QModelIndex index = projectModel->index(0,0);
    QVariant vs = projectModel->data(index, Qt::DisplayRole);
    if(vs.canConvert(QVariant::String))
        fileName = vs.toString();

    QString outfile = fileName.mid(0,fileName.lastIndexOf("."));
    runBuild("-Xlinker -Map="+outputPath+outfile+SHOW_MAP_EXTENTION);

    openFileName(outputPath+outfile+SHOW_MAP_EXTENTION);
}

/*
 * make debug info for a .c file
 */
int MainSpinWindow::makeDebugFiles(QString fileName)
{
    int rc = -1;

    if(fileName.length() == 0)
        return rc;

    /* save files before compiling debug info */
    checkAndSaveFiles();
    selectBuilder();
    rc = builder->makeDebugFiles(fileName, projectFile, aSideCompiler);

    return rc;
}

bool MainSpinWindow::rtsReset()
{
    bool rts = false;
    if(this->propDialog->getResetType() == Properties::CFG) {
        QString bname = this->cbBoard->currentText();
        ASideBoard* board = aSideConfig->getBoardData(bname);
        if(board != NULL) {
            QString reset = board->get(ASideBoard::reset);
            if(reset.length() && reset.contains("RTS",Qt::CaseInsensitive)) {
                rts = true;
            }
        }
    }
    else if(this->propDialog->getResetType() == Properties::RTS) {
        rts = true;
    }
    return rts;
}

void MainSpinWindow::findChip()
{
    if(btnConnected->isChecked())
        return;

    compileStatus->setPlainText("Identifying Propellers ...\n");

    if(rtsReset())
        propId.setRtsReset();
    else
        propId.setDtrReset();

    int indx = cbPort->currentIndex();
    this->enumeratePorts();
    int size = cbPort->count();
    if(indx < size)
        cbPort->setCurrentIndex(indx);

    for (int n = 1; n < size; n++) {
        QString mp;
        mp = cbPort->itemText(n);
        int rc = propId.isDevice(mp);
        if(rc < 0) {
            compileStatus->appendPlainText("  Port "+mp+" is busy.");
        } else if(rc > 0) {
            compileStatus->appendPlainText("  Propeller found on "+mp+".");
        } else {
            compileStatus->appendPlainText("  Propeller not found on "+mp+".");
        }
    }
}

QString MainSpinWindow::serialPort()
{
    int portIndex = cbPort->currentIndex();
    if(cbPort->currentText().compare(AUTO_PORT) == 0) {
        if(rtsReset())
            propId.setRtsReset();
        else
            propId.setDtrReset();

        //compileStatus->setPlainText("Finding first available propeller ... ");
        int size = cbPort->count();

        for (int n = 1; n < size; n++) {
            QString mp;
            mp = cbPort->itemText(n);
            int rc = propId.isDevice(mp);
            if(rc > 0) {
                //compileStatus->appendPlainText("Propeller found on "+mp+".");
                portIndex = n;
                break;
            }
        }
    }
    return(cbPort->itemText(portIndex));
}

void MainSpinWindow::enumeratePortsEvent()
{
    enumeratePorts();
    QApplication::processEvents();
    int len = this->cbPort->count();

    QString lastTermPort = term->getLastConnectedPortName();
    bool terminalOpen= term->isVisible();
#if 0
    bool lastCbPortFound = false;
    for(int n = this->cbPort->count()-1; n > -1; n--) {
        QString name = this->cbPort->itemText(n);
        if(!name.compare(lastCbPort)) {
            lastCbPortFound = true;
            break;
        }
    }
    //if(!lastCbPortFound) lastCbPort = plPortName;
#endif
    // need to check if the port we are using disappeared.
    if(this->btnConnected->isChecked()) {
        bool notFound = true;
        QString plPortName = this->term->getPortName();
        for(int n = this->cbPort->count()-1; n > -1; n--) {
            QString name = cbPort->itemText(n);
            if(!name.compare(plPortName)) {
                // if port found, set cbport name
                cbPort->setCurrentIndex(n);
                // always reselect the last port set
                lastCbPort = plPortName;
                notFound = false;
            }
        }
        if(notFound) {
            // disable port which saves last port
            btnConnected->setChecked(false);
            term->setPortEnabled(false);
            // close port separately because of different OS requirements
            portListener->close();
            // don't use connectButton() because it hides the terminal
        }
    }
    else if(lastTermPort.length() > 0) {
        for(int n = this->cbPort->count()-1; n > -1; n--) {
            QString name = cbPort->itemText(n);
            if(!name.compare(lastTermPort)) {
                if(terminalOpen) {
                    // old port found
                    cbPort->setCurrentIndex(n);
                    // always reselect the last port set
                    lastCbPort = lastTermPort;
                    // make sure we are using lastTermport
#ifdef ENUM_INIT_LASTTERMPORT
                    portListener->init(lastTermPort,portListener->getBaudRate(), getWxPortIpAddr(serialPort()));
#endif
                    // reopen port with connect button
                    btnConnected->setChecked(true);
                    connectButton();
                }
                else {
                    if(lastCbPort.length() > 0) {
                        for(int n = this->cbPort->count()-1; n > -1; n--) {
                            QString name = this->cbPort->itemText(n);
                            if(!name.compare(lastCbPort)) {
                                cbPort->setCurrentIndex(n);
                                // make sure we are using lastCbPort
#ifdef ENUM_INIT_LASTCBPORT
                                portListener->init(lastCbPort,portListener->getBaudRate(), getWxPortIpAddr(serialPort()));
#endif
                                term->setLastConnectedPortName(lastCbPort);
                                break;
                            }
                        }
                    }
                    if(this->isActiveWindow()) {
                        this->cbPort->showPopup();
                    }
                }
            }
        }
    }
    else if(len > 1) {
        if(lastCbPort.length() > 0) {
            for(int n = this->cbPort->count()-1; n > -1; n--) {
                QString name = this->cbPort->itemText(n);
                if(!name.compare(lastCbPort)) {
                    cbPort->setCurrentIndex(n);
                    break;
                }
            }
        }
        if(this->isActiveWindow()) {
            this->cbPort->showPopup();
        }
        if(terminalOpen) term->setPortEnabled(true);
    }
    else if(len > 0) {
        if(terminalOpen) term->setPortEnabled(true);
        if (len == 1 && cbPort->currentText().length() == 0)
            cbPort->setCurrentIndex(0);
    }

}

QString MainSpinWindow::getWxPortIpAddr(QString wxname)
{
    foreach (WxPortInfo info, wxPorts) {
        if(info.portName.compare(wxname) == 0) {
            return info.ipAddr;
        }
    }
    return "";
}

#ifdef ESP8266_MODULE
QList<WxPortInfo> MainSpinWindow::getWxPorts(void)
{
    QStringList args;
    args.append("-W");

    wxProcess->setProperty("Name", QVariant(aSideLoader));
    wxProcess->setProperty("IsLoader", QVariant(true));

    wxProcess->setProcessChannelMode(QProcess::MergedChannels);
    //wxProcess->setWorkingDirectory(sourcePath(fileName));

    procMutex.lock();
    procDone = false;
    procMutex.unlock();

    wxPortString = "";
    wxPorts.clear();

    statusDialog->init("Searching", "Searching for ports");

    qDebug() << aSideLoader << args;

    wxProcess->start(aSideLoader, args);

    while(procDone == false)
        QApplication::processEvents();

    statusDialog->stop();

    //QString output = wxProcess->readAllStandardOutput();
    //qDebug() << output;
    return wxPorts;
}
#else
QList<WxPortInfo> MainSpinWindow::getWxPorts(void)
{
    QStringList args;
    args.append("-X");

    wxProcess->setProperty("Name", QVariant(aSideLoader));
    wxProcess->setProperty("IsLoader", QVariant(true));

    wxProcess->setProcessChannelMode(QProcess::MergedChannels);
    //wxProcess->setWorkingDirectory(sourcePath(fileName));

    procMutex.lock();
    procDone = false;
    procMutex.unlock();

    wxPortString = "";
    wxPorts.clear();

    statusDialog->init("Searching", "Searching for ports");

    qDebug() << aSideLoader << args;

    wxProcess->start(aSideLoader, args);

    while(procDone == false)
        QApplication::processEvents();

    statusDialog->stop();

    //QString output = wxProcess->readAllStandardOutput();
    //qDebug() << output;
    return wxPorts;
}
#endif

void MainSpinWindow::enumeratePorts()
{
    int lastIndex = cbPort->currentIndex();

    disconnect(cbPort,SIGNAL(currentIndexChanged(int)),this,SLOT(setCurrentPort(int)));

    if(cbPort != NULL) cbPort->clear();

    friendlyPortName.clear();

    portListener->close();

    cbPort->addItem("");
    QApplication::processEvents();

#ifdef ENABLE_AUTO_PORT
    friendlyPortName.append(AUTO_PORT);
#endif
    QList<QextPortInfo> ports = QextSerialEnumerator::getPorts();
    QStringList stringlist;
    QString name;
    stringlist << "List of ports:";
    for (int i = 0; i < ports.size(); i++) {
        stringlist << "port name:" << ports.at(i).portName;
        stringlist << "friendly name:" << ports.at(i).friendName;
        stringlist << "physical name:" << ports.at(i).physName;
        stringlist << "enumerator name:" << ports.at(i).enumName;
        stringlist << "vendor ID:" << QString::number(ports.at(i).vendorID, 16);
        stringlist << "product ID:" << QString::number(ports.at(i).productID, 16);
        stringlist << "===================================";
#if defined(Q_OS_WIN)
        name = ports.at(i).portName;
        if(ports.at(i).friendName.length() > 0) {
            if(ports.at(i).friendName.contains("Bluetooth", Qt::CaseInsensitive)) {
                continue;
            }
        }
        if(name.lastIndexOf('\\') > -1)
            name = name.mid(name.lastIndexOf('\\')+1);
        if(name.contains(QString("LPT"),Qt::CaseInsensitive) == false) {
            friendlyPortName.append(ports.at(i).friendName);
            cbPort->addItem(name);
        }
#elif defined(Q_OS_MAC)
        name = ports.at(i).portName;
        if(ports.at(i).physName.length() > 0) {
            if(ports.at(i).physName.contains("Bluetooth", Qt::CaseInsensitive)) {
                continue;
            }
        }
        if(name.indexOf("usbserial",0,Qt::CaseInsensitive) > -1) {
            friendlyPortName.append(ports.at(i).friendName);
            cbPort->addItem(name);
        }
#else
        QString ttys = "ttyS";

        name = ports.at(i).physName;
        friendlyPortName.append(ports.at(i).friendName);

        qDebug() << "USB" << name.indexOf("usb",0,Qt::CaseInsensitive);

        // ttyUSBn
        if(name.indexOf("ttyusb",0,Qt::CaseInsensitive) > -1) {
            cbPort->addItem(name);
            qDebug() << "Added " << name;
        }
        // ttyACMn
        else if(name.indexOf("ttyacm",0,Qt::CaseInsensitive) > -1) {
            cbPort->addItem(name);
            qDebug() << "Added " << name;
	}
        // ttyAMAn
        else if(name.indexOf("ttyama",0,Qt::CaseInsensitive) > -1) {
            cbPort->addItem(name);
            qDebug() << "Added " << name;
	}
        // ttySn
        else if(name.indexOf(ttys,0,Qt::CaseInsensitive) > -1) {
            /* just allow ttyS0 - ttyS9 */
            QString ttysNum = name.mid(ttys.length());
            int ttyslen = ttysNum.length();
            //qDebug() << "enumerateports " << name << ttysNum << ttyslen;
            if (ttyslen > 5 && ttyslen < 7) {
                cbPort->addItem(name);
                qDebug() << "Added " << name;
            }
        }
        else
            cbPort->addItem(name);
#endif
        qDebug() << "enumerateports" << name;
    }

#ifdef ENABLE_WXLOADER
    getWxPorts();
    foreach (WxPortInfo wx, wxPorts) {
        if (cbPort->findText(wx.portName) > -1) {
            continue;
        }
        friendlyPortName.append(wx.portName);
        cbPort->addItem(wx.portName);
    }
#endif

#if 1
    cbPort->removeItem(0);
#else
    int cblen = cbPort->count()-1;
    QString first = cbPort->itemText(0);

    qDebug() << cblen << first;

    if (first.length() > -1) cbPort->removeItem(0);
#endif
    cbPort->setCurrentIndex(lastIndex);
#ifdef ENABLE_AUTO_PORT
    cbPort->insertItem(0,AUTO_PORT);
#endif
    this->cbPort->hidePopup();
    if(!cbPort->count()) {
        btnConnected->setCheckable(false);
    }
    else {
        btnConnected->setCheckable(true);
    }
    //connect(cbPort,SIGNAL(currentIndexChanged(int)),this,SLOT(setCurrentPort(int)));

}

void MainSpinWindow::connectButton()
{
    if(btnConnected->isChecked()) {
        portName = serialPort();
        if(portName.compare(AUTO_PORT) == 0) {
            btnConnected->setChecked(false);
            return;
        }
        term->getEditor()->setPortEnable(true);
        term->show();
        term->setPortEnabled(true);
        term->setPortName(portName);
        term->activateWindow();
        term->getEditor()->setFocus();
        portListener->init(portName, term->getBaud(), getWxPortIpAddr(serialPort()));
        portListener->open();
        cbPort->setEnabled(false);
    }
    else {
        portListener->close();
        term->hide();
        cbPort->setEnabled(true);
    }
}

void MainSpinWindow::menuActionConnectButton()
{

    if(btnConnected->isChecked() == false) {
        btnConnected->setChecked(true);
        connectButton();
    }
}

void MainSpinWindow::portRename()
{
    bool ok;
    if (getWxPortIpAddr(cbPort->currentText()).length() == 0) {
        QMessageBox::warning(this, tr("Can't Rename Port"), tr("Can only rename WX ports."));
        return;
    }

    QString text = QInputDialog::getText(this,
                       tr("Rename Port"), tr("Port Name"),
                       QLineEdit::Normal, cbPort->currentText(), &ok);

    if (ok && text.length() > 0) {
        text = text.replace(" ", "-");
        text = text.replace("'", "");

        portListener->close();
        int rc = runLoader("-n '"+text+"'");
        if (rc == 0) {
            QSize size = cbPort->size();
            cbPort->setEditable(true);
            int s = cbPort->fontInfo().pixelSize();
            int w = text.length()*s;
            if (size.width() < w) size.setWidth(w);
            int ndx = cbPort->currentIndex();
            cbPort->setItemText(ndx, text);
            //cbPort->setCurrentText(text);
            cbPort->setToolTip(text);
            QApplication::processEvents();
            cbPort->setEditable(false);
            QApplication::processEvents();
        }
    }
}

void MainSpinWindow::portResetButton()
{
    bool rts = false;
    QString port = serialPort();
    if(port.length() == 0) {
        QMessageBox::information(this, tr("Port Required"), tr("Please select a port"), QMessageBox::Ok);
        return;
    }
    //Properties::Reset res = this->propDialog->getResetType();

    if(this->propDialog->getResetType() == Properties::CFG) {
        QString bname = this->cbBoard->currentText();
        ASideBoard* board = aSideConfig->getBoardData(bname);
        if(board != NULL) {
            QString reset = board->get(ASideBoard::reset);
            if(reset.length() == 0)
                rts = false;
            else
            if(reset.contains("RTS",Qt::CaseInsensitive))
                rts = true;
            else
            if(reset.contains("DTR",Qt::CaseInsensitive))
                rts = false;
            else
                rts = false;
        }
        else {
            rts = false;
        }
    }
    else
    if(this->propDialog->getResetType() == Properties::RTS) {
        rts = true;
    }
    else
    if(this->propDialog->getResetType() == Properties::DTR) {
        rts = false;
    }

    // We need to reopen this sucker for reset if we have 2+ ports.
    // portListener->init(port, BAUD115200, getWxPortIpAddr(serialPort()));  // signals get hooked up internally

    QString savePortName = portListener->getPortName();

    /* if port name is AUTO, just reset the first port we find. */
    portName = serialPort();

    bool isopen = portListener->isOpen();

    if (getWxPortIpAddr(portName).length() > 0) {
        portListener->close();
        runLoader("-R");
        if (isopen) {
            portListener->init(savePortName, term->getBaud(), getWxPortIpAddr(savePortName));
            portListener->open();
        }
    }
    else {
        if(isopen == false)
        {
            if(savePortName.compare(portName) != 0) {
#ifdef ENUM_INIT_PORTNAME
                portListener->init(portName, term->getBaud(), getWxPortIpAddr(serialPort()));
#endif
            }

            portListener->open();
            resetPort(rts);
            portListener->close();

            if(savePortName.compare(portName) != 0) {
#ifdef ENUM_INIT_PORTNAME
                portListener->init(savePortName, term->getBaud(), getWxPortIpAddr(serialPort()));
#endif
            }
        }
        else {
            resetPort(rts);
        }
    }
}

void MainSpinWindow::resetPort(bool rts)
{
    if(rts) {
        portListener->setRts(true);
        Sleeper::ms(50);
        portListener->setRts(false);
    }
    else {
        portListener->setDtr(true);
        Sleeper::ms(50);
        portListener->setDtr(false);
    }
}

QString MainSpinWindow::shortFileName(QString fileName)
{
    QString rets;
    if(fileName.indexOf('/') > -1)
        rets = fileName.mid(fileName.lastIndexOf('/')+1);
    else if(fileName.indexOf('\\') > -1)
        rets = fileName.mid(fileName.lastIndexOf('\\')+1);
    else
        rets = fileName.mid(fileName.lastIndexOf(aSideSeparator)+1);
    return rets;
}

/*
 * same as initBoardTypes except saves last used cbBoard text
 * and restores after load. initBoardTypes restores board from registry
 */
void MainSpinWindow::reloadBoardTypes()
{
    QString boardName = cbBoard->currentText();

    cbBoard->clear();

    QFile file;
    if(file.exists(aSideCfgFile))
    {
        /* load boards in case there were changes */
        aSideConfig->loadBoards(aSideCfgFile);
    }

    /* get board types */
    QStringList boards = aSideConfig->getBoardNames();
    boards.sort();
    for(int n = 0; n < boards.count(); n++)
        cbBoard->addItem(boards.at(n));

    /* setup the first board displayed in the combo box */
    if(cbBoard->count() > 0) {
        int ndx = 0;
        if(boardName.length() != 0) {
            for(int n = cbBoard->count()-1; n > -1; n--) {
                if(cbBoard->itemText(n) == boardName) {
                    ndx = n;
                    break;
                }
            }
        }
        setCurrentBoard(ndx);
    }
}

void MainSpinWindow::initBoardTypes()
{
    int boardIndex = cbBoard->currentIndex();
    cbBoard->clear();

    QFile file;
    if(file.exists(aSideCfgFile))
    {
        /* load boards in case there were changes */
        aSideConfig->loadBoards(aSideCfgFile);
    }

    /* get board types */
    QStringList boards = aSideConfig->getBoardNames();
    boards.sort();
    for(int n = 0; n < boards.count(); n++)
        cbBoard->addItem(boards.at(n));

    QVariant lastboardv = settings->value(lastBoardNameKey);
    /* read last board/port to make user happy */
#if 1
    if (boardIndex > -1) {
        boardName = cbBoard->itemText(boardIndex);
    }
    else
#endif
    if(lastboardv.canConvert(QVariant::String))
        boardName = lastboardv.toString();

    /* setup the first board displayed in the combo box */
    if(cbBoard->count() > 0) {
        int ndx = 0;
        if(boardName.length() != 0) {
            for(int n = cbBoard->count()-1; n > -1; n--)
                if(cbBoard->itemText(n) == boardName)
                {
                    ndx = n;
                    break;
                }
        }
        setCurrentBoard(ndx);
    }
}

int MainSpinWindow::setupEditor()
{
    Editor *editor = new Editor(gdb, &spinParser, this);
    editor->setTabStopWidth(propDialog->getTabSpaces()*10);

    /* font is user's preference */
    editor->setFont(editorFont); // setFont doesn't work very well but style sheet does
    QString fs = QString("font: %1pt \"%2\";").arg(editorFont.pointSize()).arg(editorFont.family());
    editor->setStyleSheet(fs);
    editor->setLineWrapMode(Editor::NoWrap);
    connect(editor,SIGNAL(textChanged()),this,SLOT(fileChanged()));
    connect(editor, SIGNAL(saveEditorFile()), this, SLOT(saveEditor()));
    editors->append(editor);
    qDebug() << "setEditor" << editors->count();
    return editors->count()-1;
}

void MainSpinWindow::setEditorTab(int num, QString shortName, QString fileName, QString text)
{
    Editor *editor = editors->at(num);
    fileChangeDisable = true;
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    disconnect(editor,SIGNAL(textChanged()),this,SLOT(fileChanged()));
    editor->setPlainText(text);
    editor->setHighlights(shortName);
    connect(editor, SIGNAL(textChanged()),this,SLOT(fileChanged()));
    QApplication::restoreOverrideCursor();
    fileChangeDisable = false;
    editorTabs->setTabText(num,shortName);
    editorTabs->setTabToolTip(num,fileName);
    editorTabs->setCurrentIndex(num);
    currentTabChanged();
    qDebug() << "setEditorTab" << fileName << num << "Total Tabs" << editorTabs->count() << "Total Editors" << editors->count();
}

/*
 * TODO: Why don't icons show up in linux? deferred.
 * QtCreator has the same problem Windows OK, Linux not OK.
 */
void MainSpinWindow::setupFileMenu()
{
    fileMenu = new QMenu(tr("&File"), this);
    menuBar()->addMenu(fileMenu);

    fileMenu->addAction(QIcon(":/images/newfile.png"), tr(NewFile), this, SLOT(newFile()), QKeySequence::New);
    fileMenu->addAction(QIcon(":/images/openfile.png"), tr(OpenFile), this, SLOT(openFile()), QKeySequence::Open);
    fileMenu->addAction(QIcon(":/images/savefile.png"), tr(SaveFile), this, SLOT(saveFile()), QKeySequence::Save);
    fileMenu->addAction(QIcon(":/images/saveasfile2.png"), tr(SaveAsFile), this, SLOT(saveAsFile()),QKeySequence::SaveAs);

    fileMenu->addAction(QIcon(":/images/Delete.png"),tr(CloseFile), this, SLOT(closeFile()));
    fileMenu->addAction(QIcon(":/images/DeleteAll.png"),tr("Close All"), this, SLOT(closeAll()));
    fileMenu->addAction(QIcon(":/images/print.png"), tr("Print"), this, SLOT(printFile()), QKeySequence::Print);

    // recent file actions
    separatorFileAct = fileMenu->addSeparator();

    for (int i = 0; i < MaxRecentFiles; ++i) {
        recentFileActs[i] = new QAction(this);
        recentFileActs[i]->setVisible(false);
        connect(recentFileActs[i], SIGNAL(triggered()),this, SLOT(openRecentFile()));
    }

    for (int i = 0; i < MaxRecentFiles; ++i)
        fileMenu->addAction(recentFileActs[i]);

    updateRecentFileActions();

    fileMenu->addSeparator();

    fileMenu->addAction(QIcon(":/images/exit.png"), tr("E&xit"), this, SLOT(quitProgram()), QKeySequence::Quit);

    projMenu = new QMenu(tr("&Project"), this);
    menuBar()->addMenu(projMenu);

    projMenu->addAction(QIcon(":/images/newproj.png"), tr("New"), this, SLOT(newProject()), Qt::CTRL+Qt::ShiftModifier+Qt::Key_N);
    projMenu->addAction(QIcon(":/images/openproj.png"), tr("Open"), this, SLOT(openProject()), Qt::CTRL+Qt::ShiftModifier+Qt::Key_O);
    projMenu->addAction(QIcon(":/images/saveproj.png"), tr("Save"), this, SLOT(saveProject()), Qt::CTRL+Qt::ShiftModifier+Qt::Key_S);
    projMenu->addAction(QIcon(":/images/saveasproj.png"), tr(SaveAsProject), this, SLOT(saveAsProject()), Qt::CTRL+Qt::ShiftModifier+Qt::Key_A);
    projMenu->addAction(QIcon(":/images/zip2.png"), tr(ZipProject), this, SLOT(zipProject()));

    projMenu->addAction(QIcon(":/images/AddFileCopy.png"), tr(AddFileCopy), this, SLOT(addProjectFile()));
#ifdef ENABLE_ADD_LINK
    projMenu->addAction(QIcon(":/images/AddFileLink.png"), tr(AddFileLink), this,SLOT(addProjectLink()));
#endif
    projMenu->addAction(QIcon(":/images/AddProjectPath.png"), tr(AddIncludePath), this,SLOT(addProjectIncPath()));
    projMenu->addAction(QIcon(":/images/AddLibraryPath.png"), tr(AddLibraryPath), this,SLOT(addProjectLibPath()));

    projMenu->addSeparator();
    projMenu->addAction(QIcon(":/images/closeproj.png"), tr(CloseProject), this, SLOT(closeProject()), Qt::CTRL+Qt::ShiftModifier+Qt::Key_X);
    projMenu->addAction(QIcon(":/images/project.png"), tr(SetProject), this, SLOT(setProject()), Qt::Key_F4);
    //projMenu->addAction(QIcon(":/images/hardware.png"), tr("Load Board Types"), this, SLOT(hardware()), Qt::Key_F6);
    projMenu->addAction(QIcon(":/images/addlib.png"), tr(AddLib), this, SLOT(addLib()), 0);
    projMenu->addAction(QIcon(":/images/addtab.png"), tr(AddTab), this, SLOT(addTab()), 0);
    projMenu->addAction(QIcon(":/images/opentab.png"), tr(OpenTab), this, SLOT(openTab()), 0);

    // recent project actions
    separatorProjectAct = projMenu->addSeparator();

    for (int i = 0; i < MaxRecentProjects; ++i) {
        recentProjectActs[i] = new QAction(this);
        recentProjectActs[i]->setVisible(false);
        connect(recentProjectActs[i], SIGNAL(triggered()),this, SLOT(openRecentProject()));
    }

    for (int i = 0; i < MaxRecentProjects; ++i)
        projMenu->addAction(recentProjectActs[i]);

    updateRecentProjectActions();

    projMenu->addSeparator();
    //projMenu->addAction(QIcon(":/images/properties.png"), tr("Properties"), this, SLOT(properties()), Qt::Key_F6);

    QMenu *editMenu = new QMenu(tr("&Edit"), this);
    menuBar()->addMenu(editMenu);

    editMenu->addAction(QIcon(":/images/copy.png"), tr("Copy"), this, SLOT(copyFromFile()), QKeySequence::Copy);
    editMenu->addAction(QIcon(":/images/cut.png"), tr("Cut"), this, SLOT(cutFromFile()), QKeySequence::Cut);
    editMenu->addAction(QIcon(":/images/paste.png"), tr("Paste"), this, SLOT(pasteToFile()), QKeySequence::Paste);

/*
    editMenu->addSeparator();
    editMenu->addAction(tr("Edit Command"), this, SLOT(editCommand()), Qt::CTRL + Qt::Key_Colon);
    editMenu->addAction(tr("System Command"), this, SLOT(systemCommand()), Qt::CTRL + Qt::Key_Semicolon);
*/
    editMenu->addSeparator();
    editMenu->addAction(QIcon(":/images/find.png"), tr("&Find and Replace"), this, SLOT(replaceInFile()), QKeySequence::Find);

    editMenu->addSeparator();
    editMenu->addAction(QIcon(":/images/redo.png"), tr("&Redo"), this, SLOT(redoChange()), QKeySequence::Redo);
    editMenu->addAction(QIcon(":/images/undo.png"), tr("&Undo"), this, SLOT(undoChange()), QKeySequence::Undo);

    toolsMenu = new QMenu(tr("&Tools"), this);
    menuBar()->addMenu(toolsMenu);

    simpleViewIcon = QIcon(":/images/ViewSimple.png");
    projectViewIcon = QIcon(":/images/ViewProject.png");

    //
    // SimpleView/ProjectView must be the first item in the toolsMenu!
    //
    QString viewstr = this->simpleViewType ? tr(ProjectView) : tr(SimpleView);
#ifdef ALWAYS_ALLOW_PROJECT_VIEW
    if (this->simpleViewType) {
        toolsMenu->addAction(projectViewIcon, viewstr,this,SLOT(toggleSimpleView()));
    }
    else {
        toolsMenu->addAction(simpleViewIcon, viewstr,this,SLOT(toggleSimpleView()));
    }
#else
    toolsMenu->addAction(this->simpleViewType ? projectViewIcon : simpleViewIcon,
                         viewstr,this,SLOT(toggleSimpleView()));
#endif
    toolsMenu->addSeparator();
    enableProjectView(allowProjectView);

    // connect projects check-box to our enable.
    connect(propDialog,SIGNAL(enableProjectView(bool)),this,SLOT(enableProjectView(bool)));

    if(ctags->enabled()) {
        toolsMenu->addAction(QIcon(":/images/back.png"),tr("Go &Back"), this, SLOT(prevDeclaration()), QKeySequence::Back);
        toolsMenu->addAction(QIcon(":/images/forward.png"),tr("Browse Declaration"), this, SLOT(findDeclaration()), QKeySequence::Forward);
    }

    toolsMenu->addAction(QIcon(":/images/NextTab2.png"), tr("Next Tab"), this, SLOT(changeTab(bool)),QKeySequence::NextChild);

    toolsMenu->addSeparator();
    toolsMenu->addAction(QIcon(":/images/FontTT.png"), tr("Font"), this, SLOT(fontDialog()));
    //toolsMenu->addAction(QIcon(":/images/resize-plus.png"), tr("Bigger Font"), this, SLOT(fontBigger()), QKeySequence::ZoomIn);
    //toolsMenu->addAction(QIcon(":/images/resize-plus.png"), tr("Bigger Font"), this, SLOT(fontBigger()), QKeySequence(Qt::CTRL+Qt::Key_Equal));
    toolsMenu->addAction(QIcon(":/images/resize-minus.png"), tr("Smaller Font"), this, SLOT(fontSmaller()), QKeySequence::ZoomOut);

    /* special provision for bigger fonts to use default ZoomIn or Ctrl+= */
    QAction *bigger = new QAction(QIcon(":/images/resize-plus.png"), tr("Bigger Font"), this);
    QList<QKeySequence> biggerKeys;
    biggerKeys.append(QKeySequence::ZoomIn);
    biggerKeys.append(QKeySequence(Qt::CTRL+Qt::Key_Equal));
    bigger->setShortcuts(biggerKeys);
    connect(bigger,SIGNAL(triggered()),this,SLOT(fontBigger()));

    /* insert action before smaller font action */
    QList<QAction*> alist = toolsMenu->actions();
    QAction *last = alist.last();
    toolsMenu->insertAction(last,bigger);

    toolsMenu->addSeparator();
    // added for simple view, not necessary anymore
    //toolsMenu->addAction(QIcon(":/images/hardware.png"), tr("Reload Board List"), this, SLOT(reloadBoardTypes()));
    toolsMenu->addAction(QIcon(":/images/rename-port.png"), tr("Rename Port"), this, SLOT(portRename()));
    toolsMenu->addAction(QIcon(":/images/update.png"), tr("Update Workspace"), this, SLOT(updateWorkspace()));
    toolsMenu->addAction(QIcon(":/images/properties.png"), tr("Properties"), this, SLOT(properties()), Qt::Key_F6);
#ifdef ENABLE_AUTO_PORT
    toolsMenu->addAction(tr("Identify Propeller"), this, SLOT(findChip()), Qt::Key_F7);
#endif
    programMenu = new QMenu(tr("&Program"), this);
    menuBar()->addMenu(programMenu);

#if 1
// CHANGE_ALL_MAC_PROGRAM_KEYS
#ifdef Q_OS_MAC
    programMenu->addAction(QIcon(":/images/RunConsole2.png"), tr("Run with Terminal"), this, SLOT(programDebug()), Qt::ALT+Qt::Key_F8);
    programMenu->addAction(QIcon(":/images/build3.png"), tr("Build Project"), this, SLOT(programBuild()), Qt::ALT+Qt::Key_F9);
    programMenu->addAction(QIcon(":/images/run.png"), tr("Load RAM && Run"), this, SLOT(programRun()), Qt::ALT+Qt::Key_F10);
    programMenu->addAction(QIcon(":/images/burnee.png"), tr("Load EEPROM && Run"), this, SLOT(programBurnEE()), Qt::ALT+Qt::Key_F11);
#else
    programMenu->addAction(QIcon(":/images/RunConsole2.png"), tr("Run with Terminal"), this, SLOT(programDebug()), Qt::Key_F8);
    programMenu->addAction(QIcon(":/images/build3.png"), tr("Build Project"), this, SLOT(programBuild()), Qt::Key_F9);
    programMenu->addAction(QIcon(":/images/run.png"), tr("Load RAM && Run"), this, SLOT(programRun()), Qt::Key_F10);
    programMenu->addAction(QIcon(":/images/burnee.png"), tr("Load EEPROM && Run"), this, SLOT(programBurnEE()), Qt::Key_F11);
#endif
#else
/* Don't CHANGE_ALL_MAC_PROGRAM_KEYS */
    programMenu->addAction(QIcon(":/images/RunConsole2.png"), tr("Run with Terminal"), this, SLOT(programDebug()), Qt::Key_F8);
    programMenu->addAction(QIcon(":/images/build3.png"), tr("Build Project"), this, SLOT(programBuild()), Qt::Key_F9);
    programMenu->addAction(QIcon(":/images/run.png"), tr("Load RAM && Run"), this, SLOT(programRun()), Qt::Key_F10);
#ifdef Q_OS_MAC
    programMenu->addAction(QIcon(":/images/burnee.png"), tr("Load EEPROM && Run"), this, SLOT(programBurnEE()), Qt::ALT+Qt::Key_F11);
#else
    programMenu->addAction(QIcon(":/images/burnee.png"), tr("Load EEPROM && Run"), this, SLOT(programBurnEE()), Qt::Key_F11);
#endif
#endif
/* CHANGE_ALL_MAC_PROGRAM_KEYS */

    programMenu->addAction(QIcon(":/images/Abort.png"), tr("Stop Build or Loader"), this, SLOT(programStopBuild()));
    programMenu->addSeparator();
#ifdef ENABLE_FILETO_SDCARD
    programMenu->addAction(QIcon(":/images/SaveToSD.png"), tr(FileToSDCard), this, SLOT(downloadSdCard()));
#endif
    programMenu->addAction(QIcon(":/images/console.png"), tr("Open Terminal"), this, SLOT(menuActionConnectButton()));
    programMenu->addAction(QIcon(":/images/reset.png"), tr("Reset Port"), this, SLOT(portResetButton()));
    programMenu->addAction(tr(BuildAllLibraries), this, SLOT(programBuildAllLibraries()), Qt::CTRL+Qt::ALT+Qt::Key_F12);

#if defined(GDBENABLE)
    QMenu *debugMenu = new QMenu(tr("&Debug"), this);
    menuBar()->addMenu(debugMenu);

    debugMenu->addAction(tr("&Debug Start"), this, SLOT(debugCompileLoad()), Qt::Key_F5);
    debugMenu->addAction(tr("&Continue"), this, SLOT(gdbContinue()), Qt::ALT+Qt::Key_R);
    debugMenu->addAction(tr("&Next Line"), this, SLOT(gdbNext()), Qt::ALT+Qt::Key_N);
    debugMenu->addAction(tr("&Step In"), this, SLOT(gdbStep()), Qt::ALT+Qt::Key_S);
    debugMenu->addAction(tr("&Finish Function"), this, SLOT(gdbFinish()), Qt::ALT+Qt::Key_F);
    debugMenu->addAction(tr("&Backtrace"), this, SLOT(gdbBacktrace()), Qt::ALT+Qt::Key_B);
    debugMenu->addAction(tr("&Until"), this, SLOT(gdbUntil()), Qt::ALT+Qt::Key_U);
    debugMenu->addAction(tr("&Interrupt"), this, SLOT(gdbInterrupt()), Qt::ALT+Qt::Key_I);
    debugMenu->addAction(tr("&Kill"), this, SLOT(gdbKill()), Qt::ALT+Qt::Key_K);
    connect(gdb,SIGNAL(breakEvent()),this,SLOT(gdbBreak()));
#endif

    /* add editor popup context menu */
    edpopup = new QMenu(tr("Editor Popup"), this);
    edpopup->addAction(QIcon(":/images/undo.png"),tr("Undo"),this,SLOT(undoChange()));
    edpopup->addAction(QIcon(":/images/redo.png"),tr("Redo"),this,SLOT(redoChange()));
    edpopup->addSeparator();
    edpopup->addAction(QIcon(":/images/copy.png"),tr("Copy"),this,SLOT(copyFromFile()));
    edpopup->addAction(QIcon(":/images/cut.png"),tr("Cut"),this,SLOT(cutFromFile()));
    edpopup->addAction(QIcon(":/images/paste.png"),tr("Paste"),this,SLOT(pasteToFile()));
}

void MainSpinWindow::enableProjectView(bool enable)
{
    QList<QAction*> list = toolsMenu->actions();
    QAction *action = list.at(0);
    action->setEnabled(enable);
    action->setVisible(enable);
}

void MainSpinWindow::setupToolBars()
{
    fileToolBar = addToolBar(tr("File"));
    QToolButton *btnFileNew = new QToolButton(this);
    QToolButton *btnFileOpen = new QToolButton(this);
    QToolButton *btnFileSave = new QToolButton(this);
    QToolButton *btnFileSaveAs = new QToolButton(this);
    QToolButton *btnFilePrint = new QToolButton(this);

    addToolButton(fileToolBar, btnFileNew, QString(":/images/newfile.png"));
    addToolButton(fileToolBar, btnFileOpen, QString(":/images/openfile.png"));
    addToolButton(fileToolBar, btnFileSave, QString(":/images/savefile.png"));
    addToolButton(fileToolBar, btnFileSaveAs, QString(":/images/saveasfile2.png"));
    addToolButton(fileToolBar, btnFilePrint, QString(":/images/print.png"));

    connect(btnFileNew,SIGNAL(clicked()),this,SLOT(newFile()));
    connect(btnFileOpen,SIGNAL(clicked()),this,SLOT(openFile()));
    connect(btnFileSave,SIGNAL(clicked()),this,SLOT(saveFile()));
    connect(btnFileSaveAs,SIGNAL(clicked()),this,SLOT(saveAsFile()));
    connect(btnFilePrint,SIGNAL(clicked()),this,SLOT(printFile()));

    btnFileNew->setToolTip(tr("New File"));
    btnFileOpen->setToolTip(tr("Open File"));
    btnFileSave->setToolTip(tr("Save File"));
    btnFileSaveAs->setToolTip(tr("Save As File"));
    btnFilePrint->setToolTip(tr("Print File"));
    //btnFileZip->setToolTip(tr("Zip"));

    /*
     * Project Toobar
     */
    projToolBar = addToolBar(tr("Project"));

    QToolButton *btnProjectNew = new QToolButton(this);
    QToolButton *btnProjectOpen = new QToolButton(this);
    //QToolButton *btnProjectClone = new QToolButton(this);
    QToolButton *btnProjectSave = new QToolButton(this);
    QToolButton *btnProjectSaveAs = new QToolButton(this);
    btnProjectClose = new QAction(this);
    QToolButton *btnProjectZip = new QToolButton(this);

    addToolButton(projToolBar, btnProjectNew, QString(":/images/newproj.png"));
    addToolButton(projToolBar, btnProjectOpen, QString(":/images/openproj.png"));
    addToolButton(projToolBar, btnProjectSave, QString(":/images/saveproj.png"));
    addToolButton(projToolBar, btnProjectSaveAs, QString(":/images/saveasproj.png"));
    //addToolButton(projToolBar, btnProjectClone, QString(":/images/cloneproj2.png"));
    addToolBarAction(projToolBar, btnProjectClose, QString(":/images/closeproj.png"));
    addToolButton(projToolBar, btnProjectZip, QString(":/images/zip2.png"));

    /*
     * Add the AddLib button to project
     */
#ifdef USE_ADDLIB_BUTTON
    btnProjectAddLib = new QAction(this);
    addToolBarAction(projToolBar, btnProjectAddLib, QString(":/images/addlib.png"));
    btnProjectAddLib->setToolTip(tr("Add Simple Library"));
    connect(btnProjectAddLib,SIGNAL(triggered()),this,SLOT(addLib()));
#endif

    /*
     * Add Tools Toobar ... add tab, add lib
     */
    addToolsToolBar = addToolBar(tr("Add Tools"));
    // put add tools on a separate tool bar
    btnProjectAddTab = new QAction(this);
    btnProjectOpenTab = new QAction(this);
    addToolBarAction(addToolsToolBar, btnProjectAddTab, QString(":/images/addtab.png"));
    btnProjectAddTab->setToolTip(tr(AddTab));
    addToolBarAction(addToolsToolBar, btnProjectOpenTab, QString(":/images/opentab.png"));
    btnProjectOpenTab->setToolTip(tr(OpenTab));
    connect(btnProjectAddTab,SIGNAL(triggered()),this,SLOT(addTab()));
    connect(btnProjectOpenTab,SIGNAL(triggered()),this,SLOT(openTab()));

    /*
     * Add Properties after add tools
     */
    propToolBar = addToolBar(tr("Miscellaneous"));
    QToolButton *btnProjectApp = new QToolButton(this);

    addToolButton(propToolBar, btnProjectApp, QString(":/images/project.png"));

    connect(btnProjectNew,SIGNAL(clicked()),this,SLOT(newProject()));
    connect(btnProjectOpen,SIGNAL(clicked()),this,SLOT(openProject()));
    connect(btnProjectSave,SIGNAL(clicked()),this,SLOT(saveProject()));
    connect(btnProjectSaveAs,SIGNAL(clicked()),this,SLOT(saveAsProject()));
    //connect(btnProjectClone,SIGNAL(clicked()),this,SLOT(cloneProject()));
    connect(btnProjectClose,SIGNAL(triggered()),this,SLOT(closeProject()));
    connect(btnProjectApp,SIGNAL(clicked()),this,SLOT(setProject()));
    connect(btnProjectZip,SIGNAL(clicked()),this,SLOT(zipProject()));

    btnProjectNew->setToolTip(tr("New Project"));
    btnProjectOpen->setToolTip(tr("Open Project"));
    //btnProjectClone->setToolTip(tr("Clone Project"));
    btnProjectSave->setToolTip(tr("Save Project"));
    btnProjectSaveAs->setToolTip(tr("Save Project As"));
    btnProjectClose->setToolTip(tr(CloseProject));
    btnProjectApp->setToolTip(tr("Set Project to Current Tab"));
    btnProjectZip->setToolTip(tr("Zip Project"));

    //propToolBar = addToolBar(tr("Properties"));
/*
    QToolButton *btnProjectBoard = new QToolButton(this);
    addToolButton(propToolBar, btnProjectBoard, QString(":/images/hardware.png"));
    connect(btnProjectBoard,SIGNAL(clicked()),this,SLOT(hardware()));
    btnProjectBoard->setToolTip(tr("Configuration"));
*/
    QToolButton *btnProjectProperties = new QToolButton(this);
    addToolButton(propToolBar, btnProjectProperties, QString(":/images/properties.png"));
    connect(btnProjectProperties,SIGNAL(clicked()),this,SLOT(properties()));
    btnProjectProperties->setToolTip(tr("Properties"));

    if(ctags->enabled()) {
        browseToolBar = addToolBar(tr("Browse Code"));
        btnBrowseBack = new QToolButton(this);
        addToolButton(browseToolBar, btnBrowseBack, QString(":/images/back.png"));
        connect(btnBrowseBack,SIGNAL(clicked()),this,SLOT(prevDeclaration()));
        btnBrowseBack->setToolTip(tr("Back"));
        btnBrowseBack->setEnabled(false);

        btnFindDef = new QToolButton(this);
        addToolButton(browseToolBar, btnFindDef, QString(":/images/forward.png"));
        connect(btnFindDef,SIGNAL(clicked()),this,SLOT(findDeclaration()));
        btnFindDef->setToolTip(tr("Browse (Ctrl+Left Click)"));
    }

#if defined(SD_TOOLS) && defined(ENABLE_FILETO_SDCARD)
    sdCardToolBar = addToolBar(tr("SD Card"));

    btnDownloadSdCard = new QToolButton(this);
    addToolButton(sdCardToolBar, btnDownloadSdCard, QString(":/images/SaveToSD.png"));
    connect(btnDownloadSdCard, SIGNAL(clicked()),this,SLOT(downloadSdCard()));
    btnDownloadSdCard->setToolTip(tr(FileToSDCard));

#endif

    programToolBar = addToolBar(tr("Program"));
    btnProgramDebugTerm = new QToolButton(this);
    btnProgramRun = new QToolButton(this);
    QToolButton *btnProgramStopBuild = new QToolButton(this);
    QToolButton *btnProgramBuild = new QToolButton(this);
    QToolButton *btnProgramBurnEEP = new QToolButton(this);

    addToolButton(programToolBar, btnProgramStopBuild, QString(":/images/Abort.png"));
    addToolButton(programToolBar, btnProgramBuild, QString(":/images/build3.png"));
    addToolButton(programToolBar, btnProgramBurnEEP, QString(":/images/burnee.png"));
    addToolButton(programToolBar, btnProgramRun, QString(":/images/run.png"));
    addToolButton(programToolBar, btnProgramDebugTerm, QString(":/images/RunConsole2.png"));

    connect(btnProgramStopBuild,SIGNAL(clicked()),this,SLOT(programStopBuild()));
    connect(btnProgramBuild,SIGNAL(clicked()),this,SLOT(programBuild()));
    connect(btnProgramBurnEEP,SIGNAL(clicked()),this,SLOT(programBurnEE()));
    connect(btnProgramDebugTerm,SIGNAL(clicked()),this,SLOT(programDebug()));
    connect(btnProgramRun,SIGNAL(clicked()),this,SLOT(programRun()));

    btnProgramStopBuild->setToolTip(tr("Stop Build or Loader"));
    btnProgramBuild->setToolTip(tr("Build Project"));
    btnProgramBurnEEP->setToolTip(tr("Load EEPROM & Run"));
    btnProgramRun->setToolTip(tr("Load RAM & Run"));
    btnProgramDebugTerm->setToolTip(tr("Run with Terminal"));

#ifdef SIMPLE_BOARD_TOOLBAR
    /*
     * Board Type control toolbar
     */
    boardToolBar = addToolBar(tr("Board Type"));
    boardToolBar->setLayoutDirection(Qt::RightToLeft);
    cbBoard = new QComboBox(this);
    cbBoard->setLayoutDirection(Qt::LeftToRight);
    cbBoard->setToolTip(tr("Board Type Select"));
    cbBoard->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    connect(cbBoard,SIGNAL(currentIndexChanged(int)),this,SLOT(setCurrentBoard(int)));
    btnLoadBoards = new QAction(this);
    btnLoadBoards->setToolTip(tr("Reload Board List"));
    connect(btnLoadBoards,SIGNAL(clicked()),this,SLOT(reloadBoardTypes()));
    addToolBarAction(boardToolBar, btnLoadBoards, QString(":/images/hardware.png"));
    boardToolBar->addWidget(cbBoard);
    boardToolBar->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
#endif

    /*
     * Serial Port control toolbar
     */
    ctrlToolBar = addToolBar(tr("Serial Port"));
    ctrlToolBar->setLayoutDirection(Qt::RightToLeft);
    cbPort = new QPortComboBox(this);
    cbPort->setEditable(false);
    cbPort->setLayoutDirection(Qt::LeftToRight);
    cbPort->setToolTip(tr("Port Select"));
    cbPort->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    connect(cbPort,SIGNAL(currentIndexChanged(int)),this,SLOT(setCurrentPort(int)));
    connect(cbPort,SIGNAL(clicked()), this, SLOT(enumeratePortsEvent()));

    btnConnected = new QAction(this);
    btnConnected->setToolTip(tr("SimpleIDE Terminal")+tr(" resets port if AUTO."));
    btnConnected->setCheckable(true);
    connect(btnConnected,SIGNAL(triggered()),this,SLOT(connectButton()));

    btnBoardReset = new QAction(this);
    btnBoardReset->setToolTip(tr("Reset Port"));
    connect(btnBoardReset,SIGNAL(triggered()),this,SLOT(portResetButton()));
    /* can't hide a QToolButton. Use QAction instead */
    addToolBarAction(ctrlToolBar, btnBoardReset, QString(":/images/reset.png"));
    //connect(btnBoardReset, SIGNAL(triggered()), this, SLOT(portResetButton()));

    //addToolButton(ctrlToolBar, btnBoardReset, QString(":/images/reset.png"));
    addToolBarAction(ctrlToolBar, btnConnected, QString(":/images/console.png"));
#ifdef BUTTON_PORT_SCAN
    btnPortScan = new QAction(this);
    btnPortScan->setToolTip(tr("Rescan Ports"));
    connect(btnPortScan,SIGNAL(triggered()),this,SLOT(enumeratePorts()));
    addToolBarAction(ctrlToolBar, btnPortScan, QString(":/images/refresh.png"));
#endif
    ctrlToolBar->addWidget(cbPort);
    ctrlToolBar->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);

}

void MainSpinWindow::toggleSimpleView()
{
     simpleViewType = !simpleViewType;
     QList<QAction*> list = toolsMenu->actions();
     QAction *action = list.at(0);
     action->setText(this->simpleViewType ? tr(ProjectView) : tr(SimpleView));
     action->setIcon(this->simpleViewType ? projectViewIcon : simpleViewIcon);
     showSimpleView(simpleViewType);
     currentTabChanged();
}

/*
 * show simple/project view
 */
void MainSpinWindow::showSimpleView(bool simple)
{
    /* in simple view, we don't show:
     * selected items in fileMenu and projMenu
     * project manager
     * status tabs
     * file toolbar
     * btn Project Set App
     */
    QList <QAction*> fileMenuList = fileMenu->actions();
    QList <QAction*> projMenuList = projMenu->actions();
    QList <QAction*> progMenuList = programMenu->actions();

    /* simple view */
    if(simple)
    {
        int projwidth = PROJECT_WIDTH-btnShowStatusPane->width();

        if(btnShowProjectPane->isChecked()) {
            leftSplit->show();
        }
        else {
            leftSplit->hide();
        }

        if(btnShowStatusPane->isChecked()) {
            statusTabs->show();
        }
        else {
            statusTabs->hide();
        }

        showProjectPane(btnShowProjectPane->isChecked());
        showStatusPane(btnShowStatusPane->isChecked());

        fileToolBar->hide();
        propToolBar->hide();
        btnProjectClose->setVisible(false);
        addToolsToolBar->hide();
#ifdef ENABLE_FILETO_SDCARD
        sdCardToolBar->hide();
#endif
        btnBoardReset->setVisible(false);
#ifdef SIMPLE_BOARD_TOOLBAR
        btnLoadBoards->setVisible(false);
#endif
#ifdef BUTTON_PORT_SCAN
        btnPortScan->setVisible(false);
#endif
        btnConnected->setVisible(false);
        btnShowProjectPane->show();
        btnShowStatusPane->show();
        programSize->setMinimumWidth(projwidth);
        programSize->setMaximumWidth(projwidth);
        programSize->hide();
        programSize->show();

        foreach(QAction *fa, fileMenuList) {
            QString txt = fa->text();
            if(txt != NULL) {
                if(txt.endsWith(CloseFile) ||
                   txt.endsWith(NewFile) ||
                   txt.endsWith(OpenFile) ||
                   txt.endsWith(SaveFile) ||
                   txt.endsWith(SaveAsFile))
                   fa->setVisible(false);
            }
        }
        foreach(QAction *pa, projMenuList) {
            QString txt = pa->text();
            if(txt != NULL) {
                if(txt.contains(SetProject) ||
                   txt.contains(AddFileCopy) ||
                   txt.contains(AddFileLink) ||
                   txt.contains(AddIncludePath) ||
                   txt.contains(AddLibraryPath))
                   pa->setVisible(false);
            }
        }
        foreach(QAction *pa, progMenuList) {
            QString txt = pa->text();
            if(txt != NULL) {
                if(txt.contains(BuildAllLibraries))
                   pa->setVisible(false);
            }
        }
        if(ctags->enabled()) {
            browseToolBar->setVisible(false);
        }

        //projectOptions->enableDependentBuild(false);
        //projectOptions->setDependentBuild(false);

        QApplication::processEvents();
    }
    /* project view */
    else
    {
        int projwidth = PROJECT_WIDTH+6;

        leftSplit->show();
        statusTabs->show();
        fileToolBar->show();
        propToolBar->show();
        btnProjectClose->setVisible(true);
        addToolsToolBar->show();
#ifdef ENABLE_FILETO_SDCARD
        sdCardToolBar->show();
#endif
        btnBoardReset->setVisible(true);
#ifdef SIMPLE_BOARD_TOOLBAR
        btnLoadBoards->setVisible(true);
#endif
#ifdef BUTTON_PORT_SCAN
        btnPortScan->setVisible(true);
#endif
        btnConnected->setVisible(true);
        btnShowProjectPane->hide();
        btnShowStatusPane->hide();
        showProjectPane(true);
        showStatusPane(true);
        programSize->setMinimumWidth(projwidth);
        programSize->setMaximumWidth(projwidth);

        foreach(QAction *fa, fileMenuList) {
            QString txt = fa->text();
            if(txt != NULL) {
                if(txt.endsWith(CloseFile) ||
                   txt.endsWith(NewFile) ||
                   txt.endsWith(OpenFile) ||
                   txt.endsWith(SaveFile) ||
                   txt.endsWith(SaveAsFile))
                   fa->setVisible(true);
            }
        }
        foreach(QAction *pa, projMenuList) {
            QString txt = pa->text();
            if(txt != NULL) {
                if(txt.contains(SetProject) ||
                   txt.contains(AddFileCopy) ||
                   txt.contains(AddFileLink) ||
                   txt.contains(AddIncludePath) ||
                   txt.contains(AddLibraryPath))
                   pa->setVisible(true);
            }
        }
        foreach(QAction *pa, progMenuList) {
            QString txt = pa->text();
            if(txt != NULL) {
                if(txt.contains(BuildAllLibraries))
                   pa->setVisible(true);
            }
        }
        if(ctags->enabled()) {
            browseToolBar->setVisible(true);
        }

        //projectOptions->enableDependentBuild(true);

        QApplication::processEvents();
        HintDialog::hint("FirstSimpleProjectView", tr("Welcome to Project View. Users of this view are often experienced. Parallax does not recommend Project View for beginners."), this);
    }
    QVariant viewv = simple;
    settings->setValue(simpleViewKey, viewv);
    QApplication::processEvents();

}

void MainSpinWindow::showProjectPane(bool show)
{
    leftSplit->setVisible(show);
    if(show) {
        btnShowProjectPane->setIcon(QIcon(QIcon(":/images/ProjectHide.png")));
        if(leftSplit->width() < 20 && this->simpleViewType) {
            resetVerticalSplitSize();
        }
        if(this->simpleViewType) {
            // force splitter to always show project options
            QList<int> lsizes = leftSplit->sizes();
            lsizes[0] = leftSplit->height()*50/100;
            lsizes[1] = leftSplit->height()*50/100;
            leftSplit->setSizes(lsizes);
            leftSplit->adjustSize();
        }
    }
    else {
        btnShowProjectPane->setIcon(QIcon(QIcon(":/images/ProjectShow.png")));
    }
}

void MainSpinWindow::showStatusPane(bool show)
{
    statusTabs->setVisible(show);
    if(show) {
        btnShowStatusPane->setIcon(QIcon(QIcon(":/images/BuildHide.png")));
        if(statusTabs->height() < 20 && this->simpleViewType) {
            resetRightSplitSize();
        }
    }
    else {
        btnShowStatusPane->setIcon(QIcon(QIcon(":/images/BuildShow.png")));
    }
}

void MainSpinWindow::splitterChanged()
{
    showProjectPane(leftSplit->isVisible());
    showStatusPane(statusTabs->isVisible());
}

void MainSpinWindow::ideDebugShow()
{
    if(ideDebugTabIndex != 0) {
        statusTabs->removeTab(ideDebugTabIndex);
        ideDebugTabIndex = 0;
    }
    else {
        statusTabs->addTab(debugStatus,tr("IDE Debug Info"));
        ideDebugTabIndex = statusTabs->count()-1;
        statusTabs->setCurrentIndex(ideDebugTabIndex);
    }
}
