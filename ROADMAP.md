# AVManager — Roadmap

Where the project is going and why. Working document: it is updated as decisions are made.

**How to read it**

- Every item has a **permanent number**. Numbers are never reused or renumbered, even when
  an item is finished, dropped or moved to another stage. New items take the next free
  number, whichever stage they land in. **Next free number: 45.**
- Tags say what kind of work an item is: `feature`, `bug`, `refactor`, `test`, `infra`,
  `docs`, `decision` (a question to settle before building), `investigate`.
- Status: ⬜ open · 🚧 in progress · ✅ done · ✖ dropped. Finished items stay where they are.

---

## Strategy

**Goals**

1. A low-resource background tool that gets used every day, with two functions:
   - **1A** — the keyboard's volume keys are remapped to control a Denon AVR-X3800H
     directly, with modifiers to control speaker groups and Windows (which normally stays
     at 100%), and an on-screen display showing the state of the AVR, including how
     confident that state is.
   - **1B** — a monitor that checks the audio chain is actually in the mode Windows thinks
     it is and renegotiates if necessary, with the ambition of automatically selecting the
     ideal mode for whatever is playing, be it Kodi, a browser or a game.

2. Learn and practice: expand my knowledge of C++, deliberately try interesting constructs
   and Windows features, and practice professional standards, including Git, CI/CD and
   simulators for HIL/SIL testing, with a history that explains itself.

**Principles**

- **State follows evidence.** A value moves to "good" only on something observed (a reply
  parsed, a connect that returned 0), and away on any observed fault. A stored health status
  stores the last observation, not the truth.
- **Every wait ends on an event or a known deadline.** No fixed polling intervals.
- **One owner per fact.** For example, the command cooldown is a property of the connection
  (a rate limit protecting the receiver); the response deadline is a property of a request.
  Mixing the two produced a real bug (item 3).
- **Tested before it reaches `main`.** Initially this means every push has been manually
  tested with Unit tests and manual test cases. This will move to simulator tests run in CI
  and tests against the real receiver run locally before merging. Untested work lives on a
  short-lived branch.

**Current focus:** closing Stage 1 with a clean-up (items 11, 24), then a first CI pass
(items 13–15) so the Stage 3 split happens with tests underneath. Items 16 and 17 follow
the split.

**Open strategic questions:** items 11, 19, 25.

---

## Stage 1 — Keyboard volume control, end to end

The first milestone that makes the program useful: press a volume key, the receiver moves.
The event-driven control loop is rebuilt only as far as this needs.

- **1** · ✅ · `feature` — Sleep-driven control loop: receive without waiting, then sleep
  until the nearest pending deadline or a 1 s heartbeat, instead of re-checking every 15 ms.
- **2** · ✅ · `feature` — Key handling in the controller: read the hook's key counters,
  turn them into requested volume / mute / channel-level changes, clamp to limits.
  Tested end to end on the receiver: main volume, centre, surrounds, limits, mute, and
  remote-control changes flowing back. Keys pressed during startup are discarded.
- **3** · ✅ · `bug` — A stale response deadline makes the idle loop spin after a ping or
  resync. The deadline should be set by whoever starts a request (`SyncOut`), not by the
  transport (`Send`).
- **5** · ✅ · `bug` — `SyncResolve` must not judge a request that hasn't been sent yet. The
  condition is "unsent work exists", not a timer state (treating an unset deadline as
  "wait" creates a stuck state).
- **7** · ✅ · `feature` — Keyboard wake: the hook callback posts a content-free "look
  again" message to the control thread. Clear the registered thread ID when the loop exits.
- **8** · ✅ · `feature` — `main` runs the keyboard hook and the control loop together.
- **9** · ✅ · `bug` — `SyncOut` has no channel-volume branch, so Ctrl or Alt + volume ends
  the control loop.
- **11** · ⬜ · `decision` — The Ctrl+Alt+End quit shortcut in the hook callback: keep, remap
  or remove. (With the hook on its own thread it stops the hook, not the program.)
