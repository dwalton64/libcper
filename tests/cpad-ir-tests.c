/**
 * Defines tests for validating CPAD-JSON IR output from the cpad-parse library.
 *
 * Author: drewwalton@microsoft.com
 **/

#include "test-utils.h"
#include "string.h"
#include "assert.h"
#include <ctype.h>
#include <json.h>
#include <libcper/log.h>
#include <libcper/cper-utils.h>

#include <libcper/Cpad.h>
#include <libcper/cpad-parse.h>
#include <libcper/generator/cpad-generate.h>
#include <libcper/generator/sections/gen-cpad-section.h>
#include <libcper/sections/cpad-section.h>

// Set to 1 to (re)generate the committed CPAD example files, then set back to 0.
static const int GEN_CPAD_EXAMPLES = 0;

//Returns a ready-for-use memory stream containing a CPAD record with the given sections inside.
static FILE *generate_cpad_record_memstream(const char **types,
					    UINT16 *action_ids,
					    UINT16 num_types, char **buf,
					    size_t *buf_size,
					    int single_section)
{
	FILE *stream = open_memstream(buf, buf_size);
	if (!single_section) {
		generate_cpad_record((char **)(types), action_ids, num_types,
				     stream);
	} else {
		generate_single_cpad_section_record(
			(char *)(types[0]), action_ids ? action_ids[0] : 0,
			stream);
	}
	fclose(stream);
	return fmemopen(*buf, *buf_size, "r");
}

static const char *cpad_ext = "cpadhex";
static const char *json_ext = "json";

/*
 * Example (golden) file helpers.
 */
struct cpad_file_info {
	char *cpad_out;
	char *json_out;
};

static void free_cpad_file_info(struct cpad_file_info *info)
{
	if (info == NULL) {
		return;
	}
	free(info->cpad_out);
	free(info->json_out);
	free(info);
}

static struct cpad_file_info *cpad_file_info_init(const char *example_name)
{
	struct cpad_file_info *info = (struct cpad_file_info *)calloc(
		1, sizeof(struct cpad_file_info));
	if (info == NULL) {
		return NULL;
	}

	size_t size = strlen(LIBCPER_EXAMPLES) + 1 + strlen(example_name) + 1 +
		      strlen(cpad_ext) + 1;
	info->cpad_out = (char *)malloc(size);
	if (snprintf(info->cpad_out, size, "%s/%s.%s", LIBCPER_EXAMPLES,
		     example_name, cpad_ext) != (int)size - 1) {
		free_cpad_file_info(info);
		return NULL;
	}

	size = strlen(LIBCPER_EXAMPLES) + 1 + strlen(example_name) + 1 +
	       strlen(json_ext) + 1;
	info->json_out = (char *)malloc(size);
	if (snprintf(info->json_out, size, "%s/%s.%s", LIBCPER_EXAMPLES,
		     example_name, json_ext) != (int)size - 1) {
		free_cpad_file_info(info);
		return NULL;
	}
	return info;
}

static int hex2int(char c)
{
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	return -1;
}

//Converts a hex text buffer (newlines ignored) into binary. Returns byte count.
static int string_to_binary(const char *source, size_t length,
			    unsigned char **retval)
{
	*retval = malloc(length);
	if (*retval == NULL) {
		return -1;
	}
	int uppernibble = 1;
	size_t ret_index = 0;
	for (size_t i = 0; i < length; i++) {
		char c = source[i];
		if (c == '\n') {
			continue;
		}
		int val = hex2int(c);
		if (val < 0) {
			printf("Invalid hex character in test file: %c at offset %zu\n",
			       c, i);
			return -1;
		}
		if (uppernibble) {
			(*retval)[ret_index] = (unsigned char)(val << 4);
		} else {
			(*retval)[ret_index] += (unsigned char)val;
			ret_index++;
		}
		uppernibble = !uppernibble;
	}
	return ret_index;
}

