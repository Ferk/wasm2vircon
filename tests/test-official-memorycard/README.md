# Official MemoryCard port

Upstream reference: `ConsoleSoftware/TestPrograms/Test-MemoryCard/`.

This normal-C port owns its texture and requires no reference-tree input at
build time. It preserves the game scene, save/load dialogs, card signature,
and 20-word signature plus 5-word scene record layout. The public C card API
uses `int` words deliberately: Wasm pointers remain byte-addressed, while card
offsets/counts are explicit Vircon card-word units.

Manual verification requires the desktop emulator to attach or automatically
create a `.memc` card. Check save, reload, empty-card, no-card, and
wrong-signature dialogs separately; the normal integration test verifies ROM
packaging and assembler acceptance only.