- **24** · 🚧 · `refactor` — Clean-up, limited to what survives the Stage 3 split:
  `explicit` on `AVRHandler`'s single-argument constructor; consistent member naming
  (trailing `_` on the new private constants; `stayalive` is public and writable);
  consistent reset defaults between `KeyboardHook::GetError` and `Stopwatch::Read`.
  Done: `explicit` on `Timer` and `Stopwatch`, both in `timing.h`; construction failure is
  reported through a status member with a getter for now (setup moves into the receiver
  link's startup in Stage 3).

## Stage 2 — Public release and CI

- **12** · 🚧 · `docs` — README: purpose, status, how to build, architecture overview, link
  to Denon's official control protocol document.
- **13** · ⬜ · `test` — GoogleTest via CMake `FetchContent`, registered with CTest. Port the
  existing dB-conversion test to it.
- **14** · ⬜ · `test` — Unit tests for the other pure functions: `StringToDb`, `MakeCommand`,
  `Parse`, `ProcessCombo`.
- **15** · ⬜ · `infra` — GitHub Actions: build and run tests on `windows-latest` on every
  push.
- **16** · ⬜ · `infra` — Build a release executable on version tags and attach it to a
  GitHub Release.
- **17** · ⬜ · `investigate` — Can GitHub's hosted Windows runners run tests that need a
  low-level keyboard hook and `SendInput`?
- **18** · ✅ · `decision` — When to make the repository public. Public since October 2026.

## Stage 3 — Split `AVRHandler` along the test seam

`AVRHandler` currently does everything: socket, protocol, state machine, policy and event
loop. The split is decided by testability: whatever should be testable without a receiver
goes on the side whose other half can be faked.

- **19** · ⬜ · `decision` — Division of responsibilities between the receiver link and the
  controller. Current lean: the link owns everything the receiver reports (a live mirror of
  device state, including the ping); the controller owns only intent.
- **20** · ⬜ · `refactor` — Put the link behind an interface, with the real implementation
  and a fake.
- **21** · ⬜ · `test` — Receiver simulator: a local TCP server that speaks the protocol,
  based on recorded real traffic, with fault injection (dropped replies, late events,
  disconnects, garbage). When the real receiver disagrees with it, fix the simulator first.
- **22** · ⬜ · `test` — Controller tests against the fake and the simulator: state
  transitions, retry, escalation to resync.
- **23** · ⬜ · `refactor` — Move the test scaffolding out of `ControlLoop`.
- **4** · ⬜ · `feature` — Resync as its own control mode. Entered from Rest it settles all
  three states itself on exit; entered from Request it returns to Request. Audit every
  comparison against the control mode when adding it. Today only startup resyncs from Rest
  (discard keys pressed meanwhile, then settle); this becomes necessary once reconnect
  (item 30) or periodic resyncs exist.
- **6** · ⬜ · `refactor` — Remove the Rest→Report workaround in `ControlPing` (no longer
  needed since item 3) and the Request-setting in `SyncOut` (still relied on by the test
  scaffolding, see item 23).

## Stage 4 — Threading and robustness

- **25** · ⬜ · `decision` — Thread layout. Leading candidate: one thread for controller and
  link, the hook thread as today, and `main` free (likely the display thread once item 34
  exists).
- **26** · ⬜ · `feature` — Control loop on its own thread: ordered shutdown, and a stop
  mechanism (a quit message, or an atomic flag plus the wake message).
- **27** · ⬜ · `feature` — A defined way to hand state across threads. Strings in the shared
  state are the case that can actually corrupt memory.
- **28** · ⬜ · `feature` — Give up after repeated failed syncs instead of resyncing forever:
  its own counter, and it returns the loop to Rest.
- **29** · ⬜ · `feature` — Cap waits at a poll interval in degraded mode (without the socket
  event, each wait currently runs out its full deadline).
- **30** · ⬜ · `feature` — Reconnect after losing the receiver, querying the volume before
  sending anything.
- **31** · ⬜ · `refactor` — Connection health as one evidence-driven state
  (healthy / degraded / dead) instead of separate flags.
- **32** · ⬜ · `feature` — Busy-wait guard: several consecutive near-zero wakes force a
  sleep and log loudly.
- **33** · ⬜ · `feature` — Track which receiver values have actually been reported, so a
  default can't pass for a reading.
- **41** · ⬜ · `bug` — A keyup is routed by the modifiers held *when it happens*, not by
  its keydown: release the modifier before the key and the press ends on a different route.
  Windows can get unpaired events, and `isdown` flags can stick (e.g. `vup.ctrl.isdown`
  stays true). Fix: fix the route at the start of each press and send repeats and the keyup
  the same way (hook-thread-only state, no atomics). **Must land before item 35**, which
  will read `isdown`.
- **42** · ⬜ · `feature` — Swallow volume keys only while a listener is registered, so a
  control loop that exits (or gives up its listener on purpose) hands the keys straight back
  to Windows — fail-safe by construction, even with the hook still running. Counting and
  signalling can't be ordered race-free, so a failed wake may act twice once (Windows now,
  receiver later); remember the failure so later keys, including held-key repeats, go
  straight to Windows.
