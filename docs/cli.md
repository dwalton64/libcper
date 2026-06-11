# Command-line tools

These tools are built under `build/` when the `utility` option is enabled
(the default). Each accepts `--help` for full usage.

Examples below assume you have run `meson setup build && ninja -C build`.

## `cper-convert`

Converts CPER records between binary and the JSON IR.

```sh
# Binary CPER -> JSON (stdout, or a file with --out)
build/cper-convert to-json sample.cper --out sample.json

# A single section (descriptor + body, no record header) -> JSON
build/cper-convert to-json-section section.bin --out section.json

# JSON -> binary CPER (whole record or single section, auto-detected)
build/cper-convert to-cper sample.json --out sample.cper
```

`to-cper` accepts `--no-validate`, `--debug`, and `--specification
<path>`. If the input is not a valid binary header, `cper-convert to-json`
also accepts base64-encoded input.

## `cpad-convert`

The CPAD counterpart of `cper-convert`, with the same sub-commands
(`to-json`, `to-json-section`, `to-cpad`).

```sh
build/cpad-convert to-json sample.cpad --out sample.cpad.json
build/cpad-convert to-cpad sample.cpad.json --out sample.cpad
```

## `cper-generate`

Generates pseudo-random, spec-compliant CPER records for testing.

```sh
# A full record containing a generic processor and an IA32/x64 section.
build/cper-generate --out sample.cper --sections generic ia32x64

# A single section (no record header).
build/cper-generate --out section.bin --single-section arm

# Control validation bits ('all', 'some', or 'random'; default 'random').
build/cper-generate --out sample.cper --valid-bits all --sections memory
```

Run `build/cper-generate --help` for the full list of section type names
(generic, ia32x64, arm, arm-ras, memory, pcie, firmware, the CXL and DMAr
families, nvidia, ampere, platform-action-event, unknown, and others).

## `cpad-generate`

Generates pseudo-random CPAD records. Because CPAD bodies are usually
vendor-specific, the available section types are `os-generic` and `unknown`.

```sh
# A CPAD with one os-generic section and a "Replace Part" action (0x0005).
build/cpad-generate --out sample.cpad --action-ids 5 --sections os-generic

# A single section.
build/cpad-generate --out section.bin --single-section unknown
```

`--action-ids` is optional and, when given, must appear before `--sections`
with one ID per section (decimal or `0x`-prefixed hex).

## A note on the example files

The `examples/` directory stores records as **hex text** (`*.cperhex`,
`*.cpadhex`) paired with their decoded `*.json`. The hex files are test
fixtures, not binaries; to feed one to a tool, decode it first, e.g.:

```sh
xxd -r -p < examples/arm.cperhex > arm.cper
build/cper-convert to-json arm.cper
```

The `*.json` files can be passed directly to `to-cper` / `to-cpad`.
