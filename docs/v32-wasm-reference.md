# V32Wasm reference

`wasm2vircon` accepts a deliberately small WebAssembly input profile named
**V32Wasm**. It is intended as a compiler input format for programs built
for Vircon32, not as a general WebAssembly runtime or a browser-compatible
Wasm implementation.

This document explains that profile without assuming prior WebAssembly
experience. If an instruction or module feature is not listed as supported
below, treat it as unsupported: the compiler should reject it with a diagnostic
rather than silently changing its meaning.

## The short version

A typical C build has this shape:

```text
C source
  -> Clang wasm32 object files
  -> wasm-ld linked Wasm
  -> wasm2vircon
  -> Vircon32 assembly
  -> assemble and packrom
  -> .v32 ROM
```

For a normal C project, start from `example/CMakeLists.txt` instead of
manually reconstructing the individual commands. It writes linked Wasm,
assembly, program binary, ROM XML, and ROM into its build directory without
requiring `wasm-opt`.

Applications should include `vircon.h` and use its public C API. The private
Wasm imports described by that header are compiler/platform implementation
details, not a general-purpose import mechanism.

## WebAssembly concepts used here

### Modules, functions, imports, and exports

A `.wasm` file is a **module**. It can contain functions, static data, a
linear-memory declaration, imports, and exports.

- An **import** is a function supplied by the target platform. For example,
  the public C `clear_screen` helper ultimately calls a private import that
  lowers to Vircon GPU operations.
- A **defined function** has a Wasm body. Normal C functions compile this way;
  they are not interpreted at runtime.
- An **export** makes an internal function discoverable by name. The default
  application entry export is `main`, matching the official Vircon C entry
  convention.

VirconWasm translates direct defined calls and a fixed allowlist of `env.*`
platform imports. An arbitrary unresolved import is rejected; it is never
assumed to be a Vircon32 symbol.

### Inspecting compatibility

`wasm2vircon --validate-only input.wasm --entry main` runs normal Binaryen
loading and VirconWasm validation without lowering or writing assembly. It is
the quickest answer to whether the configured entry and all code reachable from
it fit the current profile.

`wasm2vircon --report-profile input.wasm` instead prints a Binaryen-decoded
module inventory followed by complete printed Wasm text. It succeeds for a
standard-valid module even when VirconWasm does not support one of its
instructions. Use it to identify all generated constructs before deciding on a
new frontend feature; it is intentionally observational and does not replace
validation.

wasm2vircon always runs the narrow in-process Binaryen
`remove-unused-module-elements` and `vacuum` passes before this frontend
decodes linked linker scaffolding. Hand-authored modules without that
scaffolding retain direct-input validation. The CMake
`WASM2VIRCON_EMBEDDED_BINARYEN` setting controls static versus normal Binaryen
linkage only; it does not toggle this required cleanup.

`wasm2vircon --skip-input-optimization` is an explicit diagnostic/test mode.
It bypasses only the two Binaryen passes for that invocation; decoding and all
compiler-owned processing remain active.

### Compiler-owned raw-linker legalization

After decoding, wasm2vircon always performs a separate conservative cleanup of
proven-unobservable linker artifacts. It can discard globals only when no
decoded function reads or writes any global and the module does not export one.
It can discard declared tables and element segments only when the module does
not export a table, because the frontend rejects table operations and indirect
calls before this pass. It also legalizes an `i32`-typed loop only if the final
direct fallthrough expression is an unconditional branch to that same loop,
which proves the loop cannot normally yield a value.

This is not a general optimizer: it does not remove unused functions, preserve
arbitrary globals, implement tables, or accept ordinary value-producing loops.
It runs whether or not optional input optimization was requested.

### Stack-machine expressions

Wasm instructions are typed expressions. In text form, they are often written
as nested S-expressions:

```wasm
(i32.add (local.get $score) (i32.const 1))
```

This means “read the local variable `score`, make an integer constant `1`, then add them together.”