- **43** · ⬜ · `decision` — Design `main` properly: startup order, supervising and
  restarting the control loop after a failure while the hook keeps running, and shutdown.
  Includes which failures should end the loop at all (e.g. an excessive key-processing
  delay currently does). Ties in with items 25, 26 and 30.

## Stage 5 — Further features

- **34** · ⬜ · `feature` — On-screen volume display over windowed and borderless-fullscreen
  apps.
- **35** · ⬜ · `feature` — Hold acceleration: a middleware thread between hook and
  controller that turns held keys into larger steps. Also decides what holding mute does
  (today every auto-repeat counts as a separate press).
- **36** · ⬜ · `feature` — Audio-mode monitor: compare what the receiver is decoding with
  what Windows is sending, and localise mismatches. Starts with a baseline capture of the
  Windows audio state.
- **40** · 🚧 · `investigate` — Decode the extra lines the receiver sends after our queries
  and as unsolicited events (samples in `DenonProtocol.h`; none appear in the command tables
  we have). Promising for goal 1B: `SYSDA` (apparently the incoming audio format),
  `OPINFINS` (apparently which input channels are present), `OPINFASP` (apparently which
  speakers are active), `SSINFAISSIG` (an input-signal code). Decode by controlled
  experiment: change the source (stereo, 7.1 LPCM, bitstream) and see which lines change.
  Feeds item 36; the recorded samples also seed the simulator (item 21).

## Unscheduled

- **10** · ⬜ · `test` — Performance measurement as its own topic: count control-loop
  wakes, read per-thread CPU time (`GetThreadTimes` / `QueryThreadCycleTime` are running
  totals, so no sampling gaps), and use Process Explorer's per-thread context-switch and
  cycle columns. Measure release builds at low verbosity — at debug verbosity the terminal
  drawing our log output dominates. (Moved out of Stage 1; test builds read ~2.5 % CPU in
  Task Manager, which is good enough for now.)
- **37** · ⬜ · `feature` — Staged keyboard-hook startup, each stage proven: thread running →
  message loop reachable → a keystroke reached the hook.
- **38** · ⬜ · `test` — More per-request latency samples from the receiver (first sample:
  37 ms).
- **39** · ⬜ · `bug` — The limit beep fires on every clamp: twice for Alt + volume (once per
  surround channel), and on every auto-repeat while a key is held at a limit, which turns
  into a trill. Beep once when a press first reaches the limit.
- **44** · ⬜ · `bug` — Special values in the receiver's replies are parsed as ordinary
  levels: `CVSW 00` means subwoofer off (currently read as −50 dB, outside the channel
  range), and `MV00` means volume off (the receiver displays `---`, not −80 dB).

---

## Done so far

- TCP connection to the receiver with an event-driven wait: wakes on socket data, thread
  messages or a timeout, with a degraded fallback when the socket event can't be trusted.
- Request / report / rest state machine with retry and escalation to a full resync; resync
  covers power, input, surround mode, mute, volume and channel volumes.
- `Timer` and `Stopwatch` helpers; waits derived from deadlines.
- Keyboard hook on its own thread: modifier tracking, key combos mapped to counters, keys
  swallowed, health probes.
- Mute tracking end to end.
- Volume keys control the receiver end to end: main volume, centre (Ctrl) and surround
  (Alt) levels, mute, clamped to the receiver's limits; Shift passes keys to Windows.