//Generates the committed example pair (binary hex + JSON) for the given CPAD type.
static void cpad_create_examples(const char *example_name, const char *type,
				 UINT16 action_id)
{
	struct cpad_file_info *info = cpad_file_info_init(example_name);
	assert(info != NULL);

	char *buf = NULL;
	size_t size = 0;
	FILE *record = generate_cpad_record_memstream(&type, &action_id, 1,
						      &buf, &size, 0);
	assert(record != NULL);

	//Write example CPAD to disk as hex (15 bytes per line).
	FILE *out = fopen(info->cpad_out, "wb");
	assert(out != NULL);
	for (size_t i = 0; i < size; i++) {
		fprintf(out, "%02x", (unsigned char)buf[i]);
		if (i % 15 == 14) {
			fwrite("\n", 1, 1, out);
		}
	}
	fclose(out);

	//Convert to IR and write JSON output.
	rewind(record);
	json_object *ir = cpad_to_ir(record);
	assert(ir != NULL);
	json_object_to_file_ext(info->json_out, ir, JSON_C_TO_STRING_PRETTY);

	json_object_put(ir);
	fclose(record);
	free(buf);
	free_cpad_file_info(info);
}

//Parses a committed example .cpadhex and asserts equality with its .json.
static void cpad_example_section_ir_test(const char *example_name)
{
	struct cpad_file_info *info = cpad_file_info_init(example_name);
	assert(info != NULL);

	FILE *cpad_file = fopen(info->cpad_out, "rb");
	assert(cpad_file != NULL);
	fseek(cpad_file, 0, SEEK_END);
	size_t length = ftell(cpad_file);
	fseek(cpad_file, 0, SEEK_SET);
	char *buffer = (char *)malloc(length);
	assert(buffer != NULL);
	assert(fread(buffer, 1, length, cpad_file) == length);
	fclose(cpad_file);

	unsigned char *cpad_bin = NULL;
	int cpad_bin_len = string_to_binary(buffer, length, &cpad_bin);
	assert(cpad_bin_len > 0);

	json_object *ir = cpad_buf_to_ir(cpad_bin, cpad_bin_len);
	assert(ir != NULL);

	json_object *expected = json_object_from_file(info->json_out);
	assert(expected != NULL);

	if (!json_object_equal(ir, expected)) {
		cper_print_log(
			"CPAD example mismatch for %s\nir: %s\nexpected: %s\n",
			example_name,
			json_object_to_json_string_ext(ir,
						       JSON_C_TO_STRING_PRETTY),
			json_object_to_json_string_ext(
				expected, JSON_C_TO_STRING_PRETTY));
		assert(0);
	}

	json_object_put(ir);
	json_object_put(expected);
	free(buffer);
	free(cpad_bin);
	free_cpad_file_info(info);
}

/*
 * IR-validity tests.
 */
static void cpad_log_section_ir_test(const char *type, UINT16 action_id,
				     int single_section)
{
	char *buf = NULL;
	size_t size = 0;
	FILE *record = generate_cpad_record_memstream(
		&type, &action_id, 1, &buf, &size, single_section);
	assert(record != NULL);

	json_object *ir = single_section ? cpad_single_section_to_ir(record) :
					   cpad_to_ir(record);
	fclose(record);
	free(buf);
	assert(ir != NULL);

	int valid = schema_validate_cpad_from_file(ir, single_section,
						   /*all_valid_bits*/ 1);
	json_object_put(ir);
	if (valid < 0) {
		printf("CPAD IR validation failed (single section mode = %d)\n",
		       single_section);
		assert(0);
	}
}

static void cpad_buf_log_section_ir_test(const char *type, UINT16 action_id,
					 int single_section)
{
	char *buf = NULL;
	size_t size = 0;
	FILE *record = generate_cpad_record_memstream(
		&type, &action_id, 1, &buf, &size, single_section);
	assert(record != NULL);

	json_object *ir =
		single_section ?
			cpad_buf_single_section_to_ir((UINT8 *)buf, size) :
			cpad_buf_to_ir((UINT8 *)buf, size);
	fclose(record);
	free(buf);
	assert(ir != NULL);

	int valid = schema_validate_cpad_from_file(ir, single_section,
						   /*all_valid_bits*/ 1);
	json_object_put(ir);
	if (valid < 0) {
		printf("CPAD buffer IR validation failed (single section mode = %d)\n",
		       single_section);
		assert(0);
	}
}

