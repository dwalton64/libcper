#ifndef CPAD_PRINT_H
#define CPAD_PRINT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stddef.h>

//Prints a human-readable summary of the CPAD record in `buf` to `out`,
//preceded by `filename` as a heading. Header and section-descriptor fields
//that are gated by a validation bit print "-- Valid Flag Not Set --" when the
//bit is clear. Returns 0 on success, or a negative value if the record is too
//small or a section descriptor does not fit within `size`.
int cpad_print_record(const char *filename, const unsigned char *buf,
		      size_t size, FILE *out);

#ifdef __cplusplus
}
#endif

#endif
