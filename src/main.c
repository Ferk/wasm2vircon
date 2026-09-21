#include "diagnostics.h"
#include "emitter.h"
#include "lowering.h"
#include "validator.h"
#include "vircon_ir.h"
#include "wasm_module.h"

#include <stdio.h>
#include <string.h>

static void print_usage(FILE *stream)
{
    fprintf(stream,
            "Usage: wasm2vircon input.wasm [--entry NAME] -o output.asm\n"
            "\n"
            "Translate the supported VirconWasm v1.6 profile into Vircon32 assembly.\n"
            "The default entry export is __original_main; --entry makes the frontend\n"
            "entry convention explicit.\n");
}

int main(int argc, char **argv)
{
    const char *input_path = NULL;
    const char *output_path = NULL;
    const char *entry_name = "__original_main";
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
    if (input_path == NULL || output_path == NULL) {
        print_usage(stderr);
        goto done;
    }
    if (!wasm_module_load(input_path, &module, &diagnostics) ||
        !validate_virconwasm_v1(&module, entry_name, &validated, &diagnostics) ||
        !lower_module_to_vircon_ir(&validated, &program, &diagnostics) ||
        !emit_vircon_assembly(&program, output_path, &diagnostics)) {
        goto done;
    }
    status = 0;

done:
    vircon_ir_dispose(&program);
    validated_module_dispose(&validated);
    wasm_module_dispose(&module);
    return status;
}
