# TaskAdmin

A small S60 3rd Edition task manager for the Nokia E51 (RM-244 / S60 3rd FP1),
built with the same native toolchain as NotiHelper.

It lists the phone's running tasks from the window server and lets the Options
menu close or kill the highlighted one. `Matar procesos` also kills every
process with the selected task's executable name, which is the only practical
way to stop a background agent from a phone menu.

## Install

1. The SISX is self-signed and now requests only user capabilities, so no
   special patches are needed to install or launch it.
2. Install `TaskAdmin.sisx`. UID3 is `0xA0000D16`, so it is a separate app from
   NotiHelper (`0xA0000D15`).
3. Open **Task Admin** from the application menu.

## Menu

| Command | Effect |
| --- | --- |
| Actualizar | Rereads the window groups. |
| Cerrar tarea | `TApaTask::EndTask()` - polite close, try this first. |
| Matar tarea | `TApaTask::KillTask()` - hard close of the window group. |
| Matar procesos | `RProcess::Kill(0)` for every process with that executable name. |
| Salir | Closes TaskAdmin. |

**Careful with Java apps.** All MIDlets run inside one Java runtime process, so
`Matar procesos` on Discofi, ResyGram or NotiHelper closes all of them. If you
only want to leave one app, use `Cerrar tarea`, then go back to Task Admin from
the menu until the task disappears.

A short protected-process list (`SysAp`, `EikSrv`, `WsSrvr`, `AppArf`, `EApps`,
`StartContent`, kernel/file/socket servers, ...) prevents the menu from killing
the phone itself. Anything else is fair game, including system applications;
close those only if you know how to reboot.

Every action is appended to `C:\Data\TaskAdmin.log`. It contains the command,
window-group id, PID and caption, plus `closed`, `killed task`,
`killed processes=N` or an error code.

## Capabilities

The EXE requests only `LocalServices ReadUserData WriteUserData`. `Cerrar
tarea` works with those. `Matar tarea` and `Matar procesos` need `PowerMgmt`,
which a self-signed certificate cannot grant: including it made the loader
refuse to start the EXE on an unpatched phone, so it was removed. The commands
remain visible and show `PowerMgmt no disponible` when tapped. A developer
certificate that grants `PowerMgmt` is needed to enable real killing.

## Build

With the Nokia S60 3rd FP1 SDK:

```sh
EPOCROOT=/path/to/s60/sdk/ ./build.sh
```

Or, reproducibly, with Docker (the SDK is a large archive.org download; the
GitHub Actions workflow caches it):

```sh
docker build -f docker/Dockerfile -t taskadmin-sdk .
chmod 777 out
docker run --rm -v "$PWD:/src:ro" -v "$PWD/out:/out" taskadmin-sdk
```

The workflow in `.github/workflows/build.yml` publishes `TaskAdmin.sisx` and
`SHA256SUMS` as a GitHub Release tagged `v1.0.0` on every push to `main`.
