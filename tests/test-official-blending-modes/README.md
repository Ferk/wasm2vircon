# Official BlendingModes port

Upstream reference: `ConsoleSoftware/TestPrograms/Test-BlendingModes/`.

This directory owns the application source and the two cartridge assets; it
does not consume the reference tree at build time. The port replaces the
official compiler's inline-assembly headers with the project runtime API and
uses normal ISO C declarations/types. The scene geometry, blend-state sequence,
lamp loop sound, animation period, random flicker and frame loop follow the
upstream source.

Blending selection and sound-loop configuration are ordinary runtime wrappers
over narrow platform imports; they are not application imports or compiler
intrinsics.
