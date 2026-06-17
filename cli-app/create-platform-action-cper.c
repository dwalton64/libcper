/**
 * A user-space application that builds a Platform Action Event CPER from a
 * binary CPAD plus per-section Action Return / Reason codes.
 *
 * Author: drewwalton@microsoft.com
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <json.h>
#include <libcper/log.h>
#include <libcper/Cper.h>
#include <libcper/Cpad.h>
#include <libcper/cper-utils.h>
#include <libcper/cpad-parse.h>
#include <libcper/cper-parse.h>
#include <libcper/platform-action-cper.h>

//Maps a numeric code to a short command-line keyword. The friendly display
//name comes from the cper-utils *_to_string helpers.
typedef struct {
	UINT8 value;
	const char *keyword;
} CODE_KEYWORD;

static const CODE_KEYWORD return_code_keywords[] = {
	{ EFI_PLATFORM_ACTION_RETURN_CODE_SUCCESS, "success" },
	{ EFI_PLATFORM_ACTION_RETURN_CODE_FAILED, "failed" },
	{ EFI_PLATFORM_ACTION_RETURN_CODE_PENDING, "pending" },
	{ EFI_PLATFORM_ACTION_RETURN_CODE_POLICY_REJECTED, "policy-rejected" },
};

static const CODE_KEYWORD reason_code_keywords[] = {
	{ EFI_PLATFORM_ACTION_REASON_CODE_NONE, "none" },
	{ EFI_PLATFORM_ACTION_REASON_CODE_UNKNOWN_ERR, "unknown-error" },
	{ EFI_PLATFORM_ACTION_REASON_CODE_EP_NOT_READY, "endpoint-not-ready" },
	{ EFI_PLATFORM_ACTION_REASON_CODE_INVALID_CPAD, "invalid-cpad" },
	{ EFI_PLATFORM_ACTION_REASON_CODE_INCORRECT_EP, "incorrect-endpoint" },
	{ EFI_PLATFORM_ACTION_REASON_CODE_INVALID_FRUID, "invalid-fru-id" },
	{ EFI_PLATFORM_ACTION_REASON_CODE_UNSUPPORTED_ACTION,
	  "unsupported-action" },
	{ EFI_PLATFORM_ACTION_REASON_CODE_UNCONFIGURED_ACTION,
	  "unconfigured-action" },
	{ EFI_PLATFORM_ACTION_REASON_CODE_INVALID_ACTION_DATA,
	  "invalid-action-data" },
	{ EFI_PLATFORM_ACTION_REASON_CODE_TIMEOUT, "timeout" },
	{ EFI_PLATFORM_ACTION_REASON_CODE_DEPENDENCY_FAILED,
	  "dependency-failed" },
	{ EFI_PLATFORM_ACTION_REASON_CODE_PRECONDITION_FAILED,
	  "precondition-failed" },
};

static const size_t return_code_keywords_len =
	sizeof(return_code_keywords) / sizeof(return_code_keywords[0]);
static const size_t reason_code_keywords_len =
	sizeof(reason_code_keywords) / sizeof(reason_code_keywords[0]);

static void print_help(void);

//Parses a code value given as a keyword or a number (decimal or 0x hex).
//Returns 0 on success, -1 on failure.
static int parse_code(const char *text, const CODE_KEYWORD *table, size_t len,
		      UINT8 *out)
{
	if (text == NULL || text[0] == '\0') {
		return -1;
	}

	//Keyword match (case-insensitive).
	for (size_t i = 0; i < len; i++) {
		if (strcasecmp(text, table[i].keyword) == 0) {
			*out = table[i].value;
			return 0;
		}
	}

	//Numeric (decimal or 0x hex).
	if (isdigit((unsigned char)text[0])) {
		char *end = NULL;
		unsigned long value = strtoul(text, &end, 0);
		if (end != NULL && *end == '\0' && value <= 0xFF) {
			*out = (UINT8)value;
			return 0;
		}
	}
	return -1;
}

//Reads a whole file into a newly allocated buffer (caller frees *out_buf).
static int read_file(const char *path, unsigned char **out_buf,
		     size_t *out_size)
{
	FILE *file = fopen(path, "rb");
	if (file == NULL) {
		fprintf(stderr,
			"create-platform-action-cper: could not open '%s'.\n",
			path);
		return -1;
	}
	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (size < 0) {
		fclose(file);
		return -1;
	}
	unsigned char *buf = malloc(size);
	if (buf == NULL || fread(buf, 1, (size_t)size, file) != (size_t)size) {
		free(buf);
		fclose(file);
		fprintf(stderr,
			"create-platform-action-cper: could not read '%s'.\n",
			path);
		return -1;
	}
	fclose(file);
	*out_buf = buf;
	*out_size = (size_t)size;
	return 0;
}

//Returns the Action ID of the CPAD section at `index`, or 0 if out of range.
static UINT16 cpad_section_action_id(const unsigned char *buf, size_t size,
				     UINT32 index)
{
	size_t offset = sizeof(CPAD_HEADER) +
			(size_t)index * sizeof(CPAD_SECTION_DESCRIPTOR);
	if (offset + sizeof(CPAD_SECTION_DESCRIPTOR) > size) {
		return 0;
	}
	const CPAD_SECTION_DESCRIPTOR *descriptor =
		(const CPAD_SECTION_DESCRIPTOR *)(buf + offset);
	return descriptor->ActionID;
}

//Warns on stderr for return/reason codes that are not standard for their
//context. Per requirements the values are still written; vendor reason codes
//(0x80-0xFF) are accepted silently.
static void warn_on_unusual_codes(const PLATFORM_ACTION_EVENT_REQUEST *requests,
				  UINT16 num_requests)
{
	for (UINT16 i = 0; i < num_requests; i++) {
		UINT8 rc = requests[i].ReturnCode;
		UINT8 rrc = requests[i].ReasonCode;
		if (rc > EFI_PLATFORM_ACTION_RETURN_CODE_POLICY_REJECTED) {
			fprintf(stderr,
				"warning: action return code 0x%02X is reserved/undefined (writing it anyway)\n",
				rc);
		}
		if (!platform_action_reason_code_valid(rc, rrc)) {
			fprintf(stderr,
				"warning: reason code 0x%02X is not defined for return code 0x%02X (writing it anyway)\n",
				rrc, rc);
		}
	}
}

//Reads a line from stdin into buf (newline stripped). Returns 0 on success.
static int read_line(char *buf, size_t size)
{
	if (fgets(buf, (int)size, stdin) == NULL) {
		return -1;
	}
	buf[strcspn(buf, "\n")] = '\0';
	return 0;
}

static void print_code_list(FILE *stream, const char *title,
			    const CODE_KEYWORD *table, size_t len,
			    int is_reason)
{
	fprintf(stream, "%s\n", title);
	for (size_t i = 0; i < len; i++) {
		//Reason codes are named in the Failed context (where they all
		//have names); NONE/vendor are context-independent.
		const char *name =
			is_reason ?
				platform_action_reason_code_to_string(
					EFI_PLATFORM_ACTION_RETURN_CODE_FAILED,
					table[i].value) :
				platform_action_return_code_to_string(
					table[i].value);
		fprintf(stream, "    0x%02X  %-20s  %s\n", table[i].value,
			table[i].keyword, name);
	}
}

//Resolves interactive input to a code value. A bare decimal is treated as a
//1-based menu index (so the user never has to guess hex vs decimal); a
//keyword matches case-insensitively; a 0x-prefixed value is taken literally
//(used for vendor codes 0x80-0xFF). Returns 0 and sets *out on success.
static int resolve_menu_input(const char *line, const CODE_KEYWORD *table,
			      size_t len, UINT8 *out)
{
	if (line == NULL || line[0] == '\0') {
		return -1;
	}

	//Keyword.
	for (size_t i = 0; i < len; i++) {
		if (strcasecmp(line, table[i].keyword) == 0) {
			*out = table[i].value;
			return 0;
		}
	}

	//0x-prefixed literal value (e.g. a vendor code).
	if (line[0] == '0' && (line[1] == 'x' || line[1] == 'X')) {
		char *end = NULL;
		unsigned long value = strtoul(line, &end, 16);
		if (end != NULL && *end == '\0' && value <= 0xFF) {
			*out = (UINT8)value;
			return 0;
		}
		return -1;
	}

	//Bare decimal -> 1-based menu index.
	if (isdigit((unsigned char)line[0])) {
		char *end = NULL;
		unsigned long index = strtoul(line, &end, 10);
		if (end != NULL && *end == '\0' && index >= 1 && index <= len) {
			*out = table[index - 1].value;
			return 0;
		}
	}
	return -1;
}

//Prints a 1-based numbered menu of codes with their friendly names.
static void print_menu(FILE *stream, const char *title,
		       const CODE_KEYWORD *table, size_t len, int is_reason)
{
	fprintf(stream, "%s\n", title);
	for (size_t i = 0; i < len; i++) {
		const char *name =
			is_reason ?
				platform_action_reason_code_to_string(
					EFI_PLATFORM_ACTION_RETURN_CODE_FAILED,
					table[i].value) :
				platform_action_return_code_to_string(
					table[i].value);
		fprintf(stream, "    [%zu] %-20s %s\n", i + 1, table[i].keyword,
			name);
	}
}

//Interactively prompts for a return code (menu number, keyword, or 0x value).
static UINT8 prompt_return_code(void)
{
	char line[128];
	for (;;) {
		print_menu(stderr, "  Return codes:", return_code_keywords,
			   return_code_keywords_len, 0);
		fprintf(stderr,
			"  Select a return code (1-%zu, keyword, or 0x.. value): ",
			return_code_keywords_len);
		fflush(stderr);
		if (read_line(line, sizeof(line)) != 0) {
			exit(1);
		}
		UINT8 value = 0;
		if (resolve_menu_input(line, return_code_keywords,
				       return_code_keywords_len, &value) == 0) {
			return value;
		}
		fprintf(stderr, "  Invalid value '%s'. Please try again.\n",
			line);
	}
}

//Interactively prompts for a reason code valid for `return_code`. Only the
//applicable standard reasons are offered; vendor codes (0x80-0xFF) are always
//accepted. When NONE is the only standard option no menu is shown - the user
//presses Enter to accept it, or types a vendor code.
static UINT8 prompt_reason_code(UINT8 return_code)
{
	CODE_KEYWORD applicable[sizeof(reason_code_keywords) /
				sizeof(reason_code_keywords[0])];
	size_t count = 0;
	for (size_t i = 0; i < reason_code_keywords_len; i++) {
		if (platform_action_reason_code_valid(
			    return_code, reason_code_keywords[i].value)) {
			applicable[count++] = reason_code_keywords[i];
		}
	}

	char line[128];

	//Only NONE is a standard option: default to it, allow a vendor override.
	if (count <= 1) {
		for (;;) {
			fprintf(stderr,
				"  Reason code: \"%s\" is the only standard option.\n",
				platform_action_reason_code_to_string(
					return_code,
					EFI_PLATFORM_ACTION_REASON_CODE_NONE));
			fprintf(stderr,
				"  Press Enter to accept, or enter a vendor-specific code (0x80-0xFF): ");
			fflush(stderr);
			if (read_line(line, sizeof(line)) != 0) {
				exit(1);
			}
			if (line[0] == '\0') {
				return EFI_PLATFORM_ACTION_REASON_CODE_NONE;
			}
			UINT8 value = 0;
			if (resolve_menu_input(line, applicable, count,
					       &value) == 0 &&
			    platform_action_reason_code_valid(return_code,
							      value)) {
				return value;
			}
			fprintf(stderr,
				"  Invalid value '%s'. Please try again.\n",
				line);
		}
	}

	//Multiple standard reasons: show the filtered menu.
	for (;;) {
		print_menu(stderr, "  Reason codes:", applicable, count, 1);
		fprintf(stderr,
			"  Select a reason code (1-%zu, keyword, or a vendor code 0x80-0xFF): ",
			count);
		fflush(stderr);
		if (read_line(line, sizeof(line)) != 0) {
			exit(1);
		}
		UINT8 value = 0;
		if (resolve_menu_input(line, applicable, count, &value) == 0 &&
		    platform_action_reason_code_valid(return_code, value)) {
			return value;
		}
		fprintf(stderr, "  Invalid value '%s'. Please try again.\n",
			line);
	}
}

static int prompt_yes_no(const char *question, int default_yes)
{
	char line[32];
	fprintf(stderr, "%s [%s]: ", question, default_yes ? "Y/n" : "y/N");
	fflush(stderr);
	if (read_line(line, sizeof(line)) != 0) {
		return default_yes;
	}
	if (line[0] == '\0') {
		return default_yes;
	}
	return line[0] == 'y' || line[0] == 'Y';
}

//Builds the request list interactively. Returns the number of requests (0 on
//cancel) and fills *out_requests (caller frees).
static UINT16 build_requests_interactive(const unsigned char *cpad_buf,
					 size_t cpad_size, UINT16 section_count,
					 PLATFORM_ACTION_EVENT_REQUEST **out)
{
	PLATFORM_ACTION_EVENT_REQUEST *requests =
		calloc(section_count, sizeof(*requests));
	if (requests == NULL) {
		return 0;
	}

	int uniform = prompt_yes_no(
		"Use the same return/reason code for every included section?",
		0);
	UINT8 uniform_rc = 0;
	UINT8 uniform_rrc = 0;
	if (uniform) {
		uniform_rc = prompt_return_code();
		uniform_rrc = prompt_reason_code(uniform_rc);
	}

	UINT16 count = 0;
	for (UINT16 i = 0; i < section_count; i++) {
		UINT16 action_id =
			cpad_section_action_id(cpad_buf, cpad_size, i);
		char question[160];
		snprintf(
			question, sizeof(question),
			"Emit a Platform Action Event for section %u (Action ID 0x%04X - %s)?",
			(unsigned)i, (unsigned)action_id,
			action_to_string(action_id));
		if (!prompt_yes_no(question, 1)) {
			continue;
		}
		requests[count].CpadSectionIndex = i;
		if (uniform) {
			requests[count].ReturnCode = uniform_rc;
			requests[count].ReasonCode = uniform_rrc;
		} else {
			requests[count].ReturnCode = prompt_return_code();
			requests[count].ReasonCode =
				prompt_reason_code(requests[count].ReturnCode);
		}
		count++;
	}

	*out = requests;
	return count;
}

//Outputs the built CPER either as binary (to out_path) or as JSON.
static int output_cper(const unsigned char *cper_buf, size_t cper_size,
		       const char *out_path, int json_mode)
{
	if (json_mode) {
		json_object *ir = cper_buf_to_ir(cper_buf, cper_size);
		if (ir == NULL) {
			fprintf(stderr,
				"create-platform-action-cper: failed to convert output to JSON.\n");
			return -1;
		}
		const char *text = json_object_to_json_string_ext(
			ir, JSON_C_TO_STRING_PRETTY);
		if (out_path != NULL) {
			FILE *f = fopen(out_path, "w");
			if (f == NULL) {
				json_object_put(ir);
				return -1;
			}
			fprintf(f, "%s\n", text);
			fclose(f);
		} else {
			printf("%s\n", text);
		}
		json_object_put(ir);
		return 0;
	}

	//Binary output requires --out.
	if (out_path == NULL) {
		fprintf(stderr,
			"create-platform-action-cper: --out is required for binary output (or use --json).\n");
		return -1;
	}
	FILE *f = fopen(out_path, "wb");
	if (f == NULL) {
		fprintf(stderr,
			"create-platform-action-cper: could not open output '%s'.\n",
			out_path);
		return -1;
	}
	fwrite(cper_buf, 1, cper_size, f);
	fclose(f);
	return 0;
}

int main(int argc, char **argv)
{
	cper_set_log_stdio();
	if (argc == 2 && strcmp(argv[1], "--help") == 0) {
		print_help();
		return 0;
	}

	const char *cpad_path = NULL;
	const char *out_path = NULL;
	int json_mode = 0;
	int have_return_code = 0, have_reason_code = 0;
	UINT8 uniform_return_code = 0, uniform_reason_code = 0;

	//Per-section requests (from --section) and exclude indices.
	PLATFORM_ACTION_EVENT_REQUEST *section_requests = NULL;
	UINT16 section_request_count = 0;
	UINT32 *exclude = NULL;
	size_t exclude_count = 0;

	int status = 1;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--help") == 0) {
			print_help();
			status = 0;
			goto cleanup;
		} else if (strcmp(argv[i], "--json") == 0) {
			json_mode = 1;
		} else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
			out_path = argv[++i];
		} else if (strcmp(argv[i], "--return-code") == 0 &&
			   i + 1 < argc) {
			if (parse_code(argv[++i], return_code_keywords,
				       return_code_keywords_len,
				       &uniform_return_code) != 0) {
				fprintf(stderr, "Invalid return code '%s'.\n",
					argv[i]);
				goto cleanup;
			}
			have_return_code = 1;
		} else if (strcmp(argv[i], "--reason-code") == 0 &&
			   i + 1 < argc) {
			if (parse_code(argv[++i], reason_code_keywords,
				       reason_code_keywords_len,
				       &uniform_reason_code) != 0) {
				fprintf(stderr, "Invalid reason code '%s'.\n",
					argv[i]);
				goto cleanup;
			}
			have_reason_code = 1;
		} else if (strcmp(argv[i], "--exclude-section") == 0 &&
			   i + 1 < argc) {
			exclude = realloc(exclude, (exclude_count + 1) *
							   sizeof(*exclude));
			exclude[exclude_count++] =
				(UINT32)strtoul(argv[++i], NULL, 0);
		} else if (strcmp(argv[i], "--exclude-sections") == 0) {
			while (i + 1 < argc && argv[i + 1][0] != '-') {
				exclude = realloc(exclude,
						  (exclude_count + 1) *
							  sizeof(*exclude));
				exclude[exclude_count++] =
					(UINT32)strtoul(argv[++i], NULL, 0);
			}
		} else if (strcmp(argv[i], "--section") == 0 && i + 1 < argc) {
			//Format: index:return-code:reason-code
			char spec[128];
			strncpy(spec, argv[++i], sizeof(spec) - 1);
			spec[sizeof(spec) - 1] = '\0';
			char *idx_str = strtok(spec, ":");
			char *rc_str = strtok(NULL, ":");
			char *rrc_str = strtok(NULL, ":");
			UINT8 rc = 0, rrc = 0;
			if (idx_str == NULL || rc_str == NULL ||
			    rrc_str == NULL ||
			    parse_code(rc_str, return_code_keywords,
				       return_code_keywords_len, &rc) != 0 ||
			    parse_code(rrc_str, reason_code_keywords,
				       reason_code_keywords_len, &rrc) != 0) {
				fprintf(stderr,
					"Invalid --section '%s'. Expected index:return-code:reason-code.\n",
					argv[i]);
				goto cleanup;
			}
			section_requests =
				realloc(section_requests,
					(section_request_count + 1) *
						sizeof(*section_requests));
			section_requests[section_request_count]
				.CpadSectionIndex =
				(UINT32)strtoul(idx_str, NULL, 0);
			section_requests[section_request_count].ReturnCode = rc;
			section_requests[section_request_count].ReasonCode =
				rrc;
			section_request_count++;
		} else if (argv[i][0] == '-') {
			fprintf(stderr,
				"Unrecognised argument '%s'. See 'create-platform-action-cper --help'.\n",
				argv[i]);
			goto cleanup;
		} else if (cpad_path == NULL) {
			cpad_path = argv[i];
		} else {
			fprintf(stderr, "Too many path arguments.\n");
			goto cleanup;
		}
	}

	if (cpad_path == NULL) {
		fprintf(stderr,
			"No CPAD file provided. See 'create-platform-action-cper --help'.\n");
		goto cleanup;
	}

	//Enforce mutually-exclusive modes.
	int per_section_mode = section_request_count > 0;
	int uniform_mode = have_return_code || have_reason_code ||
			   exclude_count > 0;
	if (per_section_mode && uniform_mode) {
		fprintf(stderr,
			"--section cannot be combined with --return-code/--reason-code/--exclude-section(s).\n");
		goto cleanup;
	}
	if (have_return_code != have_reason_code) {
		fprintf(stderr,
			"--return-code and --reason-code must be used together.\n");
		goto cleanup;
	}

	//Read and validate the CPAD.
	unsigned char *cpad_buf = NULL;
	size_t cpad_size = 0;
	if (read_file(cpad_path, &cpad_buf, &cpad_size) != 0) {
		goto cleanup;
	}
	if (!cpad_header_valid((const char *)cpad_buf, cpad_size)) {
		fprintf(stderr,
			"create-platform-action-cper: '%s' is not a valid CPAD.\n",
			cpad_path);
		free(cpad_buf);
		goto cleanup;
	}
	UINT16 section_count = ((const CPAD_HEADER *)cpad_buf)->SectionCount;

	//Build the request list according to the selected mode.
	PLATFORM_ACTION_EVENT_REQUEST *requests = NULL;
	UINT16 num_requests = 0;

	if (per_section_mode) {
		requests = section_requests;
		num_requests = section_request_count;
		section_requests = NULL;
	} else if (uniform_mode) {
		requests = calloc(section_count, sizeof(*requests));
		for (UINT16 i = 0; i < section_count; i++) {
			int excluded = 0;
			for (size_t e = 0; e < exclude_count; e++) {
				if (exclude[e] == i) {
					excluded = 1;
					break;
				}
			}
			if (excluded) {
				continue;
			}
			requests[num_requests].CpadSectionIndex = i;
			requests[num_requests].ReturnCode = uniform_return_code;
			requests[num_requests].ReasonCode = uniform_reason_code;
			num_requests++;
		}
	} else {
		num_requests = build_requests_interactive(
			cpad_buf, cpad_size, section_count, &requests);
	}

	if (num_requests == 0) {
		fprintf(stderr, "No sections selected; nothing to emit.\n");
		free(requests);
		free(cpad_buf);
		goto cleanup;
	}

	warn_on_unusual_codes(requests, num_requests);

	//Build the CPER into memory, then write it out.
	char *cper_buf = NULL;
	size_t cper_size = 0;
	FILE *mem = open_memstream(&cper_buf, &cper_size);
	int build_ret = cpad_to_platform_action_event_cper(
		cpad_buf, cpad_size, requests, num_requests, mem);
	fclose(mem);

	if (build_ret != 0) {
		fprintf(stderr,
			"create-platform-action-cper: failed to build the Platform Action Event CPER (check section indices).\n");
	} else if (output_cper((const unsigned char *)cper_buf, cper_size,
			       out_path, json_mode) == 0) {
		status = 0;
	}

	free(cper_buf);
	free(requests);
	free(cpad_buf);

cleanup:
	free(section_requests);
	free(exclude);
	return status;
}

static void print_help(void)
{
	printf(":: create-platform-action-cper <cpad-file> [--out <file>] [--json]\n");
	printf("       [--return-code <code> --reason-code <code> [--exclude-sections <i> ...]]\n");
	printf("       [--section <index>:<return-code>:<reason-code> ...]\n\n");
	printf("\tBuilds a Platform Action Event CPER from a binary CPAD. One Platform Action\n");
	printf("\tEvent section is emitted per selected CPAD section.\n\n");
	printf("\tModes (mutually exclusive):\n");
	printf("\t  Uniform:      --return-code/--reason-code apply to every section;\n");
	printf("\t                --exclude-sections omits the listed (0-based) sections.\n");
	printf("\t  Per-section:  repeatable --section <index>:<return-code>:<reason-code>\n");
	printf("\t                emits exactly the listed sections.\n");
	printf("\t  Interactive:  with no code flags, prompts for each section.\n\n");
	printf("\tOutput defaults to binary and requires --out; --json emits the IR instead\n");
	printf("\t(to --out or stdout). Codes may be a keyword or a number (decimal or 0x hex).\n\n");

	print_code_list(stdout,
			"    Action Return Codes:", return_code_keywords,
			return_code_keywords_len, 0);
	printf("\n");
	print_code_list(stdout,
			"    Action Return Reason Codes (Failed context):",
			reason_code_keywords, reason_code_keywords_len, 1);
	printf("\n:: --help\n");
	printf("\tDisplays this help information.\n");
}