In binary Wasm, the same work is represented as a stack program (both `local.get`
and `i32.const` push values into the stack, then `i32.add` pops the last 2 values
form the stack, adds them, and pushes the result into the stack).

`wasm2vircon` converts that temporary value flow into compiler-managed Vircon32
stack-frame slots and register loads; applications do not need to manage those
slots.

### Integer and floating-point values

`i32` is a 32-bit integer bit pattern. The same bits may be used as signed or
unsigned according to the instruction: `i32.div_s` is signed division, while
`i32.div_u` is unsigned division. The supported `i64` subset is represented
inside the compiler as a little-endian pair of i32 words; it is not passed in
Vircon32 registers or exposed as a native target pointer.

`f32` is an IEEE-754 binary32 floating-point value. The current profile is
practical and finite-input oriented. It does not promise complete portable
NaN, infinity, signed-zero, or every edge-case behavior expected from hosted
libm implementations.

## Required module shape

The current profile accepts one ordinary wasm32 linear memory and one entry
export:

- exactly one defined, non-shared, non-`memory64` memory;
- an initial memory size greater than zero, expressed in 64 KiB Wasm pages;
- no imported memory; `memory.size` and default-memory `memory.grow` are
  supported within the target's fixed physical linear-memory region;
- entry export `main` by default, or the name given to `--entry` (with entry
  signature `() -> ()` or `() -> i32`)
- defined functions with any practical number of `i32`/`f32` parameters and
  an `i32`, `f32`, or void result. `i64` is currently supported for locals and
  intermediate expressions only, not parameters or function results;
- active data segments with a constant `i32` offset in the default memory.

The only global exception is automatic linker stack support. The compiler
accepts exactly one mutable `i32` global named `__stack_pointer`, with a
constant, four-byte-aligned initial byte offset inside the declared Wasm
memory. It exists for frontends that need automatic aggregates. It does not
enable general globals or arbitrary global access.

## Memory model

Wasm pointers are **byte offsets**, even though Vircon32 RAM is naturally
word-addressed. wasm2vircon keeps this difference at the frontend boundary.

For a Wasm pointer `p`:

```text
Vircon RAM word address = 1,000,000 + (p >> 2)
byte lane               = p & 3
```

Bytes within a target word are packed little-endian. Therefore a C `char *`
remains byte-addressed: `p + 1` advances by one byte, not one Vircon word.

The linear-memory region starts at Vircon RAM word address `1,000,000`
(`0x000F4240`). It has 8,000,000 bytes available before the target's reserved
upper RAM area. Since Wasm memory sizes are whole 64 KiB pages, the maximum
initial or grown size is **122 pages** (7,995,392 bytes). A module may declare
a larger Wasm maximum, but a growth beyond this physical target limit returns
Wasm's normal `0xFFFFFFFF` failure result. A smaller declared maximum remains
an additional enforced limit.

At cartridge startup, active data segments are packed into little-endian target
words and copied into writable RAM. This preserves writable C static data and
string-literal byte layout. Every supported load/store is bounds-checked in
Wasm byte units. An invalid access transfers to the shared `__wasm_trap` path,
which halts the target; it cannot wrap into unrelated Vircon RAM.

The compiler stores the current page count at reserved Vircon RAM word
`999,998`, outside Wasm linear memory and below the separate Wasm stack-pointer
state at word `999,999`. It initializes that count before the entry function.
This state is an implementation detail, not a C or Wasm pointer.

## Supported instructions

This is an instruction-by-instruction reference for the accepted subset.
WebAssembly writes an operation as `type.operation`: `i32` means a 32-bit
integer value and `f32` means a 32-bit floating-point value. A suffix `_s`
means that an integer operand is interpreted as **signed** two's-complement;
`_u` means it is interpreted as **unsigned**.

Unless stated otherwise, an operation evaluates its input expressions,
produces the stated result, and leaves it available to the enclosing Wasm
expression. A comparison always produces an `i32`: `1` for true and `0` for
false.

