/**
 * TaskAdmin - small S60 3rd Edition task manager for the Nokia E51.
 *
 * The application menu lists the running window groups. Its Options menu can
 * close a task politely (TApaTask::EndTask), kill its window-group task, or
 * kill every process with the same executable name as that task. The last
 * command is what makes Java MIDlets manageable from a real menu: the Java
 * runtime owns one process shared by every MIDlet, so killing it closes all of
 * them. It is deliberately a separate command with a warning in the README.
 *
 * Killing a process that was not created by this application needs the
 * PowerMgmt capability. On a phone without the developer/installserver patches
 * Symbian will refuse to install or run the package; that is expected.
 *
 * Every action is written to C:\Data\TaskAdmin.log so a handset can report what
 * happened without a console.
 */

#include <e32base.h>
#include <e32std.h>
#include <e32capability.h>
#include <f32file.h>
#include <badesca.h>
#include <aknlists.h>
#include <aknglobalnote.h>
#include <avkon.hrh>
#include <aknapp.h>
#include <akndoc.h>
#include <aknappui.h>
#include <apgtask.h>
#include <apgwgnam.h>
#include <w32std.h>
#include <eikstart.h>
#include <coecntrl.h>

#include "TaskAdmin.hrh"
#include <TaskAdmin.rsg>

const TInt KMaxCaptionLength = 128;
const TInt KMaxProcessLength = 64;

static void LogLine(const TDesC& aText);
static TBool IsProtectedProcess(const TDesC& aName);

/** One line of the task list, with enough identity to act on it later. */
struct TTaskInfo
{
    TInt iWgId;
    TThreadId iThreadId;
    TUint iPid;
    TBuf<KMaxCaptionLength> iCaption;
    TBuf<KMaxProcessLength> iProcess;
};

/**
 * Main view container containing a standard CAknSingleStyleListBox.
 */
class CTaskAdminContainer : public CCoeControl, public MEikListBoxObserver
{
public:
    static CTaskAdminContainer* NewL(const TRect& aRect, CDesCArrayFlat* aCaptions);
    ~CTaskAdminContainer();

    void ConstructL(const TRect& aRect, CDesCArrayFlat* aCaptions);
    TInt CurrentIndex() const;
    void UpdateListL();

    // From CCoeControl
    TInt CountComponentControls() const;
    CCoeControl* ComponentControl(TInt aIndex) const;
    void SizeChanged();
    TKeyResponse OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType);

    // From MEikListBoxObserver
    void HandleListBoxEventL(CEikListBox* aListBox, TListBoxEvent aEventType);

private:
    CAknSingleStyleListBox* iListBox;
    CDesCArrayFlat* iCaptions;
};

class CTaskAdminAppUi : public CAknAppUi
{
public:
    CTaskAdminAppUi();
    void ConstructL();
    ~CTaskAdminAppUi();

    void HandleCommandL(TInt aCommand);
    void RefreshTasksL();

private:
    void RunActionL(TInt aCommand, TInt aIndex);
    void CloseTaskL(const TTaskInfo& aTask);
    void KillTaskL(const TTaskInfo& aTask);
    void KillProcessesL(const TTaskInfo& aTask);
    void ShowNoteL(const TDesC& aText);

    RArray<TTaskInfo> iTasks;
    CDesCArrayFlat* iCaptions;
    CTaskAdminContainer* iContainer;
};

/** Application/document boilerplate: a real GUI app gets a task-list entry. */
class CTaskAdminApplication : public CAknApplication
{
public:
    static CApaApplication* NewApplication();

private:
    TUid AppDllUid() const;
    CApaDocument* CreateDocumentL();
};

class CTaskAdminDocument : public CAknDocument
{
public:
    static CTaskAdminDocument* NewL(CEikApplication& aApp);
    static CTaskAdminDocument* NewLC(CEikApplication& aApp);
    ~CTaskAdminDocument();

    CEikAppUi* CreateAppUiL();

private:
    CTaskAdminDocument(CEikApplication& aApp);
    void ConstructL();
};

