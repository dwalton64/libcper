# CPER JSON Representation & Conversion Library

This repository specifies a structure for representing UEFI CPER records (as
described in UEFI Specification Appendix N) in a human-readable JSON format, in
addition to a library which can readily convert back and forth between the
standard CPER binary format and the specified structured JSON.

## Prerequisites

Before building this library and its associated tools, you must have meson
(>=1.1.1)

## Building

This project uses Meson (>=1.1.1). To build for native architecture, simply run:

```sh
meson setup build
ninja -C build
```

## Usage

This project comes with several binaries to help you deal with CPER and CPADs in binary and
JSON formats. The first of these is `cper-convert`, which is a command line tool
that can be found in `build/`. With this, you can convert to and from CPER and
CPER-JSON through the command line. An example usage scenario is below:

```sh
build/cper-convert to-cper examples/arm.json --out arm.cper
build/cper-convert to-json arm.cper --out arm.json

build/cpad-convert to-cpad examples/replace_part.json --out replace_part.cpad
build/cpad-convert to-json replace_part.cpad --out replace_part.json
```

This repository also provides tools for generating CPERs and CPADs in `build/`

`cper-generate`
Allows you to generate pseudo-random valid CPER records with sections of
specified types for testing purposes. 

`cpad-generate`
Generates CPADS with specified Action IDs and section types.  Note that, because
micro-architectures vary significantly, CPAD bodies are often expected to 
contain opaque binary data.  Use `unknown` sections to generate representative
CPADs.

Examples of CPER and CPAD generation follow:

```sh
build/cper-generate --out cper.generated.dump --sections generic ia32x64

build/cpad-generate --out replace_part.cpad --action-ids 5 --sections unknown
```

Help for these tools can be accessed through using the `--help` flag in
isolation.

Finally, a static library containing symbols for converting CPER and CPER-JSON
between an intermediate JSON format can be found generated at
`lib/libcper-parse.a`. This contains the following useful library symbols:

```sh
json_object* cper_to_ir(FILE* cper_file);
void ir_to_cper(json_object* ir, FILE* out);
```

## Specification

The specification for this project's CPER-JSON format can be found in
`specification/`, defined in both JSON Schema format and also as a LaTeX
document. Specification for the CPER binary format can be found in
[UEFI Specification Appendix N](https://uefi.org/sites/default/files/resources/UEFI_Spec_2_9_2021_03_18.pdf)
(2021/03/18).

## Usage Examples

This library is utilised in a proof of concept displaying CPER communication
between a SatMC and OpenBMC board, including a conversion into CPER JSON for
logging that utilises this library. You can find information on how to reproduce
the prototype at the
[scripts repository](https://gitlab.arm.com/server_management/cper-poc-scripts),
and example usage of the library itself at the
[pldm](https://gitlab.arm.com/server_management/pldm) repository.
