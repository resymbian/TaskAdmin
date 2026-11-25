# TaskAdmin 1.0.2

Removes `PowerMgmt` from the EXE. A self-signed SISX cannot be granted that
system capability on an unpatched phone, and the loader then refuses to start
the process at all - the icon appeared but nothing happened. **Cerrar tarea**
works without it; **Matar tarea** and **Matar procesos** now show
`PowerMgmt no disponible` instead of panicking. To get real kill support the
package must be signed with a developer certificate that grants `PowerMgmt`.

The resource-path and deferred-exit fixes from 1.0.1 are kept, and the log now
records `powermgmt=yes/no` right after `start`.

## 1.0.1

Fixes the startup "system error": the UI resource is now loaded explicitly from
`\resource\apps\TaskAdmin.rsc` instead of the default `\sys\bin\TaskAdmin.rsc`,
which is not where the SISX installs it. Exit is deferred to the idle loop so
the AppUi is fully constructed first, and startup steps are logged to
`C:\Data\TaskAdmin.log` for further diagnosis (`main`, `construct`, `base done`,
`start`, `tasks=N`, `dialog new`, `dialog execute`, `dialog done`).

## 1.0.0

Small task manager for the Nokia E51 / S60 3rd Edition FP1.

Install **`TaskAdmin.sisx`**. It is self-signed, so the phone needs the
installserver and Open4All patches that the other Resymbian packages already
use.

Open **Task Admin** from the application menu. The list shows the running
window groups; the Options menu offers:

* **Actualizar** - reread the task list;
* **Cerrar tarea** - polite close (`TApaTask::EndTask`);
* **Matar tarea** - hard close of the window group (`TApaTask::KillTask`);
* **Matar procesos** - kill every process with the same executable name as the
  selected task (`RProcess::Kill`).

`Matar procesos` is the one that can stop background agents and Java MIDlets,
but all Java MIDlets share the same runtime process: using it closes Discofi,
ResyGram and NotiHelper together. Try `Cerrar tarea` first. A small list of
system servers is protected so the menu cannot take down the phone itself.

The EXE requests `PowerMgmt`, needed to end tasks it did not create. Actions
are logged to `C:\Data\TaskAdmin.log` so failures can be reported from the
file manager.

This is the first release: no previous version, no protocol, no data format.