### Constants, locals, and value plumbing

| Instruction | Meaning in this profile |
| --- | --- |
| `i32.const` | Produces the written 32-bit integer bit pattern, such as `i32.const 42`. |
| `i64.const` | Produces the written 64-bit integer bit pattern. The compiler holds it as adjacent low and high i32 words. |
| `f32.const` | Produces the written IEEE-754 single-precision value, such as `f32.const 1.5`. |
| `local.get` | Reads one function parameter or local variable. Its result has that local's declared supported type: `i32`, `f32`, or local-only `i64`. |
| `local.set` | Evaluates one value and writes it into a function parameter/local slot. It has no result. |
| `local.tee` | Evaluates one value, writes it into a local slot, and returns that same value. A compiler uses it to save a calculation while continuing to use it. |
| `drop` | Evaluates one supported value and discards its result. Side effects of the evaluated expression still occur. |
| `select` returning `i32` | Evaluates two `i32` alternatives and an `i32` condition. It returns the first when the condition is nonzero, otherwise the second. Both alternatives are evaluated first. |
| `select` returning `f32` | The same conditional-value operation for two `f32` alternatives. The condition is still an `i32`, and both float alternatives are evaluated first. |
| `select` returning `i64` | The same operation for pair-valued i64 alternatives. Both 64-bit alternatives are evaluated before the condition chooses one. |
| `nop` | Does nothing: it consumes no values, produces no values, and has no observable Wasm state change. It is accepted and emits no target instruction. |

Parameters, locals, and temporary values are stored in compiler-managed target
stack slots. The generated calling convention uses `R0` for an `i32` or `f32`
return value; registers are caller-clobbered. Direct calls support nesting and
recursion. Each caller reserves exactly enough outgoing stack words for its
widest defined call; platform imports continue to lower directly to hardware
operations rather than consuming a generic argument area.

### Integer operations

All `i32` arithmetic below works on 32-bit values. Addition, subtraction,
multiplication, and shifts retain only the low 32 bits of the mathematical
result. Shift counts use their low five bits, so shifting by 32 has the same
count as shifting by zero, as required by Wasm.

| Instruction | Meaning in this profile |
| --- | --- |
| `i32.add` | Adds two 32-bit integers and returns the low 32 bits of the sum. |
| `i32.sub` | Subtracts the second 32-bit integer from the first and returns the low 32 bits of the difference. |
| `i32.mul` | Multiplies two 32-bit integers and returns the low 32 bits of the product. |
| `i32.div_s` | Divides the first integer by the second after interpreting both as signed 32-bit numbers. Division by zero, and signed minimum divided by `-1`, trap as Wasm requires. |
| `i32.div_u` | Divides the first integer by the second after interpreting both as unsigned values from `0` to `2^32-1`. Division by zero traps. |
| `i32.rem_s` | Returns the signed remainder after signed division. Its sign follows the first operand; division by zero traps. |
| `i32.rem_u` | Returns the unsigned remainder after unsigned division; division by zero traps. |
| `i32.shl` | Shifts the first value left by the second value's low five bits, inserting zero bits on the right. Bits shifted past bit 31 are discarded. |
| `i32.shr_s` | Shifts the first value right as a signed number. It copies the original sign bit into newly opened high bits; for example, a negative value normally stays negative. |
| `i32.shr_u` | Shifts the first value right as an unsigned bit pattern. It always inserts zero bits into newly opened high bits. |
| `i32.rotl` | Rotates every bit left by the second value's low five bits. Bits leaving bit 31 re-enter at bit 0, so no information is discarded. |
| `i32.rotr` | Rotates every bit right by the second value's low five bits. Bits leaving bit 0 re-enter at bit 31. |
| `i32.and` | Computes a bitwise AND: each result bit is one only when the corresponding bits of both inputs are one. |
| `i32.or` | Computes a bitwise OR: each result bit is one when either corresponding input bit is one. |
| `i32.xor` | Computes a bitwise exclusive OR: each result bit is one when the corresponding input bits differ. |
| `i32.extend8_s` | Keeps the low eight bits, interprets bit 7 as a sign bit, and fills bits 8 through 31 with that sign. It converts an embedded signed byte to i32. |
| `i32.extend16_s` | Keeps the low 16 bits, interprets bit 15 as a sign bit, and fills bits 16 through 31 with that sign. It converts an embedded signed 16-bit value to i32. |
| `i32.eqz` | Returns `1` when its input is exactly zero; otherwise returns `0`. |
| `i32.eq` | Returns `1` when the two 32-bit bit patterns are equal; otherwise returns `0`. |
| `i32.ne` | Returns `1` when the two 32-bit bit patterns differ; otherwise returns `0`. |
| `i32.lt_s` | Returns `1` when the first input is less than the second as signed 32-bit integers; otherwise `0`. |
| `i32.gt_s` | Returns `1` when the first input is greater than the second as signed 32-bit integers; otherwise `0`. |
| `i32.le_s` | Returns `1` when the first input is less than or equal to the second as signed 32-bit integers; otherwise `0`. |
| `i32.ge_s` | Returns `1` when the first input is greater than or equal to the second as signed 32-bit integers; otherwise `0`. |
| `i32.lt_u` | Returns `1` when the first input is less than the second as unsigned 32-bit integers; otherwise `0`. |
| `i32.gt_u` | Returns `1` when the first input is greater than the second as unsigned 32-bit integers; otherwise `0`. |
| `i32.le_u` | Returns `1` when the first input is less than or equal to the second as unsigned 32-bit integers; otherwise `0`. |
| `i32.ge_u` | Returns `1` when the first input is greater than or equal to the second as unsigned 32-bit integers; otherwise `0`. |