/*
 * Binary round-trip test.
 */
static void cpad_log_section_binary_test(const char *type, UINT16 action_id,
					 int single_section)
{
	char *buf = NULL;
	size_t size = 0;
	FILE *record = generate_cpad_record_memstream(
		&type, &action_id, 1, &buf, &size, single_section);
	assert(record != NULL);

	json_object *ir = single_section ? cpad_single_section_to_ir(record) :
					   cpad_to_ir(record);
	assert(ir != NULL);

	char *cpad_buf = NULL;
	size_t cpad_buf_size = 0;
	FILE *stream = open_memstream(&cpad_buf, &cpad_buf_size);
	if (single_section) {
		ir_single_section_to_cpad(ir, stream);
	} else {
		ir_to_cpad(ir, stream);
	}
	fclose(stream);

	assert(size == cpad_buf_size);
	assert(memcmp(buf, cpad_buf, size) == 0);

	fclose(record);
	free(buf);
	free(cpad_buf);
	json_object_put(ir);
}

static void cpad_log_section_dual_ir_test(const char *type, UINT16 action_id)
{
	cpad_log_section_ir_test(type, action_id, 0);
	cpad_log_section_ir_test(type, action_id, 1);
	cpad_buf_log_section_ir_test(type, action_id, 0);
	cpad_buf_log_section_ir_test(type, action_id, 1);
}

static void cpad_log_section_dual_binary_test(const char *type,
					      UINT16 action_id)
{
	cpad_log_section_binary_test(type, action_id, 0);
	cpad_log_section_binary_test(type, action_id, 1);
}

/*
 * Per-section tests.
 */
static void OsGenericCpadTests_IRValid(void)
{
	cpad_log_section_dual_ir_test("os-generic", CPAD_ACTION_REPLACE_PART);
	//Also exercise a proprietary action id.
	cpad_log_section_dual_ir_test("os-generic",
				      FIRST_PROPRIETARY_ACTION_ID);
}
static void OsGenericCpadTests_BinaryEqual(void)
{
	cpad_log_section_dual_binary_test("os-generic",
					  CPAD_ACTION_REPLACE_PART);
	cpad_log_section_dual_binary_test("os-generic",
					  FIRST_PROPRIETARY_ACTION_ID);
}

static void UnknownCpadTests_IRValid(void)
{
	cpad_log_section_dual_ir_test("unknown", CPAD_ACTION_DO_NOTHING);
}
static void UnknownCpadTests_BinaryEqual(void)
{
	cpad_log_section_dual_binary_test("unknown", CPAD_ACTION_DO_NOTHING);
}

//Full record with multiple sections.
static void MultiSectionCpadTests(void)
{
	const char *types[2] = { "os-generic", "os-generic" };
	UINT16 action_ids[2] = { CPAD_ACTION_REPLACE_PART,
				 CPAD_ACTION_POWER_CYCLE };
	char *buf = NULL;
	size_t size = 0;
	FILE *record = generate_cpad_record_memstream(types, action_ids, 2,
						      &buf, &size, 0);
	assert(record != NULL);

	json_object *ir = cpad_to_ir(record);
	assert(ir != NULL);
	assert(schema_validate_cpad_from_file(ir, 0, 1) >= 0);

	//Binary round-trip.
	char *cpad_buf = NULL;
	size_t cpad_buf_size = 0;
	FILE *stream = open_memstream(&cpad_buf, &cpad_buf_size);
	ir_to_cpad(ir, stream);
	fclose(stream);
	assert(size == cpad_buf_size);
	assert(memcmp(buf, cpad_buf, size) == 0);

	fclose(record);
	free(buf);
	free(cpad_buf);
	json_object_put(ir);
}

/*
 * Header validation tests (no CPER analog).
 */
