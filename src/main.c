/* Command-line driver for the Wasm decoding, validation, lowering, and emit
 * pipeline. */

#include "diagnostics.h"
#include "emitter.h"
#include "legalize.h"
#include "lowering.h"
#include "validator.h"
#include "vircon_ir.h"
#include "wasm_module.h"

#include <stdio.h>
#include <string.h>

/* Prints the deliberately small public command-line interface. */
static void print_usage(FILE *stream) {
  fprintf(stream, "Usage: wasm2vircon input.wasm [--entry NAME] "
                  "[--allow-stack-pointer] [--skip-input-optimization] -o output.asm\n"
                  "       wasm2vircon --validate-only input.wasm [--entry NAME] "
                  "[--allow-stack-pointer] [--skip-input-optimization]\n"
                  "       wasm2vircon --report-profile input.wasm\n"
                  "\n"
                  "Translate the supported VirconWasm v1.14 profile into Vircon32 "
                  "assembly.\n"
                  "The default entry export is main; --entry selects a different "
                  "frontend\n"
                  "entry export. Entries may return void or i32. "
                  "--allow-stack-pointer accepts only the\n"
                  "restricted mutable i32 __stack_pointer ABI global.\n"
                  "\n"
                  "--validate-only performs decoding and VirconWasm validation without "
                  "writing assembly.\n"
                  "--skip-input-optimization disables only the optional embedded input "
                  "optimizer; compiler-owned processing and validation still run.\n"
                  "--report-profile writes a Binaryen-decoded module inventory and "
                  "complete Wasm text; it\n"
                  "does not claim that the module is accepted by the restricted "
                  "VirconWasm profile.\n");
#ifdef USE_EMBEDDED_BINARYEN
  fputs("Embedded Binaryen normalization is enabled at build time.\n", stream);
#else
  fputs("Embedded Binaryen normalization is disabled; compiler-owned linker-artifact legalization still runs.\n",
        stream);
#endif
}

/* Parses one module invocation and releases every compiler stage on exit. */
int main(int argc, char **argv) {
  const char *input_path = NULL;
  const char *output_path = NULL;
  const char *entry_name = "main";
  bool allow_stack_pointer = false;
  bool skip_input_optimization = false;
  bool validate_only = false;
  bool report_profile = false;
  Diagnostics diagnostics;
  WasmModule module;
  ValidatedModule validated;
  VirconIrProgram program;
  int index;
  int status = 1;

  diagnostics_init(&diagnostics, stderr);
  memset(&module, 0, sizeof(module));
  memset(&validated, 0, sizeof(validated));
  vircon_ir_init(&program);

  for (index = 1; index < argc; index++) {
    if (strcmp(argv[index], "--help") == 0) {
      print_usage(stdout);
      status = 0;
      goto done;
    }
    if (strcmp(argv[index], "--entry") == 0) {
      if (++index == argc) {
        diagnostics_error(&diagnostics, "missing name after --entry");
        goto done;
      }
      entry_name = argv[index];
      continue;
    }
    if (strcmp(argv[index], "--allow-stack-pointer") == 0) {
      allow_stack_pointer = true;
      continue;
    }
    if (strcmp(argv[index], "--skip-input-optimization") == 0) {
      skip_input_optimization = true;
      continue;
    }
    if (strcmp(argv[index], "--validate-only") == 0) {
      validate_only = true;
      continue;
    }
    if (strcmp(argv[index], "--report-profile") == 0) {
      report_profile = true;
      continue;
    }
    if (strcmp(argv[index], "-o") == 0) {
      if (++index == argc) {
        diagnostics_error(&diagnostics, "missing path after -o");
        goto done;
      }
      output_path = argv[index];
      continue;
    }
    if (argv[index][0] == '-') {
      diagnostics_error(&diagnostics, "unknown option '%s'", argv[index]);
      goto done;
    }
    if (input_path != NULL) {
      diagnostics_error(&diagnostics, "only one input Wasm module is supported");
      goto done;
    }
    input_path = argv[index];
  }
  if (validate_only && report_profile) {
    diagnostics_error(&diagnostics, "--validate-only and --report-profile cannot be used "
                                    "together");
    goto done;
  }
  if (input_path == NULL || ((!validate_only && !report_profile && output_path == NULL) ||
                             ((validate_only || report_profile) && output_path != NULL))) {
    print_usage(stderr);
    goto done;
  }
  if (report_profile) {
    if (!wasm_module_report_profile(input_path, stdout, &diagnostics))
      goto done;
    status = 0;
    goto done;
  }
  if (!wasm_module_load(input_path, !skip_input_optimization, &module, &diagnostics)) {
    goto done;
  }
  if (!wasm_module_legalize_linker_artifacts(&module, &diagnostics)) {
    goto done;
  }
  if (!validate_virconwasm_v1(&module, entry_name, allow_stack_pointer, &validated, &diagnostics)) {
    goto done;
  }
  if (validate_only) {
    fprintf(stdout, "%s: VirconWasm v1.14 validation passed for entry '%s'\n", input_path, entry_name);
    status = 0;
    goto done;
  }
  if (!lower_module_to_vircon_ir(&validated, &program, &diagnostics) ||
      !emit_vircon_assembly(&program, output_path, &diagnostics))
    goto done;
  status = 0;

done:
  vircon_ir_dispose(&program);
  validated_module_dispose(&validated);
  wasm_module_dispose(&module);
  return status;
}
