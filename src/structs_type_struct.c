/*******************************************************************************
 * HEADERS
 ******************************************************************************/

/* Standard Includes */
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

/* Module Includes */
#include "structs.h"
#include "structs_type_array.h"
#include "structs_type_struct.h"

/*******************************************************************************
 * STRUCTURE TYPES
 ******************************************************************************/

#define NUM_BYTES(x) (((x) + 7) / 8)

int structs_struct_init(const struct structs_type *type, void *data)
{
	const struct structs_field *field;

	/* Make sure it's really a structure type */
	if (type->tclass != STRUCTS_TYPE_STRUCTURE) {
		errno = EINVAL;
		return (-1);
	}

	/* Initialize each field */
	memset(data, 0, type->size);
	for (field = type->args[0].v; field->name != NULL; field++) {
		assert(field->size == field->type->size);	/* safety check */
		if ((*field->type->init) (field->type,
					  (char *)data + field->offset) == -1)
			break;
	}

	/* If there was a failure, clean up */
	if (field->name != NULL) {
		while (field-- != type->args[0].v) {
			(*field->type->uninit) (field->type,
						(char *)data + field->offset);
		}
		memset((char *)data, 0, type->size);
		return (-1);
	}
	return (0);
}

int structs_struct_copy(const struct structs_type *type,
			const void *from, void *to)
{
	const struct structs_field *field;

	/* Make sure it's really a structure type */
	if (type->tclass != STRUCTS_TYPE_STRUCTURE) {
		errno = EINVAL;
		return (-1);
	}

	/* Copy each field */
	memset(to, 0, type->size);
	for (field = type->args[0].v; field->name != NULL; field++) {
		const void *const fdata = (char *)from + field->offset;
		void *const tdata = (char *)to + field->offset;

		if ((*field->type->copy) (field->type, fdata, tdata) == -1)
			break;
	}

	/* If there was a failure, clean up */
	if (field->name != NULL) {
		while (field-- != type->args[0].v) {
			(*field->type->uninit) (field->type,
						(char *)to + field->offset);
		}
		memset((char *)to, 0, type->size);
	}
	return (0);
}

int structs_struct_equal(const struct structs_type *type,
			 const void *v1, const void *v2)
{
	const struct structs_field *field;

	/* Make sure it's really a structure type */
	if (type->tclass != STRUCTS_TYPE_STRUCTURE)
		return (0);

	/* Compare all fields */
	for (field = type->args[0].v; field->name != NULL; field++) {
		const void *const data1 = (char *)v1 + field->offset;
		const void *const data2 = (char *)v2 + field->offset;

		if (!(*field->type->equal) (field->type, data1, data2))
			return (0);
	}
	return (1);
}

int structs_struct_encode(const struct structs_type *type,
			  struct structs_data *code, const void *data)
{
	struct structs_data *fcodes;
	unsigned int nfields;
	unsigned int bitslen;
	unsigned char *bits;
	int r = -1;
	unsigned int tlen;
	unsigned int i;

	/* Count number of fields */
	for (nfields = 0;
	     ((struct structs_field *)type->args[0].v)[nfields].name != NULL;
	     nfields++) ;

	/* Create bit array. Each bit indicates a field as being present. */
	bitslen = NUM_BYTES(nfields);
	if ((bits = calloc(1, bitslen)) == NULL)
		return (-1);
	memset(bits, 0, bitslen);
	tlen = bitslen;

	/* Create array of individual encodings, one per field */
	if ((fcodes = calloc(1, nfields * sizeof(*fcodes))) == NULL)
		goto fail1;
	for (i = 0; i < nfields; i++) {
		const struct structs_field *const field
			= (struct structs_field *)type->args[0].v + i;
		const void *const fdata = (char *)data + field->offset;
		struct structs_data *const fcode = &fcodes[i];
		void *dval;
		int equal;

		/* Compare this field to the default value */
		if ((dval = calloc(1, field->type->size)) == NULL)
			goto fail2;
		if (structs_init(field->type, NULL, dval) == -1) {
			free(dval);
			goto fail2;
		}
		equal = (*field->type->equal) (field->type, fdata, dval);
		structs_free(field->type, NULL, dval);
		free(dval);

		/* Omit field if value equals default value */
		if (equal == 1) {
			memset(fcode, 0, sizeof(*fcode));
			continue;
		}
		bits[i / 8] |= (1 << (i % 8));

		/* Encode this field */
		if ((*field->type->encode) (field->type, fcode,
					    (char *)data + field->offset) == -1)
			goto fail2;
		tlen += fcode->length;
	}

	/* Allocate final encoded region */
	if ((code->data = calloc(1, tlen)) == NULL)
		goto done;

	/* Copy bits */
	memcpy(code->data, bits, bitslen);
	code->length = bitslen;

	/* Copy encoded fields */
	for (i = 0; i < nfields; i++) {
		struct structs_data *const fcode = &fcodes[i];

		memcpy(code->data + code->length, fcode->data, fcode->length);
		code->length += fcode->length;
	}

	/* OK */
	r = 0;

done:
	/* Clean up and exit */
fail2:	while (i-- > 0)
		free(fcodes[i].data);
	free(fcodes);
fail1:	free(bits);
	return (r);
}

int structs_struct_decode(const struct structs_type *type,
			  const unsigned char *code, size_t cmax, void *data,
			  char *ebuf, size_t emax)
{
	const unsigned char *bits;
	unsigned int nfields;
	unsigned int bitslen;
	unsigned int clen;
	unsigned int i;

	/* Count number of fields */
	for (nfields = 0;
	     ((struct structs_field *)type->args[0].v)[nfields].name != NULL;
	     nfields++) ;

	/* Get bits array */
	bitslen = NUM_BYTES(nfields);
	if (cmax < bitslen) {
		strncpy(ebuf, "encoded structure is truncated", emax);
		errno = EINVAL;
		return (-1);
	}
	bits = code;
	code += bitslen;
	cmax -= bitslen;
	clen = bitslen;

	/* Decode fields */
	for (i = 0; i < nfields; i++) {
		const struct structs_field *const field
			= (struct structs_field *)type->args[0].v + i;
		void *const fdata = (char *)data + field->offset;
		int fclen;

		/* If field not present, assign it the default value */
		if ((bits[i / 8] & (1 << (i % 8))) == 0) {
			if (structs_init(field->type, NULL, fdata) == -1)
				goto fail;
			continue;
		}

		/* Decode field */
		if ((fclen = (*field->type->decode) (field->type,
						     code, cmax, fdata, ebuf,
						     emax)) == -1)
			goto fail;

		/* Go to next encoded field */
		code += fclen;
		cmax -= fclen;
		clen += fclen;
		continue;

		/* Un-do work done so far */
	fail:		while (i-- > 0) {
			const struct structs_field *const field2
				= (struct structs_field *)type->args[0].v + i;
			void *const fdata2 = (char *)data + field2->offset;

			structs_free(field2->type, NULL, fdata2);
		}
		return (-1);
	}

	/* Done */
	return (clen);
}

void structs_struct_free(const struct structs_type *type, void *data)
{
	const struct structs_field *field;

	/* Make sure it's really a structure type */
	if (type->tclass != STRUCTS_TYPE_STRUCTURE)
		return;

	/* Free all fields */
	for (field = type->args[0].v; field->name != NULL; field++) {
		(*field->type->uninit) (field->type,
					(char *)data + field->offset);
	}
	memset((char *)data, 0, type->size);
}

/*******************************************************************************
 * END OF FILE
 ******************************************************************************/
