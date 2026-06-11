# Testing

Tests are built when the `tests` option is enabled (the default) and run with
Meson's test runner.

```sh
meson setup build
meson test -C build
```

List the registered tests with `meson test -C build --list`.

## Unit test suites

| Suite             | Executable           | Covers                                              |
| ----------------- | -------------------- | --------------------------------------------------- |
| `test-cper-tests` | `tests/cper-tests`   | CPER decode/encode, schema validation, round trips  |
| `test-cpad-tests` | `tests/cpad-tests`   | CPAD decode/encode, header validation, round trips  |
| `test-python`     | (runs the extension) | The Python bindings (only when `python` is enabled) |

Each suite generates pseudo-random records, converts them to the JSON IR,
validates the IR against the schemas under `specification/`, and asserts that
the binary -> IR -> binary round trip is byte-identical. The CPAD and CPER
suites also compare against committed golden examples in `examples/`.

`test-python` is only registered when the project is configured with
`-Dpython=enabled`:

```sh
meson setup build-py -Dpython=enabled
meson test -C build-py
```

### Regenerating golden examples

The golden `examples/*.cperhex` / `*.cpadhex` + `*.json` pairs are produced by
the test programs. To regenerate them after an intentional format change, flip
the generation flag at the top of the test source and run the test once:

- CPER: set `GEN_EXAMPLES = 1` in `tests/ir-tests.c`
- CPAD: set `GEN_CPAD_EXAMPLES = 1` in `tests/cpad-ir-tests.c`

Rebuild, run the suite (the files are rewritten), then set the flag back to
`0` and re-run to confirm the committed files pass.

## Fuzzing

A [libFuzzer](https://llvm.org/docs/LibFuzzer.html) target,
`fuzz_cper_buf_to_ir`, feeds arbitrary bytes to `cper_buf_to_ir` and asserts
that any successfully-decoded record validates against the schema. It is built
only with Clang (it needs libFuzzer plus the Address/Leak sanitizers), so use a
dedicated build directory:

```sh
CC=clang meson setup build-fuzz -Dfuzz=enabled
ninja -C build-fuzz
meson test -C build-fuzz fuzz_cper_buf_to_ir
```

By default the test runs the fuzzer for a bounded time (`-max_total_time=10`).
You can also run the binary directly with your own corpus or libFuzzer flags:

```sh
build-fuzz/tests/fuzz_cper_buf_to_ir corpus/
```
