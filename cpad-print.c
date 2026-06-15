/**
 * Human-readable printing of CPAD records, used by the `lscpad` tool.
 *
 * Author: drewwalton@microsoft.com
 **/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <libcper/Cpad.h>
#include <libcper/cper-utils.h>
#include <libcper/sections/cpad-section.h>
#include <libcper/cpad-print.h>

#define CPAD_VALID_FLAG_NOT_SET "-- Valid Flag Not Set --"

//Width of the left-justified label column, wide enough for the longest label.
#define CPAD_LABEL_WIDTH 13

//Prints a "label: value" line, substituting the not-set message when the
//field's validation bit is clear.
static void print_field(FILE *out, const char *label, int valid,
			const char *value)
{
	fprintf(out, "  %-*s%s\n", CPAD_LABEL_WIDTH, label,
		valid ? value : CPAD_VALID_FLAG_NOT_SET);
}

//Formats a GUID into the provided buffer, which must be at least
//GUID_STRING_LENGTH + 1 bytes. The GUID is copied to an aligned local first so
//that callers may pass a pointer into a packed structure.
static void guid_to_buf(char *buf, size_t buf_len, EFI_GUID guid)
{
	guid_to_string(buf, buf_len, &guid);
}

//Copies up to `fru_len` bytes of a (possibly non-NUL-terminated) FRU string
//into `buf`, replacing non-printable characters with '?'.
static void fru_text_to_buf(char *buf, size_t buf_len, const CHAR8 *fru,
			    size_t fru_len)
{
	size_t i = 0;
	for (; i < fru_len && i < buf_len - 1; i++) {
		char c = (char)fru[i];
		if (c == '\0') {
			break;
		}
		buf[i] = isprint((unsigned char)c) ? c : '?';
	}
	buf[i] = '\0';
}

static void print_header(FILE *out, const CPAD_HEADER *header)
{
	UINT32 valid = header->ValidationBits;
	char guid_str[GUID_STRING_LENGTH + 1];
	char num_str[CPAD_UINT64_HEX_STRING_LEN];

	fprintf(out, "CPAD Header\n");

	//Timestamp (validation bit gated).
	int ts_valid = valid & (1u << CPAD_HEADER_TIME_STAMP_VALID);
	char ts_str[TIMESTAMP_LENGTH] = { 0 };
	if (ts_valid) {
		EFI_ERROR_TIME_STAMP timestamp = header->TimeStamp;
		timestamp_to_string(ts_str, TIMESTAMP_LENGTH, &timestamp);
	}
	print_field(out, "Timestamp:", ts_valid, ts_str);

	//PlatformID (validation bit gated).
	int platform_valid = valid & (1u << CPAD_HEADER_PLATFORM_ID_VALID);
	if (platform_valid) {
		guid_to_buf(guid_str, sizeof(guid_str), header->PlatformID);
	}
	print_field(out, "PlatformID:", platform_valid, guid_str);

	//PartitionID (validation bit gated).
	int partition_valid = valid & (1u << CPAD_HEADER_PARTITION_ID_VALID);
	if (partition_valid) {
		guid_to_buf(guid_str, sizeof(guid_str), header->PartitionID);
	}
	print_field(out, "PartitionID:", partition_valid, guid_str);

	//CreatorID, RecordID and Flags have no validation bit; always shown.
	guid_to_buf(guid_str, sizeof(guid_str), header->CreatorID);
	print_field(out, "CreatorID:", 1, guid_str);

	snprintf(num_str, sizeof(num_str), "0x%016llX",
		 (unsigned long long)header->RecordID);
	print_field(out, "RecordID:", 1, num_str);

	snprintf(num_str, sizeof(num_str), "0x%08X", (unsigned)header->Flags);
	print_field(out, "Flags:", 1, num_str);
}

static void print_section_descriptor(FILE *out, int index,
				     const CPAD_SECTION_DESCRIPTOR *descriptor)
{
	UINT8 valid = descriptor->SecValidMask;
	char guid_str[GUID_STRING_LENGTH + 1];
	char value_str[GUID_STRING_LENGTH + 64];

	fprintf(out, "\nCPAD Section Descriptor [%d]\n", index);

	//SectionType (always present): GUID plus a readable name when known.
	EFI_GUID section_type = descriptor->SectionType;
	guid_to_buf(guid_str, sizeof(guid_str), section_type);
	CPAD_SECTION_DEFINITION *definition =
		cpad_select_section_by_guid(&section_type);
	snprintf(value_str, sizeof(value_str), "%s (%s)", guid_str,
		 definition != NULL ? definition->ReadableName : "Unknown");
	print_field(out, "SectionType:", 1, value_str);

	//FruID (validation bit gated).
	int fru_id_valid = valid & (1u << CPAD_SECTION_FRU_ID_VALID);
	if (fru_id_valid) {
		guid_to_buf(guid_str, sizeof(guid_str), descriptor->FruId);
	}
	print_field(out, "FruID:", fru_id_valid, guid_str);

	//Urgency (validation bit gated).
	int urgency_valid = valid & (1u << CPAD_SECTION_URGENCY_VALID);
	snprintf(value_str, sizeof(value_str), "%u",
		 (unsigned)descriptor->Urgency);
	print_field(out, "Urgency:", urgency_valid, value_str);

	//Confidence (validation bit gated).
	int confidence_valid = valid & (1u << CPAD_SECTION_CONFIDENCE_VALID);
	snprintf(value_str, sizeof(value_str), "%u",
		 (unsigned)descriptor->Confidence);
	print_field(out, "Confidence:", confidence_valid, value_str);

	//FruText (validation bit gated).
	int fru_text_valid = valid & (1u << CPAD_SECTION_FRU_STRING_VALID);
	char fru_text[sizeof(descriptor->FruString) + 1];
	if (fru_text_valid) {
		fru_text_to_buf(fru_text, sizeof(fru_text),
				descriptor->FruString,
				sizeof(descriptor->FruString));
	}
	print_field(out, "FruText:", fru_text_valid, fru_text);

	//Action ID (always present): numeric value and descriptive string.
	snprintf(value_str, sizeof(value_str), "0x%04X (%s)",
		 (unsigned)descriptor->ActionID,
		 action_to_string(descriptor->ActionID));
	print_field(out, "Action ID:", 1, value_str);
}

int cpad_print_record(const char *filename, const unsigned char *buf,
		      size_t size, FILE *out)
{
	fprintf(out, "==> %s <==\n\n", filename);

	if (size < sizeof(CPAD_HEADER)) {
		fprintf(out, "  (invalid: record smaller than CPAD header)\n");
		return -1;
	}

	const CPAD_HEADER *header = (const CPAD_HEADER *)buf;
	print_header(out, header);

	const unsigned char *pos = buf + sizeof(CPAD_HEADER);
	size_t remaining = size - sizeof(CPAD_HEADER);
	for (UINT16 i = 0; i < header->SectionCount; i++) {
		if (remaining < sizeof(CPAD_SECTION_DESCRIPTOR)) {
			fprintf(out,
				"\n  (truncated: section descriptor %u does not fit in the record)\n",
				(unsigned)i);
			return -1;
		}

		const CPAD_SECTION_DESCRIPTOR *descriptor =
			(const CPAD_SECTION_DESCRIPTOR *)pos;
		print_section_descriptor(out, i, descriptor);

		pos += sizeof(CPAD_SECTION_DESCRIPTOR);
		remaining -= sizeof(CPAD_SECTION_DESCRIPTOR);
	}

	return 0;
}