// ---------------------------------------------------------------------------
// Container implementation
// ---------------------------------------------------------------------------

CTaskAdminContainer* CTaskAdminContainer::NewL(const TRect& aRect, CDesCArrayFlat* aCaptions)
{
    CTaskAdminContainer* self = new (ELeave) CTaskAdminContainer;
    CleanupStack::PushL(self);
    self->ConstructL(aRect, aCaptions);
    CleanupStack::Pop(self);
    return self;
}

CTaskAdminContainer::~CTaskAdminContainer()
{
    delete iListBox;
}

void CTaskAdminContainer::ConstructL(const TRect& aRect, CDesCArrayFlat* aCaptions)
{
    CreateWindowL();
    iCaptions = aCaptions;

    iListBox = new (ELeave) CAknSingleStyleListBox;
    iListBox->SetContainerWindowL(*this);
    iListBox->ConstructL(this, NULL);

    iListBox->CreateScrollBarFrameL(ETrue);
    iListBox->ScrollBarFrame()->SetScrollBarVisibilityL(
        CEikScrollBarFrame::EOff, CEikScrollBarFrame::EAuto);

    iListBox->Model()->SetItemTextArray(iCaptions);
    iListBox->Model()->SetOwnershipType(ELbmDoesNotOwnItemArray);

    iListBox->SetListBoxObserver(this);

    SetRect(aRect);
    ActivateL();
}

TInt CTaskAdminContainer::CountComponentControls() const
{
    return iListBox ? 1 : 0;
}

CCoeControl* CTaskAdminContainer::ComponentControl(TInt aIndex) const
{
    if (aIndex == 0) return iListBox;
    return NULL;
}

void CTaskAdminContainer::SizeChanged()
{
    if (iListBox)
    {
        iListBox->SetRect(Rect());
    }
}

TKeyResponse CTaskAdminContainer::OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType)
{
    if (iListBox)
    {
        return iListBox->OfferKeyEventL(aKeyEvent, aType);
    }
    return EKeyWasNotConsumed;
}

void CTaskAdminContainer::HandleListBoxEventL(CEikListBox* /*aListBox*/, TListBoxEvent /*aEventType*/)
{
}

TInt CTaskAdminContainer::CurrentIndex() const
{
    if (iListBox)
    {
        return iListBox->CurrentItemIndex();
    }
    return -1;
}

void CTaskAdminContainer::UpdateListL()
{
    if (iListBox)
    {
        iListBox->HandleItemAdditionL();
        if (iCaptions->Count() > 0 && iListBox->CurrentItemIndex() < 0)
        {
            iListBox->SetCurrentItemIndex(0);
        }
        DrawDeferred();
    }
}

// ---------------------------------------------------------------------------
// Application and document
// ---------------------------------------------------------------------------

CApaApplication* CTaskAdminApplication::NewApplication()
{
    return new CTaskAdminApplication;
}

TUid CTaskAdminApplication::AppDllUid() const
{
    return TUid::Uid(0xA0000D16);
}

CApaDocument* CTaskAdminApplication::CreateDocumentL()
{
    return CTaskAdminDocument::NewL(*this);
}

CTaskAdminDocument* CTaskAdminDocument::NewL(CEikApplication& aApp)
{
    CTaskAdminDocument* self = NewLC(aApp);
    CleanupStack::Pop(self);
    return self;
}

CTaskAdminDocument* CTaskAdminDocument::NewLC(CEikApplication& aApp)
{
    CTaskAdminDocument* self = new (ELeave) CTaskAdminDocument(aApp);
    CleanupStack::PushL(self);
    self->ConstructL();
    return self;
}

CTaskAdminDocument::CTaskAdminDocument(CEikApplication& aApp)
    : CAknDocument(aApp)
{
}

CTaskAdminDocument::~CTaskAdminDocument()
{
}

void CTaskAdminDocument::ConstructL()
{
}

CEikAppUi* CTaskAdminDocument::CreateAppUiL()
{
    return new (ELeave) CTaskAdminAppUi;
}

// ---------------------------------------------------------------------------
// AppUi
// ---------------------------------------------------------------------------

