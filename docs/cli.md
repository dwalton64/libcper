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

## `lscpad`

Lists the contents of CPAD files in a human-readable form. The optional path
argument is the default argument (no flag): with no path it lists every
`.cpad` file in the current directory; given a directory it lists every
`.cpad` file in it (non-recursive, alphabetically sorted); given a file it
lists that one file.

```sh
# List every .cpad file in the current directory.
build/lscpad

# List a single file.
build/lscpad sample.cpad

# List a directory of CPADs.
build/lscpad mydir/

# Print each CPAD as JSON instead of the formatted summary.
build/lscpad --json sample.cpad
```

For each file `lscpad` prints the `CPAD Header` (Timestamp, PlatformID,
PartitionID, CreatorID, RecordID, Flags) followed by each `CPAD Section
Descriptor` (SectionType, FruID, Urgency, Confidence, FruText, Action ID).
Header and section-descriptor fields whose validation bit is not set are
shown as `-- Valid Flag Not Set --`. A worked example:

```sh
build/cpad-generate --out sample.cpad --action-ids 5 --sections os-generic
build/lscpad sample.cpad
```

## `create-platform-action-cper`

Builds a Platform Action Event CPER from a binary CPAD, reporting the outcome
of one or more CPAD actions. One Platform Action Event section is emitted per
selected CPAD section; the CPER header copies the CPAD's PlatformID,
PartitionID and CreatorID (the CreatorID routes the CPER back to the
analyzer).

Section indices are 0-based. Return/reason codes may be given as a keyword or
a number (decimal or `0x` hex); run `--help` for the full list. There are
three mutually exclusive ways to supply codes:

```sh
# Uniform: the same codes for every section, optionally skipping some.
build/create-platform-action-cper sample.cpad --out ev.cper \
    --return-code failed --reason-code timeout --exclude-sections 1

# Per-section: emit exactly the listed sections, each with its own codes.
build/create-platform-action-cper sample.cpad --out ev.cper \
    --section 0:failed:invalid-fru-id \
    --section 2:success:none

# Interactive: with no code flags, prompts ask, per section, whether to emit
# an event and which codes to use.
build/create-platform-action-cper sample.cpad --out ev.cper
```

In interactive mode each section prompt shows the CPAD Action ID and its name,
and codes are chosen from a numbered menu (type the menu number or the
keyword). The reason-code menu is limited to the reasons valid for the chosen
return code; when "No Reason Code" is the only standard option it is selected
automatically (press Enter), with the option to enter a vendor-specific code
(`0x80`–`0xFF`).

Output defaults to **binary** and requires `--out`; `--json` emits the IR
instead (to `--out` or stdout). Out-of-range return codes and reason codes
that are not valid for their return code produce a warning on stderr but are
still written; vendor-specific reason codes (`0x80`–`0xFF`) are accepted
silently. A full round trip:

```sh
build/cpad-generate --out sample.cpad --action-ids 5 --sections os-generic
build/create-platform-action-cper sample.cpad --out ev.cper \
    --return-code success --reason-code none
build/cper-convert to-json ev.cper
```

## A note on the example files

The `examples/` directory stores records as **hex text** (`*.cperhex`,
`*.cpadhex`) paired with their decoded `*.json`. The hex files are test
fixtures, not binaries; to feed one to a tool, decode it first, e.g.:

```sh
xxd -r -p < examples/arm.cperhex > arm.cper
build/cper-convert to-json arm.cper
```

The `*.json` files can be passed directly to `to-cper` / `to-cpad`.
