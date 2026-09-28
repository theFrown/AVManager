# AVManager — Roadmap

Where the project is going and why. Working document: it is updated as decisions are made.

**How to read it**

- Every item has a **permanent number**. Numbers are never reused or renumbered, even when
  an item is finished, dropped or moved to another stage. New items take the next free
  number, whichever stage they land in. **Next free number: 40.**
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

**How this is built**

I'm learning C++ and Windows programming through this project, and all code here is written
by me. I use Claude (via Claude Code) as a tutor: to discuss design decisions ("This feels
a bit Pythonic, how would a seasoned C++ developer approach it?"), to check assumptions
("Can we rely on this Windows API returning X even if Y?"), to learn under-the-hood
fundamentals ("Will using a local here cost me more cycles than having a persistent class
member?"), and to review my changes before I commit them. The roadmap, commit messages and
some of the other project documents are co-written with Claude.

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

**Current focus: Stage 1.**

**Open strategic questions:** items 11, 18, 19, 25.

---

## Stage 1 — Keyboard volume control, end to end

The first milestone that makes the program useful: press a volume key, the receiver moves.
The event-driven control loop is rebuilt only as far as this needs.

- **1** · 🚧 · `feature` — Sleep-driven control loop: receive without waiting, then sleep
  until the nearest pending deadline or a 1 s heartbeat, instead of re-checking every 15 ms.
  Written; blocked on item 3.
- **2** · 🚧 · `feature` — Key handling in the controller: read the hook's key counters,
  turn them into requested volume / mute / channel-level changes, clamp to limits.
  Written; not yet tested end to end (needs items 7 and 8).
- **3** · ⬜ · `bug` — A stale response deadline makes the idle loop spin after a ping or
  resync. The deadline should be set by whoever starts a request (`SyncOut`), not by the
  transport (`Send`).
- **4** · ⬜ · `feature` — Resync as its own control mode. Entered from Rest it settles all
  three states itself on exit; entered from Request it returns to Request. Audit every
  comparison against the control mode when adding it.
- **5** · ⬜ · `bug` — `SyncResolve` must not judge a request that hasn't been sent yet. The
  condition is "unsent work exists", not a timer state (treating an unset deadline as
  "wait" creates a stuck state).
- **6** · ⬜ · `refactor` — Once 3–5 land: remove Request-setting from `SyncOut` and the
  Rest→Report workaround in `ControlPing`.
- **7** · 🚧 · `feature` — Keyboard wake: the hook callback posts a content-free "look
  again" message to the control thread. Clear the registered thread ID when the loop exits.
- **8** · ⬜ · `feature` — `main` runs the keyboard hook and the control loop together.
- **9** · ⬜ · `bug` — `SyncOut` has no channel-volume branch, so Ctrl or Alt + volume ends
  the control loop.
- **10** · ⬜ · `test` — Measure idle CPU before and after the sleep-driven loop.
- **11** · ⬜ · `decision` — The Ctrl+Alt+End quit shortcut in the hook callback: keep, remap
  or remove. (With the hook on its own thread it stops the hook, not the program.)

## Stage 2 — Public release and CI

- **12** · ⬜ · `docs` — README: purpose, status, how to build, architecture overview, link
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
- **18** · ⬜ · `decision` — When to make the repository public.

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
- **24** · 🚧 · `refactor` — Cleanups: `explicit` single-argument constructors; `Timer` and
  `Stopwatch` in their own headers; consistent member naming; how a failed construction is
  reported (factory function versus a status member).

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

## Stage 5 — Further features

- **34** · ⬜ · `feature` — On-screen volume display over windowed and borderless-fullscreen
  apps.
- **35** · ⬜ · `feature` — Hold acceleration: a middleware thread between hook and
  controller that turns held keys into larger steps.
- **36** · ⬜ · `feature` — Audio-mode monitor: compare what the receiver is decoding with
  what Windows is sending, and localise mismatches. Starts with a baseline capture of the
  Windows audio state.

## Unscheduled

- **37** · ⬜ · `feature` — Staged keyboard-hook startup, each stage proven: thread running →
  message loop reachable → a keystroke reached the hook.
- **38** · ⬜ · `test` — More per-request latency samples from the receiver (first sample:
  37 ms).
- **39** · ⬜ · `bug` — Alt + volume at a limit beeps twice (once per surround channel).

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