The backend emits extra checked/helper code where Vircon32's division behavior
does not directly match these Wasm rules. Source authors should still avoid
division by zero rather than relying on a particular trap presentation.

### Supported 64-bit integer subset

Vircon32 registers and the public defined-function ABI are 32-bit. For the
supported i64 subset, wasm2vircon therefore represents every temporary or
local i64 as two compiler-managed i32 stack words: the low 32 bits first, then
the high 32 bits. This preserves Wasm's little-endian memory layout without
pretending that a Wasm i64 is one native Vircon32 register.

An i64 shift count uses its low six bits, as WebAssembly specifies: a count of
64 is equivalent to zero. Equality instructions return ordinary `i32` boolean
values (`1` or `0`).

| Instruction | Meaning in this profile |
| --- | --- |
| `i64.extend_i32_s` | Converts an i32 to i64 by copying its low word and filling the high word with zeros for a non-negative input or ones for a negative input. |
| `i64.extend_i32_u` | Converts an i32 to i64 by copying its low word and setting the high word to zero. |
| `i32.wrap_i64` | Discards the high 32 bits of an i64 and returns its low word as i32. |
| `i64.add` | Adds two i64 values, including the carry from the low word into the high word. Overflow past bit 63 is discarded. |
| `i64.sub` | Subtracts the second i64 from the first, including borrow from the high word. Underflow wraps modulo 2^64. |
| `i64.mul` | Multiplies two i64 values and returns the low 64 bits of the product. Bits above bit 63 are discarded. |
| `i64.and` | Computes a bitwise AND independently across the low and high words. |
| `i64.or` | Computes a bitwise OR independently across the low and high words. |
| `i64.xor` | Computes a bitwise exclusive OR independently across the low and high words. |
| `i64.shl` | Shifts the full 64-bit bit pattern left by the low six bits of the count, inserting zero bits on the right. |
| `i64.shr_u` | Shifts the full 64-bit bit pattern right by the low six bits of the count, inserting zero bits on the left. |
| `i64.shr_s` | Shifts the full 64-bit value right as a signed two's-complement number, copying its original bit 63 into newly opened high bits. |
| `i64.eqz` | Returns `1` when both i64 words are zero, otherwise `0`. |
| `i64.eq` | Returns `1` when both corresponding i64 words are equal, otherwise `0`. |
| `i64.ne` | Returns `1` when either corresponding i64 word differs, otherwise `0`. |
| `i64.lt_s` | Returns `1` when the first i64 is less than the second as signed two's-complement integers, otherwise `0`. |
| `i64.le_s` | Returns `1` when the first i64 is less than or equal to the second as signed two's-complement integers, otherwise `0`. |
| `i64.gt_s` | Returns `1` when the first i64 is greater than the second as signed two's-complement integers, otherwise `0`. |
| `i64.ge_s` | Returns `1` when the first i64 is greater than or equal to the second as signed two's-complement integers, otherwise `0`. |
| `i64.lt_u` | Returns `1` when the first i64 is less than the second as unsigned values from `0` through `2^64-1`, otherwise `0`. |
| `i64.le_u` | Returns `1` when the first i64 is less than or equal to the second as unsigned values, otherwise `0`. |
| `i64.gt_u` | Returns `1` when the first i64 is greater than the second as unsigned values, otherwise `0`. |
| `i64.ge_u` | Returns `1` when the first i64 is greater than or equal to the second as unsigned values, otherwise `0`. |

