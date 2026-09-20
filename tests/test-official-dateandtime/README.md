# Official DateAndTime port

Upstream reference: `ConsoleSoftware/TestPrograms/Test-DateAndTime/`.

This is a project-owned ordinary-C port. It includes the clock texture and
tic-tac WAV, so it can be built without the reference tree. It keeps the
original setup and frame loop: the current timer time and date determine the
clock/calendar digits, the seconds colon is shown during the second half of
each 60-frame interval, and channel 0 or 1 plays on each new second.

The source replaces the official compiler's `time_info`/`date_info` helpers
with direct decoding of the documented packed timer values, and uses the
public `vircon.h` C helpers instead of the official inline-assembly headers.
The texture region for the blinking colon is deliberately the full
`(211,315)..(216,346)` glyph. This preserves the original full-size colon;
using the nearby one-pixel table strip produces a visibly tiny dot instead.

The automated test verifies the complete C-to-ROM resource pipeline and the
critical generated hardware operations. Desktop-emulator verification remains
useful for checking the live system clock/date, the one-second colon blink,
and alternating tic/tac audio.