CTaskAdminAppUi::CTaskAdminAppUi()
    : iContainer(NULL)
{
}

void CTaskAdminAppUi::ConstructL()
{
    LogLine(_L("construct"));
    BaseConstructL(CAknAppUi::EAknEnableSkin);
    LogLine(_L("base done"));

    iCaptions = new (ELeave) CDesCArrayFlat(16);

    LogLine(RProcess().HasCapability(ECapabilityPowerMgmt) ?
        _L("powermgmt=yes") : _L("powermgmt=no"));

    TRAPD(refreshErr, RefreshTasksL());
    if (refreshErr != KErrNone)
    {
        TBuf<32> errLine(_L("refresh err="));
        errLine.AppendNum(refreshErr);
        LogLine(errLine);
    }
    else
    {
        LogLine(_L("refresh ok"));
    }

    iContainer = CTaskAdminContainer::NewL(ClientRect(), iCaptions);
    AddToStackL(iContainer);
    LogLine(_L("container ready"));
}

CTaskAdminAppUi::~CTaskAdminAppUi()
{
    if (iContainer)
    {
        RemoveFromStack(iContainer);
        delete iContainer;
    }
    iTasks.Close();
    delete iCaptions;
}

void CTaskAdminAppUi::HandleCommandL(TInt aCommand)
{
    switch (aCommand)
    {
    case ETaskAdminCmdRefresh:
        RefreshTasksL();
        if (iContainer) iContainer->UpdateListL();
        break;

    case ETaskAdminCmdCloseTask:
    case ETaskAdminCmdKillTask:
    case ETaskAdminCmdKillProcesses:
        if (iContainer && iTasks.Count() > 0)
        {
            TInt index = iContainer->CurrentIndex();
            if (index >= 0 && index < iTasks.Count())
            {
                TRAPD(actionErr, RunActionL(aCommand, index));
                if (actionErr != KErrNone)
                {
                    TBuf<64> msg(_L("Error "));
                    msg.AppendNum(actionErr);
                    ShowNoteL(msg);
                }
                RefreshTasksL();
                iContainer->UpdateListL();
            }
            else
            {
                ShowNoteL(_L("Selecciona una tarea"));
            }
        }
        else
        {
            ShowNoteL(_L("No hay tareas"));
        }
        break;

    case EAknCmdExit:
    case EEikCmdExit:
        Exit();
        break;

    default:
        CAknAppUi::HandleCommandL(aCommand);
        break;
    }
}

void CTaskAdminAppUi::RefreshTasksL()
{
    iTasks.Reset();
    iCaptions->Reset();

    RWsSession& ws = iEikonEnv->WsSession();

    TInt count = ws.NumWindowGroups();
    if (count < 1) count = 1;
    CArrayFixFlat<TInt>* groups = new (ELeave) CArrayFixFlat<TInt>(count);
    CleanupStack::PushL(groups);
    User::LeaveIfError(ws.WindowGroupList(groups));

    TInt self = iEikonEnv->RootWin().Identifier();

    for (TInt i = 0; i < groups->Count(); i++)
    {
        TInt wg = groups->At(i);
        if (wg == 0 || wg == self) continue;

        CApaWindowGroupName* wgName = NULL;
        TRAPD(wgErr, wgName = CApaWindowGroupName::NewLC(ws, wg));
        if (wgErr != KErrNone || wgName == NULL) continue;

        if (wgName->Hidden())
        {
            CleanupStack::PopAndDestroy(wgName);
            continue;
        }

        TPtrC caption = wgName->Caption();
        TBuf<KMaxCaptionLength> safeCaption;
        if (caption.Length() > 0)
        {
            safeCaption.Copy(caption.Left(KMaxCaptionLength));
        }
        else
        {
            TUid appUid = wgName->AppUid();
            if (appUid.iUid != 0)
            {
                safeCaption.Format(_L("App [0x%08X]"), (TUint)appUid.iUid);
            }
            else
            {
                safeCaption.Format(_L("Task %d"), wg);
            }
        }

        for (TInt c = 0; c < safeCaption.Length(); c++)
        {
            if (safeCaption[c] == '\t' || safeCaption[c] == '\n' || safeCaption[c] == '\r')
            {
                safeCaption[c] = ' ';
            }
        }

        TThreadId threadId = 0;
        TUint pid = 0;
        TBuf<KMaxProcessLength> processName;

        ws.GetWindowGroupClientThreadId(wg, threadId);
        RThread thread;
        if (thread.Open(threadId) == KErrNone)
        {
            RProcess process;
            if (thread.Process(process) == KErrNone)
            {
                processName.Copy(process.Name());
                pid = (TUint) process.Id();
                process.Close();
            }
            thread.Close();
        }

        TTaskInfo info;
        info.iWgId = wg;
        info.iThreadId = threadId;
        info.iPid = pid;
        info.iCaption.Copy(safeCaption);
        info.iProcess.Copy(processName);
        User::LeaveIfError(iTasks.Append(info));

        TBuf<KMaxCaptionLength + 8> itemText;
        itemText.Format(_L("\t%S\t\t"), &safeCaption);
        iCaptions->AppendL(itemText);

        CleanupStack::PopAndDestroy(wgName);
    }

    CleanupStack::PopAndDestroy(groups);

    if (iCaptions->Count() == 0)
    {
        iCaptions->AppendL(_L("\t(No running tasks)\t\t"));
    }

    TBuf<64> line(_L("tasks="));
    line.AppendNum(iTasks.Count());
    LogLine(line);
}