### Floating-point operations and conversions

`f32` operations use Vircon32 single-precision hardware instructions. This
profile is intended for ordinary finite game values; it does not promise full
portable behavior for NaN, infinity, or signed zero.

| Instruction | Meaning in this profile |
| --- | --- |
| `f32.add` | Adds two single-precision floating-point values. |
| `f32.sub` | Subtracts the second single-precision value from the first. |
| `f32.mul` | Multiplies two single-precision floating-point values. |
| `f32.div` | Divides the first single-precision value by the second. Avoid zero and non-finite inputs; their complete Wasm edge-case behavior is not part of this restricted profile's promise. |
| `f32.neg` | Changes the sign of a single-precision value: positive becomes negative and negative becomes positive. |
| `f32.abs` | Produces the non-negative magnitude of a single-precision value. |
| `f32.floor` | Rounds a finite float down toward negative infinity, such as `1.8` to `1.0` and `-1.2` to `-2.0`. |
| `f32.ceil` | Rounds a finite float up toward positive infinity, such as `1.2` to `2.0` and `-1.8` to `-1.0`. |
| `f32.eq` | Returns `1` when the two floats compare equal, otherwise `0`. A NaN compares unequal to every value, including itself. |
| `f32.ne` | Returns `1` when the two floats compare unequal, otherwise `0`. A NaN compares unequal to every value, including itself. |
| `f32.lt` | Returns `1` when the first float is less than the second, otherwise `0`. |
| `f32.le` | Returns `1` when the first float is less than or equal to the second, otherwise `0`. |
| `f32.gt` | Returns `1` when the first float is greater than the second, otherwise `0`. |
| `f32.ge` | Returns `1` when the first float is greater than or equal to the second, otherwise `0`. |
| `f32.convert_i32_s` | Reinterprets the input as a signed 32-bit integer and converts its numeric value to a float. For example, `0xFFFFFFFF` becomes `-1.0`. |
| `f32.convert_i32_u` | Interprets the input as an unsigned 32-bit integer and converts its numeric value to a float. `0xFFFFFFFF` rounds to approximately `4294967296.0`. |
| `i32.trunc_sat_f32_s` | Drops a float's fractional part and converts it to signed `i32`. Values outside the signed i32 range are clamped to the nearest endpoint instead of trapping. |
| `i32.reinterpret_f32` | Keeps all 32 raw bits of a float but treats those bits as an i32. It performs no numeric conversion. |
| `f32.reinterpret_i32` | Keeps all 32 raw bits of an i32 but treats those bits as a float. It performs no numeric conversion. |

### Structured control flow

