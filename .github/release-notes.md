# TaskAdmin 1.0.0

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