void CTaskAdminAppUi::RunActionL(TInt aCommand, TInt aIndex)
{
    if (aIndex < 0 || aIndex >= iTasks.Count())
    {
        ShowNoteL(_L("No task selected"));
        return;
    }

    const TTaskInfo& task = iTasks[aIndex];

    TBuf<160> log(_L("action="));
    log.AppendNum(aCommand);
    log.Append(_L(" wg="));
    log.AppendNum(task.iWgId);
    log.Append(_L(" pid="));
    log.AppendNum(task.iPid);
    log.Append(_L(" name="));
    log.Append(task.iCaption);
    LogLine(log);

    switch (aCommand)
    {
    case ETaskAdminCmdCloseTask:
        CloseTaskL(task);
        break;
    case ETaskAdminCmdKillTask:
        KillTaskL(task);
        break;
    case ETaskAdminCmdKillProcesses:
        KillProcessesL(task);
        break;
    default:
        break;
    }
}

/** Polite close: the app gets its normal shutdown handling. */
void CTaskAdminAppUi::CloseTaskL(const TTaskInfo& aTask)
{
    TApaTask task(iEikonEnv->WsSession());
    task.SetWgId(aTask.iWgId);
    if (!task.Exists())
    {
        ShowNoteL(_L("Task is already gone"));
        return;
    }
    task.EndTask();
    ShowNoteL(_L("Task closed"));
    LogLine(_L("closed"));
}

/** Hard close of the window group, which also kills its owning process. */
void CTaskAdminAppUi::KillTaskL(const TTaskInfo& aTask)
{
    if (!RProcess().HasCapability(ECapabilityPowerMgmt))
    {
        ShowNoteL(_L("PowerMgmt no disponible: usa Cerrar tarea"));
        LogLine(_L("kill denied"));
        return;
    }

    TApaTask task(iEikonEnv->WsSession());
    task.SetWgId(aTask.iWgId);
    if (!task.Exists())
    {
        ShowNoteL(_L("Task is already gone"));
        return;
    }
    task.KillTask();
    ShowNoteL(_L("Task killed"));
    LogLine(_L("killed task"));
}