static void CpadHeaderValidationTests(void)
{
	const char *type = "os-generic";
	UINT16 action_id = CPAD_ACTION_REPLACE_PART;
	char *buf = NULL;
	size_t size = 0;
	FILE *record = generate_cpad_record_memstream(&type, &action_id, 1,
						      &buf, &size, 0);
	assert(record != NULL);
	fclose(record);
	assert(size >= sizeof(CPAD_HEADER));

	//A freshly generated header must be valid.
	assert(cpad_header_valid(buf, size) == 1);

	//Buffer smaller than the header is invalid.
	assert(cpad_header_valid(buf, sizeof(CPAD_HEADER) - 1) == 0);

	//Corrupt the start signature (offset 0, 4 bytes).
	{
		char *bad = malloc(size);
		memcpy(bad, buf, size);
		bad[0] ^= 0xFF;
		assert(cpad_header_valid(bad, size) == 0);
		free(bad);
	}

	//Corrupt the end signature (offset 6, 4 bytes).
	{
		char *bad = malloc(size);
		memcpy(bad, buf, size);
		bad[6] ^= 0xFF;
		assert(cpad_header_valid(bad, size) == 0);
		free(bad);
	}

	//Zero the section count (offset 10, 2 bytes).
	{
		char *bad = malloc(size);
		memcpy(bad, buf, size);
		bad[10] = 0;
		bad[11] = 0;
		assert(cpad_header_valid(bad, size) == 0);
		free(bad);
	}

	free(buf);
}

/*
 * Action ID -> name rendering tests.
 */
static void CpadActionIdStringTests(void)
{
	assert(strcmp(action_to_string(CPAD_ACTION_DO_NOTHING), "Do Nothing") ==
	       0);
	assert(strcmp(action_to_string(CPAD_ACTION_REPLACE_PART),
		      "Replace Part") == 0);
	//Inject error is a defined action, not "Unknown".
	assert(strcmp(action_to_string(CPAD_ACTION_INJECT_ERROR),
		      "Inject Error") == 0);
	//Proprietary range.
	assert(strcmp(action_to_string(FIRST_PROPRIETARY_ACTION_ID),
		      "Proprietary Action") == 0);
	assert(strcmp(action_to_string(0xFFFF), "Proprietary Action") == 0);
	//Reserved/undefined standard action id (0x0007 - 0x7FFF).
	assert(strcmp(action_to_string(0x0007), "Unknown") == 0);
}

/*
 * Compile-time-style assertions over the section/generator tables.
 */
static void CpadCompileTimeAssertions_TwoWayConversion(void)
{
	for (size_t i = 0; i < cpad_section_definitions_len; i++) {
		if (cpad_section_definitions[i].ToCPAD != NULL) {
			assert(cpad_section_definitions[i].ToIR != NULL);
		}
		if (cpad_section_definitions[i].ToIR != NULL) {
			assert(cpad_section_definitions[i].ToCPAD != NULL);
		}
	}
}

static void CpadCompileTimeAssertions_ShortcodeNoSpaces(void)
{
	for (size_t i = 0; i < cpad_generator_definitions_len; i++) {
		for (int j = 0;
		     cpad_generator_definitions[i].ShortName[j + 1] != '\0';
		     j++) {
			assert(isspace(cpad_generator_definitions[i]
					       .ShortName[j]) == 0);
		}
	}
}

int main(void)
{
	if (GEN_CPAD_EXAMPLES) {
		cpad_create_examples("cpad-os-generic", "os-generic",
				     CPAD_ACTION_REPLACE_PART);
		cpad_create_examples("cpad-unknown", "unknown",
				     CPAD_ACTION_DO_NOTHING);
	}

	OsGenericCpadTests_IRValid();
	OsGenericCpadTests_BinaryEqual();
	UnknownCpadTests_IRValid();
	UnknownCpadTests_BinaryEqual();
	MultiSectionCpadTests();
	CpadHeaderValidationTests();
	CpadActionIdStringTests();
	CpadCompileTimeAssertions_TwoWayConversion();
	CpadCompileTimeAssertions_ShortcodeNoSpaces();

	//Golden example comparisons.
	cpad_example_section_ir_test("cpad-os-generic");
	cpad_example_section_ir_test("cpad-unknown");

	printf("\n\nCPAD tests completed successfully.\n");
	return 0;
}
