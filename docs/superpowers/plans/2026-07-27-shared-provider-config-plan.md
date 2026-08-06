# Shared Provider Config Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: execute inline in the main checkout. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace per-applet provider definitions with one validated, watched KConfig value shared by every instance.

**Architecture:** A focused `SharedProviderConfig` owns `aiquotapilotrc/[Providers]/definitions`. The native applet delegates three QML-facing operations to it; QML keeps the old per-instance value only as a one-time migration fallback.

**Tech Stack:** C++17, Qt 6, KF6 KConfig, QML, Qt Test.

## Global Constraints

- Keep credentials and runtime snapshots unchanged.
- Reject malformed or oversized provider JSON without overwriting the last valid value.
- Save shared data only on KCM Apply/OK.
- Work directly in the main checkout; do not create a worktree.

---

### Task 1: Shared configuration store

**Files:**
- Create: `src/sharedproviderconfig.h`
- Create: `src/sharedproviderconfig.cpp`
- Create: `tests/cpp/tst_sharedproviderconfig.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `QString providers() const`, `bool ensure(const QString &)`, `bool save(const QString &)`, signal `providersChanged()`.

- [ ] Add tests that save a valid provider array, reject malformed/non-array/oversized input, reopen the file, and observe a notified write from a second store.
- [ ] Run `cmake --build build --target tst_sharedproviderconfig`; expect the target or symbols to be missing.
- [ ] Implement the smallest KSharedConfig/KConfigWatcher store with `KConfigBase::Notify`.
- [ ] Run `build/tst_sharedproviderconfig`; expect all cases to pass.

### Task 2: Applet and QML integration

**Files:**
- Modify: `src/aiusagewatcherapplet.h`
- Modify: `src/aiusagewatcherapplet.cpp`
- Modify: `package/contents/ui/main.qml`
- Modify: `package/contents/ui/config/ProvidersConfig.qml`
- Modify: `tests/tst_providerConfig.qml`

**Interfaces:**
- Produces on `Plasmoid`: `sharedProviders`, `ensureSharedProviders(QString)`, `saveSharedProviders(QString)`.

- [ ] Add a QML backend mock and assertions that shared providers initialize the model and Apply calls `saveSharedProviders`.
- [ ] Delegate applet properties and invokables to `SharedProviderConfig`.
- [ ] Make `main.qml` prefer `sharedProviders` and migrate the current instance only when shared data is empty.
- [ ] Make the provider KCM initialize from shared data and persist it only in `saveConfig()`.
- [ ] Run the focused C++ and QML tests.

### Task 3: Safe migration and deployment

**Files:**
- Runtime backup under `~/.config/aiUsageWatcher-recovery-backups/`.

**Interfaces:**
- Consumes: formal applet `providers` configuration after graceful `plasmashell` shutdown.
- Produces: `~/.config/aiquotapilotrc` and reloaded formal panel.

- [ ] Back up `plasma-org.kde.plasma.desktop-appletsrc`, `plasmawindowedrc`, and any existing `aiquotapilotrc`.
- [ ] Stop the standalone `plasmawindowed`, gracefully stop `plasmashell`, and confirm the formal applet config is on disk.
- [ ] Build and install to `~/.local`.
- [ ] Start `plasmashell`; verify the formal panel uses the transparent provider layout and shared provider count.
- [ ] Start a fresh `plasmawindowed`; verify its provider IDs equal the shared formal IDs.
