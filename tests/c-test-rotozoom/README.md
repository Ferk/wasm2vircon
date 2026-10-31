# Official Rotozoom port

Upstream reference: `ConsoleSoftware/TestPrograms/Test-Rotozoom/`.

This project-owned ordinary-C port retains the two texture regions, gamepad
controls, clamping ranges, clear/background draw, wheel multiply colour,
scale, angle, hotspot, and rotozoom draw from the official test. It packages
the two copied PNG assets as texture IDs 0 and 1.

The original uses the official custom-C `itoa`, `ftoa`, `strcpy`, and `strcat`
helpers to compose status strings. The port uses ordinary-C `print_int_at` for
the angle and `ftoa` followed by `print_at` for the zoom value. Like the
original, zoom uses up to five fractional digits and removes trailing zeroes.
