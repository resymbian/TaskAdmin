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
#include <eiklbo.h>
#include <aknglobalnote.h>
#include <avkon.hrh>
#include <aknapp.h>
#include <akndoc.h>
#include <aknappui.h>
#include <aknselectionlist.h>
#include <apgtask.h>
#include <w32std.h>
#include <eikstart.h>

#include "TaskAdmin.hrh"
#include <TaskAdmin.rsg>

const TInt KMaxCaptionLength = 128;
const TInt KMaxProcessLength = 64;

class CTaskAdminAppUi;

/** One line of the task list, with enough identity to act on it later. */
struct TTaskInfo
    {
    TInt iWgId;
    TThreadId iThreadId;
    TUint iPid;
    TBuf<KMaxCaptionLength> iCaption;
    TBuf<KMaxProcessLength> iProcess;
    };

static void LogLine(const TDesC& aText);

/**
 * Selection list with our own Options menu. It only queues a command and
 * exits; the AppUi performs the action after the dialog is gone, so a task
 * cannot be killed while its own dialog still owns the screen.
 */
class CTaskAdminDialog : public CAknSelectionListDialog
{
public:
    static CTaskAdminDialog* NewL(TInt& aIndex, MDesCArray* aArray,
        CTaskAdminAppUi& aAppUi);

    void ProcessCommandL(TInt aCommand);

private:
    CTaskAdminDialog(TInt& aIndex, MDesCArray* aArray,
        CTaskAdminAppUi& aAppUi);
    void ConstructL();

    CTaskAdminAppUi& iAppUi;
};

class CTaskAdminAppUi : public CAknAppUi
{
public:
    CTaskAdminAppUi();
    void ConstructL();
    ~CTaskAdminAppUi();

    void HandleCommandL(TInt aCommand);

    void QueueAction(TInt aCommand, TInt aIndex);
    void RequestQuit();

    void RefreshTasksL();
    void ShowListL();

private:
    void RunActionL(TInt aCommand, TInt aIndex);
    void CloseTaskL(const TTaskInfo& aTask);
    void KillTaskL(const TTaskInfo& aTask);
    void KillProcessesL(const TTaskInfo& aTask);
    void ShowNoteL(const TDesC& aText);
    void ScheduleExitL();
    static TInt ExitCallback(TAny* aSelf);

    RArray<TTaskInfo> iTasks;
    CDesCArrayFlat* iCaptions;
    CIdle* iExitIdle;
    TInt iPendingCommand;
    TInt iPendingIndex;
    TBool iQuit;
};

/** Application/document boilerplate: a real GUI app gets a task-list entry. */
class CTaskAdminApplication : public CAknApplication
{
public:
    static CApaApplication* NewApplication();

