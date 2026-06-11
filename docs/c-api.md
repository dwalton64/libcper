# Using the C library

The core library `libcper-parse` converts CPER and CPAD records between their
binary form and a [json-c](https://github.com/json-c/json-c) `json_object`
intermediate representation. Headers are installed under `libcper/`.

## Public API

From `libcper/cper-parse.h`:

```c
int cper_header_valid(const char *cper_buf, size_t size);

json_object *cper_to_ir(FILE *cper_file);
json_object *cper_buf_to_ir(const unsigned char *cper_buf, size_t size);
json_object *cper_single_section_to_ir(FILE *cper_section_file);
json_object *cper_buf_single_section_to_ir(const unsigned char *cper_buf,
                                           size_t size);

void ir_to_cper(json_object *ir, FILE *out);
void ir_single_section_to_cper(json_object *ir, FILE *out);
```

From `libcper/cpad-parse.h` (the CPAD equivalents):

```c
int cpad_header_valid(const char *cpad_buf, size_t size);

json_object *cpad_to_ir(FILE *cpad_file);
json_object *cpad_buf_to_ir(const unsigned char *cpad_buf, size_t size);
json_object *cpad_single_section_to_ir(FILE *cpad_section_file);
json_object *cpad_buf_single_section_to_ir(const unsigned char *cpad_buf,
                                           size_t size);

void ir_to_cpad(json_object *ir, FILE *out);
void ir_single_section_to_cpad(json_object *ir, FILE *out);
```

The `*_to_ir` functions return a new `json_object` (release it with
`json_object_put`). The `ir_to_*` functions write binary to a `FILE *`, which
may be a real file or an in-memory stream from `open_memstream`.

## Example

Decode a CPER buffer, print it, and re-encode it:

```c
#include <stdio.h>
#include <stdlib.h>
#include <json.h>
#include <libcper/cper-parse.h>

int main(int argc, char **argv)
{
	FILE *f = fopen(argv[1], "rb");
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	unsigned char *buf = malloc(n);
	fread(buf, 1, n, f);
	fclose(f);

	/* Binary -> JSON IR. */
	json_object *ir = cper_buf_to_ir(buf, n);
	if (ir == NULL) {
		fprintf(stderr, "not a valid CPER record\n");
		return 1;
	}
	printf("%s\n", json_object_to_json_string_ext(ir,
						       JSON_C_TO_STRING_PRETTY));

	/* JSON IR -> binary (to an in-memory stream). */
	char *out = NULL;
	size_t out_size = 0;
	FILE *mem = open_memstream(&out, &out_size);
	ir_to_cper(ir, mem);
	fclose(mem);
	printf("re-encoded %zu bytes\n", out_size);

	json_object_put(ir);
	free(out);
	free(buf);
	return 0;
}
```

## Compiling and linking

If `libcper` is installed, use pkg-config:

```sh
cc example.c $(pkg-config --cflags --libs libcper) $(pkg-config --cflags --libs json-c) -o example
```

Against an uninstalled build tree, point the compiler at the headers and the
built library directly:

```sh
cc example.c -Iinclude -Lbuild -lcper-parse $(pkg-config --cflags --libs json-c) -o example
LD_LIBRARY_PATH=build ./example sample.cper
```

CPAD programs are identical except they include `libcper/cpad-parse.h` and call
the `cpad_*` / `ir_to_cpad` functions.