| Instruction | Meaning in this profile |
| --- | --- |
| `block` | Groups a sequence of expressions and creates a named structured branch target. Resultless blocks are supported, as are named `i32` and `f32` result blocks whose branches carry one value to the block result. Branching to a block exits after its final expression. |
| `loop` | Groups a sequence of expressions and creates a structured branch target at its beginning. It has no result value here. Branching to a loop starts its next iteration. |
| `if` | Evaluates an `i32` condition and executes its then-body when the condition is nonzero. Resultless conditionals support an optional `else`. An `if` may also produce one `i32` or `f32` value when both arms merge at the end. |
| `br` | Unconditionally transfers to one active enclosing `block` or `loop`, selected by Wasm's nesting depth. It may carry one `i32` or `f32` value when targeting a compatible named result block. |
| `br_if` | Evaluates an `i32` condition and performs the same structured branch as `br` only when that condition is nonzero. A value-carrying form preserves its value on the non-branching path. |
| `br_table` | Selects one active enclosing resultless `block` or `loop` target from an `i32` selector. See the explanation below. |
| `return` | Leaves the current defined function. A void function returns no value; an `i32` or `f32` function returns one value. |
| `call` | Calls a directly named defined function or one allowlisted `env` platform import. Function pointers and indirect calls are not supported. |
| `unreachable` | Immediately takes the shared `__wasm_trap` path, which halts the target. It is not an ignored marker or a no-op. |

Wasm branches use structured nesting, not arbitrary assembly labels. The
compiler maintains a control-target stack so nested branch depths resolve to
the correct enclosing block or loop.

`br_table` uses the selector as an **unsigned** number: case zero is selected
by zero, case one by one, and so on. Every value outside the table's case range
takes its required default target. Branch values are not supported. The
compiler resolves all targets through that same control-target stack, then
emits an immutable cartridge-ROM table of target code addresses. It bounds
checks the selector, loads the selected address, and transfers with Vircon32
`JMP Rn`. `br_table` branch values remain unsupported.

### Linear-memory operations

| Instruction | Meaning in this profile |
| --- | --- |
| `i32.load8_u` | Reads one byte at the calculated byte address and returns an `i32` from `0` through `255`. The `_u` means the byte is zero-extended, never treated as a negative signed byte. |
| `i32.store8` | Stores the low eight bits of its value at the calculated byte address. It preserves the neighboring three byte lanes in the containing Vircon RAM word. |
| `i32.load` | Reads four consecutive bytes at the calculated byte address and forms a 32-bit little-endian `i32`. The address may be aligned or unaligned. |
| `i32.store` | Splits an `i32` into four little-endian bytes and stores them at consecutive byte addresses. The address may be aligned or unaligned. |
| `f32.load` | Reads four consecutive bytes at the calculated byte address and uses their unchanged 32-bit pattern as an `f32`. The address may be aligned or unaligned. |
| `f32.store` | Writes the unchanged 32-bit pattern of an `f32` to four consecutive bytes. The address may be aligned or unaligned. |
| `i64.load` | Reads eight consecutive little-endian bytes into a pair-valued i64. The address may be aligned or unaligned. |
| `i64.store` | Writes a pair-valued i64 as eight consecutive little-endian bytes. The address may be aligned or unaligned. A statically aligned constant store may use a smaller direct two-word lowering. |
| `memory.copy` | Copies `length` bytes from source byte address to destination byte address in the default linear memory. It is overlap-safe, like C `memmove`. |
| `memory.fill` | Writes `length` bytes at destination byte address. Each byte is the low eight bits of the supplied fill value. |
| `memory.size` | Returns the current default-memory length in 64 KiB Wasm pages. It reads compiler-owned page-count state, so its result changes after successful growth. |
| `memory.grow` | Evaluates its unsigned `i32` page request, zero-initializes that many new pages on success, and returns the old page count. It returns `0xFFFFFFFF` without changing memory if the request exceeds the module maximum or the 122-page target limit. |