    // The default would look beside the EXE in \sys\bin; our resource is
    // installed under \resource\apps with the other S60 UI resources.
    TFileName ResourceFileName() const;

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

TFileName CTaskAdminApplication::ResourceFileName() const
{
    // Same basename as the EXE, but in the standard resource directory.
    return TFileName(_L("\\resource\\apps\\TaskAdmin.rsc"));
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
// Dialog
// ---------------------------------------------------------------------------

CTaskAdminDialog::CTaskAdminDialog(TInt& aIndex, MDesCArray* aArray,
    CTaskAdminAppUi& aAppUi)
    : CAknSelectionListDialog(aIndex, aArray, NULL), iAppUi(aAppUi)
{
}

void CTaskAdminDialog::ConstructL()
{
    CAknSelectionListDialog::ConstructL(R_TASKADMIN_MENUBAR);
}

CTaskAdminDialog* CTaskAdminDialog::NewL(TInt& aIndex, MDesCArray* aArray,
    CTaskAdminAppUi& aAppUi)
{
    CTaskAdminDialog* self = new (ELeave) CTaskAdminDialog(aIndex, aArray,
        aAppUi);
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop(self);
    return self;
}

void CTaskAdminDialog::ProcessCommandL(TInt aCommand)
{
    switch (aCommand)
    {
    case ETaskAdminCmdRefresh:
    case ETaskAdminCmdCloseTask:
    case ETaskAdminCmdKillTask:
    case ETaskAdminCmdKillProcesses:
        {
        TInt selected = 0;
        if (ListBox() != NULL) selected = ListBox()->CurrentItemIndex();
        iAppUi.QueueAction(aCommand, selected);
        TryExitL(EAknSoftkeyCancel);
        break;
        }
    case EAknCmdExit:
        iAppUi.RequestQuit();
        TryExitL(EAknSoftkeyCancel);
        break;
    default:
        CAknSelectionListDialog::ProcessCommandL(aCommand);
        break;
    }
}

// ---------------------------------------------------------------------------
// AppUi
// ---------------------------------------------------------------------------

CTaskAdminAppUi::CTaskAdminAppUi()
    : iExitIdle(NULL)
{
}

void CTaskAdminAppUi::ConstructL()
{
    LogLine(_L("construct"));
    BaseConstructL();
    LogLine(_L("base done"));

    iCaptions = new (ELeave) CDesCArrayFlat(16);
    iPendingCommand = 0;
    iPendingIndex = 0;
    iQuit = EFalse;

    LogLine(RProcess().HasCapability(ECapabilityPowerMgmt) ?
        _L("powermgmt=yes") : _L("powermgmt=no"));
    LogLine(_L("start"));
    ShowListL();
}

CTaskAdminAppUi::~CTaskAdminAppUi()
{
    delete iExitIdle;
    iTasks.Close();
    delete iCaptions;
}

void CTaskAdminAppUi::HandleCommandL(TInt aCommand)
{
    if (aCommand == EAknCmdExit)
    {
        RequestQuit();
        Exit();
        return;
    }
    CAknAppUi::HandleCommandL(aCommand);
}

void CTaskAdminAppUi::QueueAction(TInt aCommand, TInt aIndex)
{
    iPendingCommand = aCommand;
    iPendingIndex = aIndex;
}

void CTaskAdminAppUi::RequestQuit()
{
    iQuit = ETrue;
}

void CTaskAdminAppUi::RefreshTasksL()
{
    iTasks.Reset();
    iCaptions->Reset();

    RWsSession ws;
    User::LeaveIfError(ws.Connect());
    CleanupClosePushL(ws);

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

        TBuf<KMaxCaptionLength> caption;
        if (ws.GetWindowGroupNameFromIdentifier(wg, caption) != KErrNone)
            continue;
        if (caption.Length() == 0) continue;

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
        info.iCaption.Copy(caption);
        info.iProcess.Copy(processName);
        User::LeaveIfError(iTasks.Append(info));
        iCaptions->AppendL(caption);
    }

    CleanupStack::PopAndDestroy(groups);
    CleanupStack::PopAndDestroy(&ws);

    TBuf<64> line(_L("tasks="));
    line.AppendNum(iTasks.Count());
    LogLine(line);
}

void CTaskAdminAppUi::ShowListL()
{
    while (!iQuit)
    {
        RefreshTasksL();

        if (iTasks.Count() == 0)
        {
            ShowNoteL(_L("No running tasks found"));
            iQuit = ETrue;
            break;
        }

        TInt index = 0;
        LogLine(_L("dialog new"));
        CTaskAdminDialog* dialog = CTaskAdminDialog::NewL(index, iCaptions,
            *this);
        LogLine(_L("dialog execute"));
        dialog->ExecuteLD(R_TASKADMIN_SELECT_DIALOG);
        LogLine(_L("dialog done"));

        if (iPendingCommand == 0)
        {
            iQuit = ETrue;
            break;
        }

        TInt command = iPendingCommand;
        TInt selected = iPendingIndex;
        iPendingCommand = 0;
        iPendingIndex = 0;

        if (command == ETaskAdminCmdRefresh) continue;

        TRAPD(error, RunActionL(command, selected));
        if (error != KErrNone)
        {
            TBuf<96> note(_L("TaskAdmin error "));
            note.AppendNum(error);
            TRAPD(noteError, ShowNoteL(note));
            (void) noteError;
            TBuf<96> log(_L("error="));
            log.AppendNum(error);
            LogLine(log);
        }
    }

    if (iQuit) ScheduleExitL();
}

void CTaskAdminAppUi::ScheduleExitL()
{
    // Exit() from inside ConstructL can tear down the AppUi before the
    // framework has finished starting it. Defer until the idle loop runs.
    if (iExitIdle != NULL) return;
    iExitIdle = CIdle::NewL(CActive::EPriorityIdle);
    iExitIdle->Start(TCallBack(ExitCallback, this));
}

TInt CTaskAdminAppUi::ExitCallback(TAny* aSelf)
{
    ((CTaskAdminAppUi*) aSelf)->Exit();
    return EFalse;
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
