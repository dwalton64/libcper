# Extending with OEM/vendor sections

Section support in `libcper` is modular: parsers and generators are tables of
definitions keyed by a section-type GUID, so vendor sections can be added at
compile time. This guide walks through adding a fictional `myvendor` section to
both the parser (`cper-parse`) and the generator (`cper-generate`).

CPAD sections are added the same way using the parallel `cpad-section.h` /
`gen-cpad-section.h` tables.

## 1. Define the section GUID

GUIDs are declared in `include/libcper/Cper.h` and defined in `Cper.c`, shared
by both the parser and the generator.

`include/libcper/Cper.h`:

```c
extern EFI_GUID gMyVendorSectionGuid;
```

`Cper.c`:

```c
EFI_GUID gMyVendorSectionGuid = { 0x40d26425, 0x3396, 0x4c4d,
				  { 0xa5, 0xda, 0x3d, 0x47, 0x26, 0x3a, 0xf4,
				    0x25 } };
```

## 2. Add a section parser

Create `include/libcper/sections/cper-section-myvendor.h`:

```c
#ifndef CPER_SECTION_MYVENDOR_H
#define CPER_SECTION_MYVENDOR_H

#include <json.h>
#include <libcper/Cper.h>

json_object *cper_section_myvendor_to_ir(const UINT8 *section, UINT32 size,
					 char **desc_string);
void ir_section_myvendor_to_cper(json_object *section, FILE *out);

#endif
```

Create `sections/cper-section-myvendor.c` implementing both directions:

```c
#include <stdio.h>
#include <json.h>
#include <libcper/Cper.h>
#include <libcper/sections/cper-section-myvendor.h>

json_object *cper_section_myvendor_to_ir(const UINT8 *section, UINT32 size,
					 char **desc_string)
{
	// Convert the `size` bytes at `section` into JSON IR.
	// Optionally write a short human-readable summary to *desc_string.
}

void ir_section_myvendor_to_cper(json_object *section, FILE *out)
{
	// Convert the JSON IR back into binary, writing it to `out`.
}
```

Register the section in `sections/cper-section.c` by adding an entry to
`section_definitions` (GUID, readable name, short name, the two conversion
functions):

```c
#include <libcper/sections/cper-section-myvendor.h>

CPER_SECTION_DEFINITION section_definitions[] = {
	// ...
	{ &gMyVendorSectionGuid, "MyVendor Error", "myvendor",
	  cper_section_myvendor_to_ir, ir_section_myvendor_to_cper },
};
```

## 3. Add a section generator (optional)

To let `cper-generate` produce your section, declare a generator in
`include/libcper/generator/sections/gen-section.h`:

```c
size_t generate_section_myvendor(void **location,
				 GEN_VALID_BITS_TEST_TYPE validBitsType);
```

Implement it in `generator/sections/gen-section-myvendor.c`, then register it in
`generator/sections/gen-section.c` by adding to `generator_definitions` (GUID,
shortcode, generator function). The shortcode is the name used on the
`cper-generate --sections` command line and **must contain no spaces** (this is
asserted by the test suite):

```c
CPER_GENERATOR_DEFINITION generator_definitions[] = {
	// ...
	{ &gMyVendorSectionGuid, "myvendor", generate_section_myvendor },
};
```

## 4. Register the new files with the build

Add the new C files to the source lists in the top-level `meson.build`:

- the parser source (`sections/cper-section-myvendor.c`) to
  `libcper_parse_sources`
- the generator source (`generator/sections/gen-section-myvendor.c`) to
  `libcper_generate_cper_sources`

Then rebuild:

```sh
ninja -C build
```

After this, `cper-convert`, `cper-generate`, and all of the conversion
libraries handle your section. The test suite enforces two invariants you
should keep in mind:

- if a section defines a `ToIR` function it must also define `ToCPER` (and vice
  versa), and
- generator shortcodes must not contain spaces.
