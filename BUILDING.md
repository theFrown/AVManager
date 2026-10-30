# Building and using AVManager

## Requirements
| package | minimum version | tested version|
|---|---|---|
| CMake | 3.25+ | 4.3.3|
| Ninja | unknown | 1.12.1|
| Windows x64 | 10 | 11 Pro 26200 |
| Visual Studio 2022 Build Tools (or Visual Studio 2022) with the "Desktop development with C++" workload. | unknown | MSVC toolset 14.44, Windows SDK 10.0.26100 |
|(optional) Git | unknown | 2.54.0.windows.1 |

You can probably build with any other compiler that compiles C++20, but that has not been tested.

## Getting the source
Clone the repo with the following command:
```
git clone https://github.com/theFrown/AVManager
```

## Building
### From the command line

1. Open an x64 Native Tools prompt (easiest way is to open start menu and search for
`x64 Native Tools Command Prompt for VS 2022` or if you know where to find it run Visual Studio's
`vcvars64.bat`) and navigate to the cloned repo.
2. configure the CMake preset:
```
cmake --preset windows-release
```
3. build:
```
cmake --build --preset windows-release
```
this places the build files in `build\windows-release\` including the executable you want in
`build\windows-release\AVManager.exe`. Place this file anywhere you like, but ideally in a location
where the main Windows user profile has write permissions.

***Alternatively***, use the debug build version for more flexible debugging:
```
cmake --preset windows-debug
cmake --build --preset windows-debug
```
which will place the files in the associated folder.

### With VSCode

1. Install all the dependencies ***plus*** the latest version of the C/C++ extension and CMake Tools
 extension
2. Select one of 2 *configure presets*: (either hit the preset button in the status bar or
`Ctrl + Shift + P` > `CMake: Select Configure Preset`)
   - Release[windows-release] - runs with full optimization
   - Debug[windows-debug] - skips certain optimizations for debugging purposes
3. Start the build (either hit the `Build` button in the status bar or `Ctrl + Shift + P` > `CMake: Build`)

## Configuration
### In the AVR settings
- set `Network Control` to `On`
- set `DHCP` to `On`

### In your Wifi/LAN router's settings
- assign a static IP for the AVR (often called *DHCP reservation*)

*Note: alternatively, it should also work to disable DHCP on the AVR and set a static IP address
there (rather than in the router), but this is neither tested nor recommended. If you want to do
this, be sure to set it to a value outside the router's DHCP range to avoid conflicts.*

### Command line arguments
- IPv4 address of the AVR in your network added through input argument, either:
  - run with command line argument with format `x.x.x.x`, e.g. `192.168.1.2`
  - make a shortcut and change the Target: `...\AVManager.exe xxx.xxx.xxx.xxx`

  without an argument it runs the default IP address `192.168.1.200`

### Optional but useful modifications to the source code
- global verbosity level: in `main.cpp` change the constructor argument of `AVRHandler` to the desired
level (e.g. `AVRHandler avr{..., Verbosity::Info};`)
  - options in order of verbosity: `Silent`, `Error`, `Warning`, `Info`, `Debug`, `Trace`
- log file location `portable` yes/no: in `main.cpp` change the argument for `log.Start(...);` to
true/false respectively.
  - `true`: logs are stored in a `logs` folder relative to the location of `AVManager.exe`
  - `false`: logs are stored in `%LOCALAPPDATA%`, i.e. `C:\Users\<username>\AppData\Local\AVManager\logs`
- the increment of one button press: `volume_step_` in `avr_handler.h` should be set to match the
setting in the AVR. I've got it set to 5, because my AVR is set to 0.5 dB increments.

## Running
Run `AVManager.exe` from the command line, from explorer directly or through a shortcut.

### What it does
- It creates a log file for each run and opens a console to print its outputs to.
- It will absorb any keypresses to the following volume media keys on most keyboards[^1]
, meaning these keys will not seem to work anymore:
  - volume up
  - volume down
  - mute
- AVManager will instead send commands to the AVR to modify the volume *there* while leaving
the Windows volume untouched
- The volume keys will have different effects on different speaker groups depending on which
modifiers are used:
  - `without modifier`: **Main Volume**, will control the AVR the exact same way as when you press
  the associated button on the AVR's remote.
    - `Volume Up` and `Volume Down`: each press increases/decreases the main volume by either
    0.5 dB or 1 dB depending on AVR settings and `volume_step_`. *Note: if the code hangs and finds many volume up
    presses it will cap the number to 10 presses to protect eardrums and speakers.*
    - `Mute`: will either fully mute the main volume or toggle -40 dB depending on how you set it
    up on the AVR. *Note: if the code hangs and finds multiple presses when it resumes, it will
    assume "Mute On" was intended.*
  - `Ctrl`: **Center Channel**, will set the relative volume of the center channel as an offset
  to the main volume, use to calibrate dialogue.
    - `Volume Up` and `Volume Down`: will increase/decrease the speaker level of the center channel
    relative to the main volume.
    - `Mute`: will reset the center channel offset back to 0 relative to the main volume. Repeat
    presses are ignored.
  - `Alt`: **Surround Channels**, will set the relative volume of both the Surround Left
  and Surround Right channels as an offset to the main volume. Use to calibrate surround effects.
  (does not yet control sides or height channels, see Known Issues).
    - `Volume Up` and `Volume Down`: will increase/decrease the speaker level of the surround
    channels relative to the main volume.
    - `mute`: will reset the surround channel offset back to 0 relative to the main volume. Repeat
    presses are ignored.
  - `Shift`: **Windows Volume**, will pass the keys through to Windows unchanged
    - `Volume Up` and `Volume Down`: will increase/decrease the Windows volume (keep this at 100%
    for best audio quality!).
    - `Mute`: will mute/unmute Windows.
  - `Ctrl`\+`Alt`: **Special Commands**
    - `Mute`: **Audio Cue**, has no effect on volume but plays an audible \**ding*\* as an easy way
    to check/signal that the keyhook is working well without opening consoles or logs.
    - `End`: **Exit Program**, gracefully closes all logs and connections and closes.
  - `any other combination of modifiers`: **Main Volume**, conflicting modifiers are ignored and
    sent to the AVR main volume instead.

[^1]: Some fancier keyboards have custom key handling for non-standard keys through their drivers
before Windows sees them. If that is the case, the key might bypass my keyboard hook entirely.
I am currently not sure if this can be helped and would probably recommend mapping other keys
that this keyboard has to the same functionalities.

### To close the program
The preferred method is `Ctrl`+`Alt`+`End`, this asks the program to close nicely and wrap
up logs etc neatly.

It is technically possible to just close the console window, but this leaves
clean-up up to Windows **and leaves logs looking like a crash**. The latter is relevant because
I will add checks in the future for crashes based on the fact that the previous logs lack their
wrap-up lines.

## Known issues
- Surround currently only affects 5.1 channels, so 7.1, 5.1.4, etc will only have the Surround Left
and Surround Right respond to `Alt` + `volume up/down`. Fixing this without unnecessary network
traffic requires knowing what speaker channels are hooked up and used, which is related to
[roadmap item 40](ROADMAP.md) and is not scheduled any time soon unless you ask me to bump it up.
- Log files are not capped in either length or number. If it runs for weeks, it will become long,
if you run it many times, you will have equally many logs. Fixing it is
[roadmap item 52](ROADMAP.md), targeted for short term.
- Will often shut itself down after a Windows sleep-wake cycle. Being investigated as
[roadmap item 60](ROADMAP.md).
- May complain and resync when the AVR power-cycles. This happens because the AVR reports a
***bunch*** of messages when it power-cycles and if you press a volume key in that ~10 second
window AVManager might interpret the stream of messages as a sign the AVR is not listening to its
commands. It should be harmless in effect, just some unnecessary network traffic and logging. To
avoid it, whenever you reboot the AVR, give it some time to stop sending out messages before
sending commands if you want to be sure. At least wait until the AVR power LED stops blinking
and the display shows the correct audio mode. Investigated in [roadmap item 61](ROADMAP.md).
- "Feature X is not in yet": see the [roadmap](ROADMAP.md) for features that should work now, what to
expect near-term and what to expect long-term.
- No modifier for subwoofer(s). I have not felt the need to control mine despite daily use of the
AVR for varied media sources, so I'm not planning on adding it :).


## Troubleshooting
In general the program is fairly verbose when it comes to errors and warnings. Pretty much any
function that can fail will report why it fails as either an [ERROR] or a [WARNING] message.
Always read through the logs to get more information on what might be wrong.

In particular, check whether or not the log ends with `===== end of log =====` on its own line:
- Any Log with `===== end of log =====` at the end indicates a ***graceful close***.
- Any log without it indicates a ***crash or forceful close*** (closing the console, shutting
down Windows, etc). Instead the last line will be a standard `timestamp - message` combo that may
or may not relate to why it closed.

| problem | likely cause | solution |
|---|---|---|
| shuts down immediately (log includes `===== end of log =====`) | probably a bad IP address (wrong subnet, wrong format, other device) or AVR/firewall/router not setup correctly | find reported errors in the log, but in general: make sure the AVR is on the same LAN/Wifi network, is setup for Network Control, the firewall is not blocking AVManager from talking to it and if possible assign the AVR a static IP address in your router |
| shuts down after ~20 seconds (log includes `end of log`) | probably connection timed out, can be due to valid but unreachable IP address, Network Control being off or firewall dropping the in- or outbound message | check that Network Control is on, check the AVR IP address and if possible set a static IP address, let AVManager through the firewall.
| shuts down after a long time working (log includes `end of log`) | probably something changed in the network and AVR became unreachable | check AVR and Router are on and if possible assign the AVR a static IP address |
| crashes (log file ends with a `timestamp + message`) | only known case is if you shut it down by closing the console/debugger | I'd love to see the log, so feel free to send them to me through GitHub! If possible set the program to Verbosity::Debug or Verbosity::Trace first and reproduce so I have something to work with |
| AVR does not respond | probably the AVR is in standby mode, so keypresses are swallowed | wake up the AVR |
| no log or `Logger failed to start..` | probably trying to use it in `portable` mode in a folder without write permissions | Open a terminal and run from CLI and look for the accompanying error messages `Logger failed to..` for more details. In general: either use AVManager in `non-portable` mode or make sure `AVManager.exe` is in a folder where your user has write access. *Maybe* launching with admin permissions works, but not tested. |
| CMake can't find Ninja or the compiler, or gives error `fatal error C1034: ...: no include path set` and doesn't build | trying to build from a plain command prompt/powershell terminal | use an x64 Native Tools prompt (i.e. the shortcut or after running `vcvars64.bat`, see build instructions above)|
