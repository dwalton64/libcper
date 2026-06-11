# Concepts

`libcper` deals with two related binary record formats and a JSON
intermediate representation (IR) that mirrors them.

## CPER (Common Platform Error Record)

A CPER is the standard UEFI structure for reporting a hardware error,
defined in
[UEFI Specification Appendix N](https://uefi.org/specs/UEFI/2.9_A/Apx_N_Common_Platform_Error_Record.html).
A record consists of:

- A **record header** with a signature (`CPER`), revision, timestamp,
  severity, platform/partition/creator GUIDs, and a section count.
- One or more **section descriptors**, each giving the offset, length,
  severity, FRU information, and a **section type GUID** for a section body.
- The **section bodies** themselves (processor, memory, PCIe, firmware,
  vendor-specific, etc.).

`libcper` knows how to decode the section types listed by
`cper-generate --help` and renders unknown sections as base64.

## CPAD (Common Platform Action Descriptor)

A CPAD describes one or more **actions** a platform should take in response
to analysis of errors. It is defined by the OCP RAS API specification and
deliberately mirrors the CPER layout so the same routing fields (platform ID,
partition ID, creator ID, record ID) can be reused.

A CPAD record consists of:

- A **record header** with a signature (`CPAD`), revision, timestamp,
  highest **urgency**, routing GUIDs, and a section count.
- One or more **section descriptors**. In addition to the CPER-style fields,
  each descriptor carries an **Action ID** (what to do — e.g. replace part,
  power cycle), an **urgency**, and a **confidence** level.
- The **section bodies** describing the action. Because micro-architectures
  vary, most CPAD bodies are vendor-defined opaque data; `libcper` ships an
  `os-generic` body and treats everything else as `unknown` (base64).

### Platform Action Events

When an endpoint acts on a CPAD, it reports the outcome using a **Platform
Action Event** — a CPER section (severity "Platform Action Event") that
carries the action return code, a return reason code, and enough of the
source CPAD's identifiers to correlate the result with the requested action.
This section is handled by `libcper` like any other CPER section.

## The JSON intermediate representation (IR)

Rather than expose the packed binary structures directly, `libcper` converts
records to/from a JSON IR. Decoding (`*_to_ir`) produces JSON; encoding
(`ir_to_*`) writes binary. The IR is the same data the CLI tools print and
the Python bindings return.

A decoded record has three top-level keys:

- `header` — the record header fields.
- `sectionDescriptors` — an array of per-section descriptors.
- `sections` — an array of section bodies (one entry per descriptor).

The exact shape of the IR is defined by JSON Schemas under `specification/`
(see [specification.md](specification.md)). Multi-byte integer fields that do
not have a natural JSON representation are emitted as `0x`-prefixed hex
strings; large counters and addresses are emitted as JSON integers.
