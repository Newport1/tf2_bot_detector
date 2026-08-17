# TODO

Living task list. Historical context for the mh_stuff work lives in `migration.md`.

Priority scale:
- **P0** — Fix now (broken UX or dead ends)
- **P1** — High-leverage technical debt
- **P2** — High user value, moderate effort
- **P3** — Polish / cleanup
- **P4** — Nice-to-have
- **P5** — Large / deferred

Items marked **[upstream]** were surfaced from issues and notes on the repo this was
forked from (surepy / PazerOP). Each one below was checked against this fork's source
before being listed — see [Upstream items already fixed here](#upstream-items-already-fixed-here)
for the ones that turned out to be already resolved.

---

## Cross-platform / Shared

### P0

- [ ] **The update check can hang the setup flow with no way past it.** [upstream]
      `UpdateCheckPage::CanCommit` (`SetupFlow/UpdateCheckPage.cpp:296`) returns false while the
      status is `CheckQueued` or `Checking`. `UpdateManager::Update` (`UpdateManager.cpp:282-321`)
      only leaves those states when the `std::future<BuildInfo>` becomes ready or throws — and
      **no timeout or deadline is set anywhere**, on the future or on the HTTP client.
      A server that accepts the connection but never answers (or a stalled DNS/connect) leaves
      the page in `Checking` forever, with Next permanently greyed out and no skip.
      A *refused* connection is fine — HTTPClient's retry path gives up and raises, which lands
      in `CheckFailed`, and `CanCommit` allows that. So the fix is the missing deadline, not the
      error handling. Needs: a connect/read timeout, plus a wall-clock deadline that forces
      `CheckFailed`, plus a "skip / continue anyway" escape on the page itself.

- [ ] **First-run settings gate gives no reason when it blocks.** [upstream]
      `BasicSettingsPage::CanCommit` → `InternalValidateSettings`
      (`SetupFlow/BasicSettingsPage.cpp:9-21`) requires a valid Steam dir, a valid tf dir *and*
      a valid local SteamID before Next enables — but the page renders all three inputs
      unconditionally and shows no per-field pass/fail. The user sees a dead Next button and no
      indication of which of the three is unsatisfied. Surface the individual validator result
      (`DirectoryValidatorResult::m_Message` already carries a usable string) next to each field.
      Note the Steam Web API key is **not** part of this gate, contrary to the upstream report —
      a missing API key does not block the flow in this fork.

### P1

- [ ] **Drop cpprestsdk → cpp-httplib.** Detail in
      [Drop cpprestsdk (detail)](#drop-cpprestsdk-detail).
      - Add cpp-httplib to `vcpkg.json`, swap the CMake `find_package`
      - Rewrite the ~50 transport lines in `HTTPClient.cpp`
      - Remove the cpprestsdk dependency
      - **Set connect/read timeouts** — directly unblocks the P0 above
      - Optional brotli parity
      - Unblocks moving the CI Windows runner off `windows-2022`

### P2

- [ ] **Add a general-purpose player tag** (`custom` / `etc`). Raised from live testing —
      detail in [Live testing, 2026-08-16 (detail)](#live-testing-2026-08-16-detail).
      - Preferred: one new allowed attribute value + minimal UI support
      - Alternative: keep the official four attributes and put the real reason in `proof[]`
        (+ an optional conversion script)
- [ ] **Manual SteamID add** — mark a player offline, without having to be in a match with them.

### P3

- [ ] Finish dropping the `mh::stuff` INTERFACE target from the root `CMakeLists.txt`
- [ ] `git rm` the `submodules/mh_stuff` submodule + its `.gitmodules` entry (once the build is
      fully green)
- [ ] Bump fmt to 11/12, drop the `_SILENCE_STDEXT_ARR_ITERS` workaround, delete the dead
      `fmt::formatter<mh::source_location>`
- [ ] Optional: de-`mh`-ify the remaining vendored headers
- [ ] **Document that SteamHistory only supplies SourceBans data** [upstream] — player lists
      still have to be placed in `cfg/` as `playerlist.*.json`. Currently ambiguous in both the
      UI and the docs, and users read the API key as enabling list sync.
- [ ] **Document — and preferably guarantee in code — that updating never overwrites a user's
      personal `playerlist.json`.** [upstream] Fear of losing marks is a recurring reason people
      avoid updating.

### P4

- [ ] Font settings persistence across reboots
- [ ] Highlight new / low-age accounts
- [ ] Auto-disconnect / warn on bot-heavy servers
- [ ] Open profiles in the Steam client instead of a browser

### P5 (deferred)

- Full arbitrary custom labels / colors / icons
- "Good Noodle" / safe list as a first-class concept
- Real-name rules as a first-class trigger
- Profile-picture hash checking
- Auto-F1 vote / auto-mute
- Window size persistence, team-color matching, map-change logging, etc.

---

## Windows

### P0

- [x] **Live-tested 2026-08-17, both orderings pass.** Run log
      `build/tf2_bot_detector/logs/2026-08-17_12-43-33.log`.
      - *No Steam:* two clean starts, `SteamID [I:0:0]` (invalid, Steam not logged in), graceful
        shutdown both times. Steam dir and TF dir resolved correctly, TF2 on `E:\SteamLibrary`.
      - *Launch path still works:* RCON `Connection established!` on socket 3048, console.log
        opened from the external drive. That required `IsTF2Running()` true through the new
        process scan — the path the window lookup used to serve.
      - *Quit TF2, TF2BD running:* TF2 exited 12:44:58. RCON retried at 12:44:59, 12:45:06,
        12:45:11, 12:45:16 — **four attempts, then it stopped**, versus the pre-fix Linux
        behaviour of retrying every ~5s forever ("until we literally run out of ports",
        surepy #25). The launch button came back: the relaunch at 12:45:00 was a real click,
        confirmed by `auto_launch_tf2: false` in settings.json, so it was not the auto-launch
        path firing.
      - *Reverse, quit TF2BD with TF2 still up:* `Disconnecting Socket (656)` → `Graceful
        shutdown` at 12:46:09, TF2 left running. Clean.
      - The `forcibly closed` teardown burst is still exactly 8 lines, matching the previous
        session — unchanged, still cosmetic.

- [ ] **`DrawLaunchTF2Button` logs "TF2 already running!" and then launches anyway.**
      `SetupFlow/TF2CommandLinePage.cpp:458` — `if (IsTF2Running()) LogError(...)` with no
      `return` and no `else`, so it falls straight through to `OpenTF2`. The guard has never
      guarded anything. It mattered less when `IsTF2Running` was a window-class lookup that
      answered a different question; now that it is accurate, this is worth making real —
      skip the launch and surface the reason via `m_Data.m_LaunchError` so the button does not
      appear to do nothing. Deliberately not changed in the same pass as the fix above, so the
      live test result stands against exactly what was tested.

- [x] **`IsTF2Running` switched from a window lookup to a process scan.**
      Was `FindWindowA("Valve001", nullptr)` — a *window class* lookup, wrong in both directions:
      `Valve001` is the generic Source engine class, so any other Source game read as TF2; and
      the window dies before the process does, so RCON could be released while TF2 was alive
      (`FindWindow` is also desktop/session-scoped). Now mirrors `Linux::IsTF2Running`: a 2s
      cached scan over `TF2ProcessNames()`.

- [x] **`IsProcessRunning` hardened** — it had a single caller, so its defects were latent;
      routing `IsTF2Running` through it would have made all of them live.
      - The static handle cache had **no mutex**, and is now polled from two paths.
      - Keyed on `string_view::data()`, which is not guaranteed null-terminated. Every caller
        passed a literal, so it never bit — but `TF2ProcessNames()` now feeds it.
      - `OpenProcess` result cached unchecked; it returns NULL on access-denied (elevated TF2),
        after which `GetExitCodeProcess(NULL)` fails and `CloseHandle(NULL)` logged an error
        every poll. Only non-null handles are cached now, and they are `SafeHandle`-owned.
      - `CreateToolhelp32Snapshot` was not checked for `INVALID_HANDLE_VALUE` — which is
        non-null, so `HandleDeleter` would have closed it.

- [x] **`g_SkipOpenTF2Check` made usable.** Was declared `extern` under `_DEBUG` and defined
      nowhere, so referencing it was an unresolved external. Now defined and honoured by
      `IsTF2Running`, which is the only way to exercise the TF2-running branches of the setup
      flow without a running TF2.

- [x] **`Tests/WindowsProcessTests.cpp`** — the counterpart to `LinuxProcessTests.cpp`.
      Covers a child observed starting and exiting through the handle cache, a name that cannot
      exist not poisoning the cache, a deliberately non-null-terminated `string_view`, and
      `IsTF2Running` agreeing with a hand-rolled scan of `TF2ProcessNames()`.
      Honest limits: the `string_view` case pins the intended behaviour but would not have
      *failed* the old code (the miswritten key poisoned the cache without changing the answer),
      the mutex fix is not covered because races are not deterministic, and the `IsTF2Running`
      case only becomes load-bearing with TF2 actually running — which is what the live test
      above is for.

### P1

- [ ] (Blocked on shared P1) Move the CI Windows runner from `windows-2022` to `windows-latest`
      once cpprestsdk is gone

### P3

- [ ] Live-test auto-kick and auto-chat — still completely unexercised on Windows
- [x] `launch_tf2bd_linux.sh` no longer ships inside the Windows zips. Both jobs upload the
      shared `staging/` folder, and the script is checked in there, so it reached Windows users
      while *not* reaching Linux users (the AppImage zip is built from `dist/`). The Windows
      "Prepare staging/" step now removes it. **Needs a dispatch CI run to verify** — it is a
      workflow change and cannot be exercised locally.

### Observed / no action needed (Windows)

- Burst of `WSAECONNABORTED` at shutdown → teardown race, cosmetic only
- `chat_*.log` / `console_*.log` read 0 bytes while the app holds the handle open (normal
  Windows behaviour; the size appears after exit)
- Blank player entry during a session → the player had left, had not finished connecting, or
  (2026-08-17) the server was joined while it was already on the end-of-map vote

---

## Linux

### P0

- [ ] **Harden `getPidFromProcessName`** (`LinuxHelpers.h:21`).
      Currently `strstr(cmdline, name)` against `/proc/<pid>/cmdline` — a substring match on
      argv[0] — returning the first `readdir` hit. TF2 runs behind
      `reaper → pressure-vessel → tf.sh → tf_linux64`, so the match can latch onto a wrapper.
      This is the same hazard the `kTF2ProcessNames` comment in `PathUtils.cpp` warns about,
      reintroduced one layer down.

### P3 — AppImage polish (core path already working)

Detail in [AppImage (detail)](#appimage-detail).

- [ ] Replace the icon (currently a 32px frame upscaled to 256, blurry)
- [ ] Test on a few distros, older glibc especially
- [ ] Discord on Linux: confirm `discord_game_sdk.so` is bundled correctly for non-static builds
- [ ] Optional: build inside the sniper SDK for a wider ABI floor
- [ ] Optional: wire AppImage generation as a CMake target, not only CI

### P3 — CI

- [ ] Remove the stale `submodules/mh_stuff/libmh-stuff.so` copy from `build-linux.yml:151`
      (header-only now — this will fail the Linux discord-integration artifact upload)

### P4

- [ ] **`ChatWrappersGeneratorPage` segfault** [upstream] — reported by kamild, May 2026, on the
      "Generating chat message wrappers..." step (`SetupFlow/ChatWrappersGeneratorPage.cpp:51`).
      **Not reproduced on this fork.** Same general area as surepy #57. Needs a repro before it
      is worth acting on; keep at P4 until someone hits it here.
- [ ] Linux chat corruption (surepy #57) — same condition: only if actually reproduced here

### Done (Linux)

- [x] Zombie process detection (`kill(pid, 0)` → read `/proc/<pid>/stat`, state `Z`)
- [x] Regression test that forks a real zombie

---

## Upstream items already fixed here

Checked against source while folding in the upstream notes. Listed so they don't get
re-raised.

- **"Expected file `../hl2.exe` does not exist" dead end.** Fixed. `ValidateTFDir`
  (`Util/PathUtils.cpp:141-152`) now calls `FindTF2Executable`, which accepts
  `tf_win64.exe` / `tf.exe` / `hl2.exe` (`PathUtils.cpp:30-34`), and the failure message is
  "No recognized TF2 executable found next to this tf/ folder" rather than naming one hardcoded
  binary. `GeneralSettings::GetBinaryName` (`Config/Settings.cpp:259-282`) picks by
  `TFBinaryMode` with a fallback, so the 64-bit rename is handled end to end.
- **Missing `gameoverlayui.exe` blocking Steam-dir validation.** Fixed in `899de30`;
  `ValidateSteamDir` falls back to `gameoverlayui64` (`PathUtils.cpp:184-190`).
- **SteamRep "goto profile" link.** Removed from `AddDefaultGotoProfileSites`
  (`Config/Settings.cpp:488`). Only affects new/default configs — an existing `settings.json`
  keeps its saved `goto_profile_sites` until reset.
  - [ ] Still open: **capture the SteamRep DB dump** as a seed for a future offline list,
        before the domain lapses and it becomes unrecoverable.

---

# Details

## Drop cpprestsdk (detail)

**Why:** cpprestsdk is archived by Microsoft and was **removed from vcpkg** upstream
(`9ceec72e0a [cpprestsdk, azure-storage-cpp] Deindex (#52130)`). Our pinned baseline
(`2537044…`, 2022-10-28) still has it at **2.10.18**, which uses `stdext::checked_array_iterator`
— removed in the VS 2026 / MSVC 14.51 toolset, so it fails to compile on newer toolchains
(the reason CI is pinned to `windows-2022`). Upstream did patch it (`#51750`, in 2.10.19) but
then deindexed it, so bumping the baseline is not a durable path.

**Scope is small and isolated** — only `tf2_bot_detector/Networking/HTTPClient.cpp` touches
cpprestsdk. Everything around it is transport-agnostic:
- Public interface `IHTTPClient` (`GetString` / `GetStringAsync → mh::task<std::string>` /
  `GetRequestCounts`) leaks no cpprest types.
- Error types `http_error` / `HTTPResponseCode` / `URL` (`HTTPHelpers.h`) are built on
  `mh::error_condition_exception` + nlohmann — no cpprest.
- ~10 call sites (SteamAPI, LogsTFAPI, GithubAPI, ConfigHelpers, SteamHistoryAPI) only
  `co_await GetStringAsync(...)`. No call-site churn.
- The throttle/retry/counting logic (bulk of HTTPClient.cpp) stays as-is.

cpprest is used only for: per-host `web::http::client::http_client` (cached by
`GetSchemeHostPort`), `request(GET, path)`, `status_code()`, `extract_utf8string`,
`http_exception`, and `utility::conversions::to_string_t`.

**Recommendation: cpp-httplib over libcurl.**
- `HTTPClient.cpp:1-2` already define `CPPHTTPLIB_OPENSSL_SUPPORT` / `CPPHTTPLIB_ZLIB_SUPPORT`,
  and cpp-httplib was already trialed here (it was in the deleted nuget binary cache). It's just
  not in `vcpkg.json` / CMake currently.
- `res->body` is already a UTF-8 `std::string` → deletes every `utility::conversions::to_string_t`
  call (the UTF-16 dance only existed because cpprest uses `wstring` on Windows).
- Header-only, `Get(path)` is one call; no write callbacks / handle lifecycle.
- libcurl is heavier here: write-callback boilerplate, `curl_global_init`, and a `CURL*` easy
  handle can't be shared across threads (breaks the per-host cache + thread-pool offload). No
  functional gain for a GET-only use case.

**Async bridge is already solved:** both libcurl and cpp-httplib are blocking, but
`mh::thread_pool::add_task(fn)` returns an `mh::task<T>` you can `co_await` (runs the blocking
call on a pool thread, resumes the coroutine on completion). This also **deletes the
`#ifdef __linux__ … co_await … #else pplawait …` split** (HTTPClient.cpp:168-182) and `pplawait.h`.

**Steps:**
- [ ] Add cpp-httplib to `vcpkg.json`; swap `find_package(cpprestsdk)` / `cpprestsdk::cpprest`
      in `tf2_bot_detector/CMakeLists.txt:378,392` for cpp-httplib.
- [ ] Rewrite the ~50 transport lines in `HTTPClient.cpp` (cache `httplib::Client` per
      scheme+host+port; `co_await pool.add_task([cli,path]{ return cli->Get(path); })`; map
      `!res`/`res.error()` to the retry path and `res->status` to `http_error`).
- [ ] Remove the `cpprestsdk` dep from `vcpkg.json`.
- [ ] Set `set_connection_timeout` / `set_read_timeout` — **this is what closes the shared P0
      update-check hang**; add `CPPHTTPLIB_BROTLI_SUPPORT` if brotli parity with cpprest's
      `compression` feature is wanted.
- [ ] Note: `httplib::Client` serializes requests via an internal socket mutex — fine given
      per-host throttling already mostly serializes, but worth verifying under concurrency.
- [ ] Once done, CI can move back off the `windows-2022` pin to `windows-latest`.

## AppImage (detail)

A basic AppImage builds and runs (verified locally). Turned out easy: the binary is nearly
self-contained (vcpkg deps static-linked; only `libtbb`/`libstdc++`/`libgcc_s` are private
dynamic deps; SDL2 `dlopen`s host X11/GL/wayland, which we leave to the host).

**Distribution model:** the AppImage is *just the binary*. `cfg/ fonts/ images/ licenses/ logs/
temp/ tf2_addons/` + the `.AppImage` ship together in one folder, like the Windows portable zip —
nothing packaged *inside* the read-only image. `dirname($APPIMAGE)` is the writable portable folder
for both reads and writes; no read/write split, no XDG, no separate data dir.

**Done:**
- [x] **Data-dir fix:** `Platform::GetCurrentExeDir()` (Linux) returns `path($APPIMAGE).parent_path()`
      when `$APPIMAGE` is set, else `/proc/self/exe`'s dir. Single caller (`Filesystem.cpp:73 →
      m_ExeDir`), so the search path / Steam-cwd `current_path()` chdir / `GetLocalAppDataDir` /
      `GetTempDir` all follow. (Also fixed a latent `readlink` non-null-termination bug.)
- [x] **Build script:** `packaging/linux/build-appimage.sh` — self-contained (downloads
      appimagetool, uses committed 256px `packaging/linux/tf2_bot_detector.png` so CI needs no image
      tooling), `ldd`-bundles the private libs (auto-includes `discord_game_sdk.so` if a non-static
      build links it), writes `.desktop` + `AppRun` (sets `SDL_VIDEODRIVER=x11`, no chdir), runs
      appimagetool with `APPIMAGE_EXTRACT_AND_RUN=1` (FUSE-less). Output: `dist/*.AppImage`.
- [x] **CI:** `build-linux.yml` builds the AppImage and uploads `dist/` (with resources copied
      beside the `.AppImage`) as `tf2-bot-detector_appimage_*`.

**Remaining detail on the open items above:**
- Icon: currently a 32px `.ico` frame upscaled to 256 (blurry); want proper hi-res art.
- Older glibc: see whether `libstdc++`/`libgcc_s` bundling is enough or more compat work is needed.
- Discord: the current static build excludes discord (`platform: "!static"`); when shipping it,
  confirm `discord_game_sdk.so` gets bundled (the `ldd` loop already would) and works.
- Sniper SDK: Steam Runtime 3.0, glibc 2.31 — a wide floor, guaranteed present post-TF2-x64
  (TF2 requires sniper, see `TF2CommandLinePage.cpp:294`). Run host-side; do **not** launch TF2BD
  *through* sniper (TF2BD launches TF2 via sniper → nested pressure-vessel).

## Live testing, 2026-08-16 (detail)

Raised from a ~2h live Windows session on `01fdb9f`. No bots or cheaters encountered, so
detection itself is untested against real targets.

**General-purpose player tag.** Today every mark has to borrow an existing semantic tag, which
corrupts the meaning of that tag's data. Real cases from the session: a possible
YouTuber/streamer, a suspicious player who might just be very good, a friend not yet added, and
a player encountered often in pubs. None of those are cheating claims and none fit the existing
attributes. `racist` was used as a stand-in during testing — **those marks are placeholders, not
real classifications**, and the resulting `playerlist.json` entries should be re-tagged or
dropped rather than shipped.

**TF2 quit left TF2BD stuck on the player list — FIXED on Linux, root cause was `kill(pid, 0)`.**
Reported 2026-08-16 from live testing on *Linux*: after quitting TF2, both Steam ("Quit Game")
and TF2BD still showed it running, and TF2BD stayed on the empty player-list panel instead of
returning to the launch button.

Root cause: `Linux::IsProcessRunningPid` was `return kill(pid, 0) == 0`, and **`kill(pid, 0)`
succeeds for a zombie** — a process that has exited but has not been reaped by its parent. TF2
routinely lingers as `<defunct>` on quit, which is the same reason Steam keeps offering
"Quit Game". Verified by forking a real zombie: `/proc` state `Z`, `kill(pid,0) == 0`.

Consequences, all from that one line: `IsTF2Running()` never went false, so
`TF2CommandLinePage::ValidateSettings` never released the RCON client and never returned
`TriggerOpen`; the UI had no path back to the launch button; and TF2BD reconnected to a dead
listener every ~5s indefinitely (visible in the log at 22:25 as `Socket (104) opened` → 4s →
`socket error` → repeat). That retry loop is the same one that produced surepy issue #25,
"until we literally run out of ports".

Fixed by reading the state field of `/proc/<pid>/stat` and treating `Z` as not running.
Regression test `Tests/LinuxProcessTests.cpp` forks an actual zombie; it fails against the old
implementation and passes against the new one.

Note the RCON *connection* was already being refused while this was happening, i.e. TF2's engine
was fully gone. TF2BD was not holding TF2 open — it was only failing to notice.

The Windows equivalent of this path has **not** been retested — see Windows P0.

### Observed and explained, no action needed

- A player entry rendered blank during the session. Cause identified: the player had left, or had
  not finished connecting. Not a defect.
- RCON logs one burst of `WSAECONNABORTED` at shutdown (8 lines at `21:57:45`). Root cause is a
  teardown race — `disconnect()` closes a live socket while `read_packet_len()` is blocked in
  `recv`, which Winsock reports as an abort where POSIX would not. Fires only at teardown, after
  play has stopped, and is purely cosmetic. Intermittent because it depends on whether a read
  happens to be in flight at close time (a clean quit at `19:47:27` produced none).
- `chat_*.log` and `console_*.log` read as **0 bytes while the app is running**. Windows does not
  refresh a file's directory-entry size while a handle is open; the data is being written the
  whole time. This session's were 16.9 KB and 6.1 MB once the app exited. Not a bug, but it looks
  exactly like one during testing.
- Auto-kick and auto-chat were **not exercised** this session. Still untested on Windows.

## Carried over from migration.md (still open)

- [ ] **Finish dropping the `mh::stuff` shim.** SourceRCON no longer uses mh
      (`locked_value` → `std::mutex` done in fork commit `f0275ed`; its CMake `mh::stuff`
      link + FetchContent block are gone). Now delete the `mh_vendored` / `mh::stuff` INTERFACE
      target from the root `CMakeLists.txt` — nothing depends on it anymore.
- [ ] **`git rm` the `submodules/mh_stuff` submodule** and its `.gitmodules` entry. Held back
      until the build is fully green so originals stay available for reference.
- [ ] **fmt 11/12** — requires bumping the vcpkg submodule + `builtin-baseline` to a 2025+ commit
      (re-resolves all ports). Then drop the `_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING`
      workaround in `tf2_bot_detector_common/CMakeLists.txt`.
- [ ] **Delete the now-dead custom `fmt::formatter<mh::source_location>`** (fmt 10 provides one
      for `char`; ours is a partial spec that's dead for `char`).
- [ ] **Optional: de-`mh`-ify** — move the vendored `mh/` headers under their own namespace if
      full de-mh-ification is wanted (kept `mh::` to minimize churn).

## Recently completed

- [x] SourceRCON `mh::locked_value<srcon_addr>` → plain value + `std::mutex` (fork `f0275ed`)
- [x] Pruned unused vendored `mh/` headers (deleted 17; closure now 59/59, no dead files)
- [x] CI pinned to `windows-2022`; removed NuGet binary caching from both workflows
      (+ vestigial `VCPKG_CACHE_VERSION`)
- [x] Deleted all 117 stale vcpkg binary-cache nuget packages from GitHub Packages
- [x] First clean Windows compile + link of this fork
- [x] Windows launch-option parsing fix (no more argv[0] rules on Steam launch options)
- [x] Steam library discovery rewritten for the modern `libraryfolders.vdf` format
- [x] SteamRep goto-profile link removed
- [x] v1.7.0 released; README split into Windows / Linux install sections
