/**
 * A user-space application that lists the contents of CPAD (Common Platform
 * Action Descriptor) files in a human-readable form, or as JSON.
 *
 * Author: drewwalton@microsoft.com
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <json.h>
#include <libcper/log.h>
#include <libcper/Cpad.h>
#include <libcper/cpad-parse.h>
#include <libcper/cpad-print.h>

static void print_help(void);
static int process_path(const char *path, int json_mode);

int main(int argc, char **argv)
{
	cper_set_log_stdio();

	const char *path = NULL;
	int json_mode = 0;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--help") == 0) {
			print_help();
			return 0;
		} else if (strcmp(argv[i], "--json") == 0) {
			json_mode = 1;
		} else if (argv[i][0] == '-') {
			fprintf(stderr,
				"Unrecognised argument '%s'. See 'lscpad --help' for command information.\n",
				argv[i]);
			return 1;
		} else if (path == NULL) {
			//The first non-flag argument is the path.
			path = argv[i];
		} else {
			fprintf(stderr,
				"Too many path arguments. See 'lscpad --help' for command information.\n");
			return 1;
		}
	}

	//With no path argument, default to the current directory.
	return process_path(path != NULL ? path : ".", json_mode);
}

//Returns 1 if `name` ends with the ".cpad" extension.
static int has_cpad_extension(const char *name)
{
	size_t len = strlen(name);
	const char *ext = ".cpad";
	size_t ext_len = strlen(ext);
	return len >= ext_len && strcmp(name + len - ext_len, ext) == 0;
}

//Reads the entire file at `path` into a newly allocated buffer. Returns 0 on
//success (caller frees *out_buf), or -1 on failure.
static int read_file(const char *path, unsigned char **out_buf,
		     size_t *out_size)
{
	FILE *file = fopen(path, "rb");
	if (file == NULL) {
		fprintf(stderr, "lscpad: could not open '%s'.\n", path);
		return -1;
	}

	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (size < 0) {
		fprintf(stderr, "lscpad: could not determine size of '%s'.\n",
			path);
		fclose(file);
		return -1;
	}

	unsigned char *buf = malloc(size);
	if (buf == NULL) {
		fclose(file);
		return -1;
	}
	if (fread(buf, 1, (size_t)size, file) != (size_t)size) {
		fprintf(stderr, "lscpad: could not read '%s'.\n", path);
		free(buf);
		fclose(file);
		return -1;
	}
	fclose(file);

	*out_buf = buf;
	*out_size = (size_t)size;
	return 0;
}

//Lists a single CPAD file. Returns 0 on success, -1 on failure.
static int process_file(const char *path, int json_mode)
{
	unsigned char *buf = NULL;
	size_t size = 0;
	if (read_file(path, &buf, &size) != 0) {
		return -1;
	}

	if (!cpad_header_valid((const char *)buf, size)) {
		fprintf(stderr, "lscpad: '%s' is not a valid CPAD file.\n",
			path);
		free(buf);
		return -1;
	}

	int ret = 0;
	if (json_mode) {
		json_object *ir = cpad_buf_to_ir(buf, size);
		if (ir == NULL) {
			fprintf(stderr,
				"lscpad: could not parse '%s' to JSON.\n",
				path);
			ret = -1;
		} else {
			printf("==> %s <==\n%s\n", path,
			       json_object_to_json_string_ext(
				       ir, JSON_C_TO_STRING_PRETTY));
			json_object_put(ir);
		}
	} else {
		ret = cpad_print_record(path, buf, size, stdout);
	}

	free(buf);
	return ret;
}

static int compare_strings(const void *a, const void *b)
{
	const char *const *sa = a;
	const char *const *sb = b;
	return strcmp(*sa, *sb);
}

//Lists every ".cpad" file in `dir_path`, alphabetically, non-recursively.
//Returns 0 if all files were listed successfully, -1 otherwise.
static int process_directory(const char *dir_path, int json_mode)
{
	DIR *dir = opendir(dir_path);
	if (dir == NULL) {
		fprintf(stderr, "lscpad: could not open directory '%s'.\n",
			dir_path);
		return -1;
	}

	//Collect the matching file names so they can be sorted.
	char **names = NULL;
	size_t count = 0;
	size_t capacity = 0;
	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		if (!has_cpad_extension(entry->d_name)) {
			continue;
		}
		if (count == capacity) {
			capacity = capacity == 0 ? 16 : capacity * 2;
			char **grown =
				realloc(names, capacity * sizeof(*names));
			if (grown == NULL) {
				break;
			}
			names = grown;
		}
		names[count] = strdup(entry->d_name);
		if (names[count] == NULL) {
			break;
		}
		count++;
	}
	closedir(dir);

	if (count == 0) {
		printf("No .cpad files found in '%s'.\n", dir_path);
		free(names);
		return 0;
	}

	qsort(names, count, sizeof(*names), compare_strings);

	int ret = 0;
	for (size_t i = 0; i < count; i++) {
		char full_path[PATH_MAX];
		snprintf(full_path, sizeof(full_path), "%s/%s", dir_path,
			 names[i]);
		if (i > 0) {
			printf("\n");
		}
		if (process_file(full_path, json_mode) != 0) {
			ret = -1;
		}
		free(names[i]);
	}
	free(names);
	return ret;
}

//Dispatches based on whether `path` is a directory or a file.
static int process_path(const char *path, int json_mode)
{
	struct stat path_stat;
	if (stat(path, &path_stat) != 0) {
		fprintf(stderr, "lscpad: cannot access '%s'.\n", path);
		return 1;
	}

	int ret;
	if (S_ISDIR(path_stat.st_mode)) {
		ret = process_directory(path, json_mode);
	} else {
		ret = process_file(path, json_mode);
	}
	return ret == 0 ? 0 : 1;
}

static void print_help(void)
{
	printf(":: lscpad [path] [--json]\n");
	printf("\tLists the contents of CPAD (Common Platform Action Descriptor) files.\n\n");
	printf("\tThe optional 'path' argument (the default argument, no flag required) selects what to list:\n");
	printf("\t  - omitted:      every '.cpad' file in the current directory\n");
	printf("\t  - a directory:  every '.cpad' file in that directory (non-recursive, sorted)\n");
	printf("\t  - a file:       that single CPAD file\n\n");
	printf("\tFor each file the CPAD header and section descriptors are printed. Header and\n");
	printf("\tsection-descriptor fields whose validation bit is not set are shown as\n");
	printf("\t'-- Valid Flag Not Set --'.\n\n");
	printf("\tIf '--json' is set, each CPAD is converted to JSON and printed instead.\n\n");
	printf(":: --help\n");
	printf("\tDisplays help information to the console.\n");
}
