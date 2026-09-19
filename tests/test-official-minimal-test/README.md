# Official MinimalTest port

Upstream reference: `ConsoleSoftware/TestPrograms/Test-MinimalTest/`.

This is a project-owned normal-C port, not a build dependency on that tree.
The source changes are limited to using `int main(void)`, the project runtime
header, and byte-addressed normal C. The original `define_region_center(0, 0,
139, 99)` is retained; its inline runtime helper evaluates the same hotspot
`(69, 49)`. The copied `assets/` files are the upstream PNG and WAV used by
the reference project's ROM definition.