The `offset` immediate on every supported load/store participates in the
effective address: the dynamic address plus that constant byte offset is the
address used by the operation. The Wasm `align` immediate is only an
optimization hint; correctness never depends on the runtime address actually
having that alignment.

### i64 storage optimization

The i64 instructions listed above are general supported expressions within a
function's local/temporary pair-value model. Separately, when a constant
`i64.store` has a constant, four-byte-aligned, in-bounds address, the compiler
may emit two direct Vircon RAM word stores instead of the normal byte-addressed
memory lowering. This is only a code-size optimization; it does not change
Wasm byte layout or make constant stores semantically special.

## What standard WebAssembly is not supported

The WebAssembly specification contains substantially more functionality than
VirconWasm. An opcode that is not listed in the supported-instructions section
is rejected. The entries below explain the reason for each currently relevant
unsupported family.

“Candidate” means that the feature looks compatible with the architecture and
could be added after a real normalized frontend module demonstrates the need.
It is not a promise that the compiler already accepts it.

### Value types and operations

**`f32.min`, `f32.max`, `f32.copysign`, `f32.trunc`, and `f32.nearest`**

Some have superficially similar Vircon32 instructions: `FMIN`, `FMAX`, and
`ROUND`. They do not automatically provide the exact WebAssembly rules for
NaN propagation, signed zero, or ties-to-even rounding. `f32.copysign` needs
precise bit-level signed-zero/NaN handling, while `f32.trunc` needs a float
result rather than the target's integer conversion. These are not added merely
because a similarly named target instruction exists. They are candidates only
after their required edge-case contract is defined and tested.

**`f64` and all 64-bit floating-point operations**

Vircon32 has single-precision floating-point instructions only. Supporting
`f64` would require a software floating-point runtime with carefully specified
NaN, rounding, conversion, and ABI behavior. It is deliberately out of scope
for the current game-oriented profile.

**Remaining i64 operations and ABI forms**

The documented i64 local/temporary subset uses adjacent i32 words, but i64
parameters, function results, and direct-call argument passing are still
unsupported because the existing call ABI transports one word per parameter
and returns through one 32-bit register. Signed/unsigned division and
remainder, rotates, narrow loads/stores, and most
conversions are also not implemented yet. Some are plausible pair-lowering
candidates, but division and a public multiword call ABI need careful trap and
calling-convention design rather than a superficial instruction mapping.

**`i32.clz`, `i32.ctz`, and `i32.popcnt`**

Vircon32 has no direct count-leading-zero, count-trailing-zero, or population
count instruction. Each could be implemented by a generated helper routine,
but it's currently not included because the helper's size and edge-case tests
have not yet been justified by a supported application.

**Other missing i32 conversions**

Non-saturating `i32.trunc_f32_s` and `i32.trunc_f32_u` must trap for NaN and
out-of-range input, unlike the supported saturating conversion.
`i32.trunc_sat_f32_u` also needs correct unsigned handling between `2^31` and
`2^32-1`. These are not direct target conversions and remain unimplemented
until their full Wasm semantics are tested.

**`f32.sqrt`**

Vircon32 has no direct square-root instruction. A runtime implementation is
possible, but it would need an accuracy and exceptional-value policy. The
public C API can currently use its finite-math layer instead, so no general
Wasm square-root helper has been added.


**SIMD, relaxed SIMD, reference values, and GC types**

`v128`, SIMD opcodes, reference types, and garbage-collection types require
value representations and runtime facilities that do not exist in Vircon32.
They are deliberate non-goals rather than partially implemented features.

Use the public C finite-math helpers where appropriate. They lower through
verified Vircon CPU operations rather than requiring broad general Wasm float
support.

### Control flow and calls

**Structured result values and loop results**

An `if` can produce one `i32` or `f32` result; but value-producing loops,
i64 structured results, and value-carrying `br_table` remain unsupported,
they need additional loop-carried or multiword value-flow rules.

**Tables, element segments, `call_indirect`, function pointers, and reference calls**