/** Never let the menu take down a process the phone needs to boot. */
static TBool IsProtectedProcess(const TDesC& aName)
{
    if (aName.CompareF(_L("SysAp.exe")) == 0) return ETrue;
    if (aName.CompareF(_L("EikSrv.exe")) == 0) return ETrue;
    if (aName.CompareF(_L("WsSrvr.exe")) == 0) return ETrue;
    if (aName.CompareF(_L("AppArf.exe")) == 0) return ETrue;
    if (aName.CompareF(_L("EApps.exe")) == 0) return ETrue;
    if (aName.CompareF(_L("StartContent.exe")) == 0) return ETrue;
    if (aName.CompareF(_L("EKern.exe")) == 0) return ETrue;
    if (aName.CompareF(_L("Efsrv.exe")) == 0) return ETrue;
    if (aName.CompareF(_L("Esock.exe")) == 0) return ETrue;
    if (aName.CompareF(_L("C32.exe")) == 0) return ETrue;
    if (aName.CompareF(_L("EcomServer.exe")) == 0) return ETrue;
    if (aName.CompareF(_L("SecurityServer.exe")) == 0) return ETrue;
    if (aName.CompareF(_L("LicenseServer.exe")) == 0) return ETrue;
    if (aName.CompareF(_L("DrmServer.exe")) == 0) return ETrue;
    return EFalse;
}

/**
 * Kill every process with this task's executable name. All Java MIDlets share
 * one runtime process, so this deliberately closes them all; the README says
 * so, and "Cerrar tarea" is offered first for that reason.
 */
void CTaskAdminAppUi::KillProcessesL(const TTaskInfo& aTask)
{
    if (!RProcess().HasCapability(ECapabilityPowerMgmt))
    {
        ShowNoteL(_L("PowerMgmt no disponible: usa Cerrar tarea"));
        LogLine(_L("kill denied"));
        return;
    }

    if (aTask.iProcess.Length() == 0)
    {
        ShowNoteL(_L("This task has no process name"));
        return;
    }
    if (IsProtectedProcess(aTask.iProcess))
    {
        ShowNoteL(_L("Protected system process"));
        LogLine(_L("protected"));
        return;
    }

    TFindProcess finder(aTask.iProcess);
    TFullName name;
    TInt killed = 0;
    TInt skipped = 0;
    TUint self = (TUint) RProcess().Id();

    for (;;)
    {
        if (finder.Next(name) != KErrNone) break;

        RProcess process;
        if (process.Open(finder) != KErrNone) continue;

        if ((TUint) process.Id() == self || IsProtectedProcess(process.Name()))
        {
            skipped++;
        }
        else
        {
            process.Kill(0);
            killed++;
        }
        process.Close();
    }

    TBuf<160> note(_L("Killed "));
    note.AppendNum(killed);
    note.Append(_L(" process(es)"));
    if (skipped > 0)
    {
        note.Append(_L(", skipped "));
        note.AppendNum(skipped);
    }
    ShowNoteL(note);

    TBuf<64> log(_L("killed processes="));
    log.AppendNum(killed);
    LogLine(log);
}

void CTaskAdminAppUi::ShowNoteL(const TDesC& aText)
{
    CAknGlobalNote* note = CAknGlobalNote::NewLC();
    note->ShowNoteL(EAknGlobalInformationNote, aText);
    CleanupStack::PopAndDestroy(note);
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

static void LogLine(const TDesC& aText)
{
    RFs fs;
    if (fs.Connect() != KErrNone) return;

    fs.MkDirAll(_L("C:\\Data\\"));

    RFile file;
    const TInt KFileMode = EFileRead | EFileWrite | EFileShareAny;
    TInt error = file.Open(fs, _L("C:\\Data\\TaskAdmin.log"), KFileMode);
    if (error == KErrNotFound)
    {
        error = file.Create(fs, _L("C:\\Data\\TaskAdmin.log"), KFileMode);
    }

    if (error == KErrNone)
    {
        TInt size = 0;
        file.Size(size);
        file.Seek(ESeekEnd, size);

        HBufC8* ascii = HBufC8::NewLC(aText.Length() + 1);
        TPtr8 bytes = ascii->Des();
        bytes.Copy(aText);
        bytes.Append('\n');
        file.Write(bytes);
        CleanupStack::PopAndDestroy(ascii);
        file.Close();
    }

    fs.Close();
}

GLDEF_C TInt E32Main()
{
    LogLine(_L("main"));
    return EikStart::RunApplication(CTaskAdminApplication::NewApplication);
}
