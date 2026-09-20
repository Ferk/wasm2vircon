# Official Rotozoom port

Upstream reference: `ConsoleSoftware/TestPrograms/Test-Rotozoom/`.

This project-owned ordinary-C port retains the two texture regions, gamepad
controls, clamping ranges, clear/background draw, wheel multiply colour,
scale, angle, hotspot, and rotozoom draw from the official test. It packages
the two copied PNG assets as texture IDs 0 and 1.

The original uses the official custom-C `itoa`, `ftoa`, `strcpy`, and `strcat`
helpers to compose status strings. The port instead uses the project runtime's
ordinary-C `print_int_at` and intentionally small `print_fixed_2_at` helpers.
The latter prints the bounded zoom value to exactly two decimal places; unlike
the original's `ftoa`, it does not print up to five significant decimal digits.
This keeps formatting outside the compiler and avoids adding unneeded general
string/format APIs; it does not affect the graphical transform or controls.
