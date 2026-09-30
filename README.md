# AVManager

This project is designed to make using an AV receiver as the main audio output for Windows as seamless and painless as it can be.

I use a PC for gaming, home theater, and streaming. I bought a Denon AVR (AVR/AVC-X3800H) and noticed two quirks on day 1: A) controlling the volume with the Denon remote is a lot less convenient than my old habit of using the keyboard volume keys to control Windows volume[^1], and B) Windows would drop from 7.1 down to stereo unannounced and my home theater and games would suffer. Other things I noticed soon after: I end up frequently controlling center channel for dialogue, which is more buttons, and I am often straining to read what the display says (e.g. what channels, what center channel volume, etc).

All of that will be addressed by this program:
### Goal 1A: Keyboard control and on-screen display
- Controlling the Denon from my Windows PC, sending commands over TCP/IP according to the Denon spec[^2].
- Intercepting volume up, down and mute keys in Windows and sending those to the Denon instead.
- Using modifiers (currently: ctrl = center channel, alt = surrounds, shift = passthrough to Windows).
- Linking the two through a state machine tracking the Denon's state to prevent double-sends, missed responses, sending before the Denon is ready, etc.
- Displaying volume changes on a custom OSD, including confidence levels (e.g. font color is grey for sent but unconfirmed, white for confirmed, red for not responding) and expandable to goal 1B.
### Goal 1B: Audio format syncing and auto-switching
- Checking the audio pipeline in Windows for what format it is outputting.
- Checking the audio format the Denon is receiving (need not be the same, there is an LG C3 between them).
- Restarting or reconfiguring whatever Windows audio components needed to get back to 7.1.
- Future sub-goal: detect when Atmos-enabled content starts playing, launch Dolby Access and confirm the entire chain hooks onto Atmos, drop back to 7.1 LPCM afterwards[^3].

A secondary goal in parallel to those above is for me to expand my knowledge of C++, Windows programming, Git, CMake, and any other tools that are relevant. So far I haven't added any tools inside the codebase just for the sake of it, but I will soon start playing with a basic CI/CD flow to learn the basics.

[^1]: The Denon needs line of sight and my plants keep shifting to hide the sensor. For the best audio quality, you want Windows at 100% volume and rely entirely on the Denon to control the actual sound level.
[^2]: I haven't found a version of the protocol for the X3800H formally published by Denon, I used the one I found here: [avsforum.com](https://www.avsforum.com/threads/denon-ip-and-rs232-commands-dennon-x3700h-x3800h-2021-2022.3290782/)
[^3]: Before I bought the AVR, I needed Dolby Access even to get 5.1 over HDMI, and from that time I know that the link between the PC and the LG and from the LG to the decoder is extremely unreliable, audio drop-outs, buzzing, stuck in stereo, etc. That is why I chose to only use Dolby Access when it is needed. Low priority since I don't have height speakers yet.

## Status

The repo includes a dedicated [roadmap](ROADMAP.md) with more detail, but the short summary:

| 1A: Keyboard control and on-screen display | status |
|---|---|
| Talking to the Denon | ✅ working |
| Intercepting keys | ✅ working |
| Using Modifiers | ✅ working |
| Linking state machine | 🚧 WIP |
| OSD | ⬜ not yet started |

| 1B: Audio format syncing and auto-switching | status |
|---|---|
| Checking Windows pipeline | ⬜ early exploratory work done outside this codebase |
| Checking Denon audio format | 🚧 looking promising, not yet formalised |
| Fixing a desync | ⬜ not yet started |
| Auto-switching Atmos | ⬜ not yet started |

| Currently working on |
|---|
| cleaning up control logic |
| splitting out test scaffolding into a basic CI flow |
| splitting class structure to smaller classes |
| (automated) test infrastructure |

## How it works

- `KeyboardHook` (keyboard.h/cpp) registers a low level hook in a dedicated thread, which runs a blocking message loop such that Windows runs its callback (`ProcessKeys`) on any keystroke. The callback reads the keys and usually passes them straight through to Windows, unless they are key-downs or key-ups for volume up, down and mute. 
  - ***Returning immediately is critical for gaming input latency***, returning fast is critical to not be annoying in general, and Windows might even drop our hook after 300 ms delay in handling the keystroke. (not observed in testing)
- volume up, down and mute are stored in the `KeyStates` struct atomically (in separate members for the different modifiers), and a thread message is sent (if a listener's thread ID has been registered). 
- Modifiers (ctrl, alt, shift) are stored separately in order to direct keypresses to the right member of the struct. This is essential for ctrl+alt edge cases. With shift as modifier the volume keys *are* passed to Windows.
- `AVRHandler` (.h/.cpp) creates a winsock2 socket, opens the connection to the Denon and manages sending, receiving and any errors and edge cases arising from it.
- DenonProtocol.h contains the relevant commands and formats of expected responses from the Denon
- `AVRHandler` also contains the control loop (to be split out soon) that manages the 3 states:
  - `requested_`, the state we ***want*** the Denon to be in (i.e. whatever keys have been pressed and processed)
  - `commanded_`, the state we ***told*** the Denon to be in (i.e. whatever commands have actually been sent to the Denon without error)
  - `reported_`, the state we ***believe*** the Denon is currently in (i.e. whatever the Denon has sent to us(in response or spontaneously) over the socket)
- `ControlLoop()` itself is under construction, but in short it:
  - pulls in any keystrokes that have come in recently
  - pulls in any messages from the Denon
  - sends commands if `requested_` and `commanded_` don't match
  - resends commands if `requested_` and `reported_` don't match within a deadline
  - resyncs all parameters from the Denon if it suspects we have missed messages (commands keep failing)
  - goes to sleep until:
    - `command_cooldown_` is over (i.e. we can send new commands)
    - when `response_deadline_` is over (i.e. we can check if we're synced up) 
    - after a heartbeat period (currently 1 s, tbd)
    - the socket reports an incoming message from the Denon
    - the `KeyboardHook` sends a thread message (or anyone else does)

More to follow!

## Building
Coming soon! 

*the current main() runs through test scaffolding and includes a lot of logging, so in its current state it is not yet ready to be run as a background process as intended. I will work on this soon and then provide build instructions (see [roadmap](ROADMAP.md)).*

## How this was written

I'm learning C++ and Windows programming through this project, and all code here is written by me. I use Claude (via Claude Code) as a **tutor**: 

- to discuss design decisions ("This feels a bit Pythonic, how would a seasoned C++ developer approach it?"), 
- to check assumptions ("Can we rely on this Windows API returning X even if Y?"), 
- to learn under-the-hood fundamentals ("Will using a local here cost me more cycles than having a persistent class member?"), 
- and to review my changes before I commit them. 

I wrote this Readme and most commit messages and had Claude review them and make suggestions. For some commit messages and the roadmap I let Claude draft it first based on the ongoing discussion, and the Roadmap is almost entirely maintained by Claude.

## License

MIT - see [LICENSE](LICENSE)