These require a function-table representation, table initialization, indirect
call validation, and an ABI for dynamically selected callees. They are not
needed by the current direct-call profile and are deliberately excluded until a
source-language use case justifies that runtime machinery.

**Exceptions, tail calls, continuations, and stack switching**

These proposals require substantial changes to function exit, stack-frame, and
control-transfer rules. VirconWasm deliberately does not try to act as a full
Wasm virtual machine, so they are out of scope.

### Memories, globals, and bulk features

**`i32.load8_s`**

The signed-byte load is not yet accepted even though the current profile has
the needed sign-extension operation. It is a small **candidate**: load one byte through
the existing packed-memory path, then sign-extend it. It has not been added
yet because no normalized supported application has emitted it.

**`i32.load16_s`, `i32.load16_u`, and `i32.store16`**

Wasm permits these accesses at every byte address, including odd addresses.
On packed word-addressed Vircon RAM, an unaligned 16-bit access can span two
target words and needs correct byte combination or read-modify-write behavior.
This is a plausible next memory **candidate**, but it deserves focused lane and
bounds tests instead of being assumed equivalent to a target halfword access.

**Multiple memories, memory imports, `memory64`, shared memory, and atomics**

The profile owns one fixed 32-bit linear-memory region in Vircon RAM. Multiple
or imported memories would need multiple region/initialization policies;
`memory64` cannot fit the 32-bit target address model; shared memory and
atomics need concurrency semantics absent from Vircon32. These are deliberate
exclusions; the one supported memory remains confined to the documented middle
RAM region even when it grows.

**Passive/declarative data segments and `memory.init`/`data.drop`**

Current active data segments are copied into writable RAM at cartridge startup.
Passive segments and their bulk-memory instructions require retaining
ROM-resident initialization data plus dynamic segment-lifetime state. That is
more runtime machinery than the current static-data model needs.

**Table bulk operations**

`table.init`, `elem.drop`, `table.copy`, `table.fill`, and related operations
remain unavailable because tables themselves are unavailable.

**Arbitrary globals**

The only accepted exception is the canonical mutable i32 `__stack_pointer`
linker global used by some frontends for automatic aggregates. General globals
need explicit static initialization, mutability, and address/lifetime rules at
the frontend boundary. They are intentionally not enabled merely to accept
linker artifacts.

### Host/runtime facilities

**WASI, system calls, browser imports, dynamic linking, and an embedded Wasm interpreter**

Vircon32 cartridges have no operating-system process model, filesystem, web
host, dynamic linker, or need to execute arbitrary Wasm at runtime. The
compiler produces native Vircon32 assembly, so embedding a Wasm VM would work
against the project's design.

**Arbitrary imports**

Only the documented `env` platform-import allowlist is accepted. Treating an
unknown import as an assembly symbol would bypass signature validation and turn
misspelled or unsupported runtime dependencies into unclear linker failures.
New imports are added only as narrow, language-neutral hardware operations.

**Threads, atomics, the component model, and the WebAssembly JavaScript API**

Vircon32 has no thread scheduler, atomic-memory model, JavaScript host, or
component-model runtime. These are deliberate non-goals for a static
single-cartridge compiler path.

These exclusions are intentional. wasm2vircon aims to translate useful,
inspectable frontend output into native Vircon32 code, not to emulate an entire
WebAssembly machine.

## When a program is rejected

A rejection normally means the frontend emitted a construct outside this
profile. The diagnostic includes the Wasm opcode, function index, and a useful
function name when one is available, plus an expression path.

Treat that as an investigation point:

1. Inspect the normalized Wasm that the frontend actually emitted.
2. Identify the source construct or linker artifact responsible.
3. Decide whether a small source rewrite is appropriate.
4. Add backend support only when the construct is useful, well understood, and
   covered by a focused test.

This keeps WebAssembly-specific behavior at the frontend boundary and keeps
the Vircon32 backend usable by future non-Wasm frontends.
