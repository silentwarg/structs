/*******************************************************************************
 * HEADERS
 ******************************************************************************/

/* Standard Includes */
#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

/* Module Includes */
#include "structs.h"
#include "structs_type_array.h"

/*******************************************************************************
 * MACROS/VARIABLES
 ******************************************************************************/

#define NUM_BYTES(x) (((x) + 7) / 8)

/*******************************************************************************
 * FUNCTION DEFINITIONS
 ******************************************************************************/

/*******************************************************************************
 * VARIABLE LENGTH ARRAYS
 ******************************************************************************/

int structs_array_copy(const struct structs_type *type,
		       const void *from, void *to)
{
	const struct structs_type *const etype = type->args[0].v;
	const struct structs_array *const fary = from;
	struct structs_array *const tary = to;
	int errno_save;
	unsigned int i;

	/* Make sure it's really an array type */
	if (type->tclass != STRUCTS_TYPE_ARRAY) {
		errno = EINVAL;
		return (-1);
	}

	/* Allocate a new array */
	memset(tary, 0, sizeof(*tary));
	if ((tary->elems = calloc(1, fary->length * etype->size)) == NULL)
		return (-1);

	/* Copy elements into it */
	for (i = 0; i < fary->length; i++) {
		const void *const from_elem
		    = (char *)fary->elems + (i * etype->size);
		void *const to_elem = (char *)tary->elems + (i * etype->size);

		if ((*etype->copy) (etype, from_elem, to_elem) == -1)
			break;
		tary->length++;
	}

	/* If there was a failure, undo the half completed job */
	if (i < fary->length) {
		errno_save = errno;
		while (i-- > 0) {
			(*etype->uninit) (etype,
					  (char *)tary->elems +
					  (i * etype->size));
		}
		free(tary->elems);
		memset(tary, 0, sizeof(*tary));
		errno = errno_save;
		return (-1);
	}

	/* Done */
	return (0);
}

int structs_array_equal(const struct structs_type *type,
			const void *v1, const void *v2)
{
	const struct structs_type *const etype = type->args[0].v;
	const struct structs_array *const ary1 = v1;
	const struct structs_array *const ary2 = v2;
	unsigned int i;

	/* Make sure it's really an array type */
	if (type->tclass != STRUCTS_TYPE_ARRAY)
		return (0);

	/* Check array lengths first */
	if (ary1->length != ary2->length)
		return (0);

	/* Now compare individual elements */
	for (i = 0;
	     i < ary1->length && (*etype->equal) (etype,
						  (char *)ary1->elems +
						  (i * etype->size),
						  (char *)ary2->elems +
						  (i * etype->size)); i++) ;
	return (i == ary1->length);
}

int structs_array_encode(const struct structs_type *type,
			 struct structs_data *code, const void *data)
{
	const struct structs_type *const etype = type->args[0].v;
	const struct structs_array *const ary = data;
	const unsigned int bitslen = NUM_BYTES(ary->length);
	struct structs_data *ecodes;
	u_int32_t elength;
	unsigned char *bits;
	void *delem;
	unsigned int tlen;
	int r = -1;
	unsigned int i;

	/* Make sure it's really an array type */
	if (type->tclass != STRUCTS_TYPE_ARRAY) {
		errno = EINVAL;
		return (-1);
	}

	/* Get the default value for an element */
	if ((delem = calloc(1, etype->size)) == NULL)
		return (-1);
	if (structs_init(etype, NULL, delem) == -1)
		goto fail1;

	/* Create bit array. Each bit indicates an element that is present. */
	if ((bits = calloc(1, bitslen)) == NULL)
		goto fail2;
	memset(bits, 0, bitslen);
	tlen = 4 + bitslen;	/* length word + bits array */

	/* Create array of individual encodings, one per element */
	if ((ecodes = calloc(1, ary->length * sizeof(*ecodes))) == NULL)
		goto fail3;
	for (i = 0; i < ary->length; i++) {
		const void *const elem = (char *)ary->elems + (i * etype->size);
		struct structs_data *const ecode = &ecodes[i];

		/* Check for default value, leave out if same as */
		if ((*etype->equal) (etype, elem, delem) == 1) {
			memset(ecode, 0, sizeof(*ecode));
			continue;
		}
		bits[i / 8] |= (1 << (i % 8));

		/* Encode element */
		if ((*etype->encode) (etype, ecode, elem) == -1)
			goto fail4;
		tlen += ecode->length;
	}

	/* Allocate final encoded region */
	if ((code->data = calloc(1, tlen)) == NULL)
		goto fail4;

	/* Copy array length */
	elength = htonl(ary->length);
	memcpy(code->data, &elength, 4);
	code->length = 4;

	/* Copy bits array */
	memcpy(code->data + code->length, bits, bitslen);
	code->length += bitslen;

	/* Copy encoded elements */
	for (i = 0; i < ary->length; i++) {
		struct structs_data *const ecode = &ecodes[i];

		memcpy(code->data + code->length, ecode->data, ecode->length);
		code->length += ecode->length;
	}

	/* OK */
	r = 0;

	/* Clean up and exit */
fail4:	while (i-- > 0)
		free(ecodes[i].data);
	free(ecodes);
fail3:	free(bits);
fail2:	structs_free(etype, NULL, delem);
fail1:	free(delem);
	return (r);
}

int structs_array_decode(const struct structs_type *type,
			 const unsigned char *code, size_t cmax,
			 void *data, char *ebuf, size_t emax)
{
	const struct structs_type *const etype = type->args[0].v;
	struct structs_array *const ary = data;
	const unsigned char *bits;
	u_int32_t elength;
	unsigned int bitslen;
	int clen;
	unsigned int i;

	/* Make sure it's really an array type */
	if (type->tclass != STRUCTS_TYPE_ARRAY) {
		errno = EINVAL;
		return (-1);
	}

	/* Get number of elements */
	if (cmax < 4)
		goto truncated;
	memcpy(&elength, code, 4);
	ary->length = ntohl(elength);
	code += 4;
	cmax -= 4;
	clen = 4;

	/* Get bits array */
	bitslen = NUM_BYTES(ary->length);
	if (cmax < bitslen) {
truncated:	strncpy(ebuf, "encoded array is truncated", emax);
		errno = EINVAL;
		return (-1);
	}
	bits = code;
	code += bitslen;
	cmax -= bitslen;
	clen += bitslen;

	/* Allocate array elements */
	if ((ary->elems = calloc(1, ary->length * etype->size)) == NULL)
		return (-1);

	/* Decode elements */
	for (i = 0; i < ary->length; i++) {
		void *const edata = (char *)ary->elems + (i * etype->size);
		int eclen;

		/* If element not present, assign it the default value */
		if ((bits[i / 8] & (1 << (i % 8))) == 0) {
			if (structs_init(etype, NULL, edata) == -1)
				goto fail;
			continue;
		}

		/* Decode element */
		if ((eclen = (*etype->decode) (etype,
					       code, cmax, edata, ebuf,
					       emax)) == -1)
			goto fail;

		/* Go to next encoded element */
		code += eclen;
		cmax -= eclen;
		clen += eclen;
		continue;

		/* Un-do work done so far */
fail:		while (i-- > 0) {
			structs_free(etype, NULL,
				     (char *)ary->elems + (i * etype->size));
		}
		free(ary->elems);
		return (-1);
	}

	/* Done */
	return (clen);
}

void structs_array_free(const struct structs_type *type, void *data)
{
	const struct structs_type *const etype = type->args[0].v;
	struct structs_array *const ary = data;
	unsigned int i;

	/* Make sure it's really an array type */
	if (type->tclass != STRUCTS_TYPE_ARRAY)
		return;

	/* Free individual elements */
	for (i = 0; i < ary->length; i++)
		(*etype->uninit) (etype,
				  (char *)ary->elems + (i * etype->size));

	/* Free array itself */
	free(ary->elems);
	memset(ary, 0, sizeof(*ary));
}

int structs_array_length(const struct structs_type *type,
		     const char *name, const void *data)
{
	const struct structs_array *ary = data;

	/* Find array */
	if ((type = structs_find(type, name, (const void **)&ary, 0)) == NULL)
		return (-1);

	/* Make sure it's really an array type */
	if (type->tclass != STRUCTS_TYPE_ARRAY) {
		errno = EINVAL;
		return (-1);
	}

	/* Return length */
	return (ary->length);
}

int structs_array_reset(const struct structs_type *type,
			const char *name, void *data)
{
	/* Find array */
	if ((type = structs_find(type, name, (const void **)&data, 1)) == NULL)
		return (-1);

	/* Make sure it's really an array type */
	if (type->tclass != STRUCTS_TYPE_ARRAY) {
		errno = EINVAL;
		return (-1);
	}

	/* Free it, which resets it as well */
	structs_array_free(type, data);
	return (0);
}

int structs_array_insert(const struct structs_type *type,
			 const char *name, unsigned int index, void *data)
{
	struct structs_array *ary = data;
	const struct structs_type *etype;
	void *mem;

	/* Find array */
	if ((type = structs_find(type, name, (const void **)&ary, 0)) == NULL)
		return (-1);
	etype = type->args[0].v;

	/* Make sure it's really an array type */
	if (type->tclass != STRUCTS_TYPE_ARRAY) {
		errno = EINVAL;
		return (-1);
	}

	/* Check index */
	if (index > ary->length) {
		errno = EDOM;
		return (-1);
	}

	/* Reallocate array, leaving room for new element and a shift */
	if ((mem =
	     realloc(ary->elems, (ary->length + 2) * etype->size)) == NULL)
		return (-1);
	ary->elems = mem;

	/* Initialize new element; we'll move it into place later */
	if ((*etype->init) (etype,
			    (char *)ary->elems + (ary->length +
						  1) * etype->size) == -1)
		return (-1);

	/* Shift array over by one and move new element into place */
	memmove((char *)ary->elems + (index + 1) * etype->size,
		(char *)ary->elems + index * etype->size,
		(ary->length - index) * etype->size);
	memcpy((char *)ary->elems + index * etype->size,
	       (char *)ary->elems + (ary->length + 1) * etype->size,
	       etype->size);
	ary->length++;
	return (0);
}

int structs_array_delete(const struct structs_type *type,
			 const char *name, unsigned int index, void *data)
{
	const struct structs_type *etype;
	struct structs_array *ary = data;

	/* Find array */
	if ((type = structs_find(type, name, (const void **)&ary, 0)) == NULL)
		return (-1);
	etype = type->args[0].v;

	/* Make sure it's really an array type */
	if (type->tclass != STRUCTS_TYPE_ARRAY) {
		errno = EINVAL;
		return (-1);
	}

	/* Check index */
	if (index >= ary->length) {
		errno = EDOM;
		return (-1);
	}

	/* Free element */
	(*etype->uninit) (etype, (char *)ary->elems + (index * etype->size));

	/* Shift array */
	memmove((char *)ary->elems + index * etype->size,
		(char *)ary->elems + (index + 1) * etype->size,
		(--ary->length - index) * etype->size);
	return (0);
}

/*
 * Given a name, prepare all arrays in the name path for being
 * set with a new element.
 *
 * This means that any array index equal to the length of the array
 * causes a new element to be appended to the array rather than
 * an element not found error, and that any array index equal to
 * zero means to reset the entire array to be empty.
 *
 * This allows an entire array to be set by setting each element
 * in order, as long as this function is called with the name of
 * the element before each set operation.
 */
int structs_array_prep(const struct structs_type *type,
		       const char *name, void *data)
{
	char *nbuf;
	char *s;

	/* Skip if name cannot be an array element name */
	if (name == NULL || *name == '\0')
		return (0);

	/* Copy name into writable buffer */
	if ((nbuf = calloc(1, strlen(name) + 2)) == NULL)
		return (-1);
	nbuf[0] = '*';		/* for debugging */
	strcpy(nbuf + 1, name);

	/* Check all array element names in name path */
	for (s = nbuf; s != NULL; s = strchr(s + 1, STRUCTS_SEPARATOR)) {
		const struct structs_type *atype;
		struct structs_array *ary = data;
		unsigned long index;
		char *eptr;
		char *t;
		char ch;

		/* Go to next descendant node using prefix of name */
		ch = *s;
		*s = '\0';
		if ((atype = structs_find(type,
					  nbuf + (s != nbuf),
					  (const void **)&ary, 1)) == NULL) {
			free(nbuf);
			return (-1);
		}
		*s = ch;

		/* If not array type, continue */
		if (atype->tclass != STRUCTS_TYPE_ARRAY)
			continue;

		/* Get array index */
		if ((t = strchr(s + 1, STRUCTS_SEPARATOR)) != NULL)
			*t = '\0';
		index = strtoul(s + 1, &eptr, 10);
		if (eptr == s + 1 || *eptr != '\0') {
			errno = ENOENT;
			free(nbuf);
			return (-1);
		}

		/* If setting an element that already exists, accept */
		if (index < ary->length)
			goto next;

		/* Must be setting the next new item in the array */
		if (index != ary->length) {
			errno = ENOENT;
			free(nbuf);
			return (-1);
		}

		/* Add new item; it will be in an initialized state */
		if (structs_array_insert(atype, NULL, ary->length, ary) == -1) {
			free(nbuf);
			return (-1);
		}

next:
		/* Repair name buffer for next time */
		if (t != NULL)
			*t = STRUCTS_SEPARATOR;
	}

	/* Done */
	free(nbuf);
	return (0);
}

/*
 * Given a name, allocate n items to a structs array.  If array is already
 * allocated it will be grown to nitems, if smaller than items the trailing
 * items will be truncated.
 */
int structs_array_setsize(const struct structs_type *type,
			  const char *name, unsigned int nitems, void *data,
			  int do_init)
{
	struct structs_array *ary = data;
	const struct structs_type *etype;
	const char *mem_type;
	void *mem;
	int idx;

	/* Find array */
	if ((type = structs_find(type, name, (const void **)&ary, 0)) == NULL)
		return (-1);
	etype = type->args[0].v;
	mem_type = type->args[1].v;

	/* Make sure it's really an array type */
	if (type->tclass != STRUCTS_TYPE_ARRAY) {
		errno = EINVAL;
		return (-1);
	}

	/* Check size for negative and current (noop) */
	if (nitems < 0) {
		errno = EDOM;
		return (-1);
	}
	if (nitems == ary->length) {
		return (0);
	}
	/* Check size for 0 - free elements */
	if (nitems == 0) {
		free(ary->elems);
		ary->elems = NULL;
		ary->length = 0;
		return (0);
	}

	/* Reallocate array, leaving room for new element and a shift */
	if ((mem = realloc(ary->elems, nitems * etype->size)) == NULL)
		return (-1);

	/* Zero out any new memory */
	if (nitems > ary->length) {
		memset(((char *)mem) + (ary->length * etype->size), 0,
		       (nitems - ary->length) * etype->size);
	}

	/* Do initialization */
	if (do_init) {
		for (idx = ary->length; idx < nitems; idx++) {
			/* Initialize new element */
			if ((*etype->init) (etype,
					    (char *)mem +
					    (idx * etype->size)) == -1) {
				return (-1);
			}
		}
	}
	ary->elems = mem;
	ary->length = nitems;
	return (0);
}

/*******************************************************************************
 * FIXED LENGTH ARRAYS
 ******************************************************************************/

int structs_fixedarray_init(const struct structs_type *type, void *data)
{
	const struct structs_type *const etype = type->args[0].v;
	const unsigned int length = type->args[2].i;
	unsigned int i;

	for (i = 0; i < length; i++) {
		if ((*etype->init) (etype,
				    (char *)data + (i * etype->size)) == -1) {
			while (i-- > 0) {
				(*etype->uninit) (etype,
						  (char *)data +
						  (i * etype->size));
			}
			return (-1);
		}
	}
	return (0);
}

int structs_fixedarray_copy(const struct structs_type *type,
			const void *from, void *to)
{
	const struct structs_type *const etype = type->args[0].v;
	const unsigned int length = type->args[2].i;
	unsigned int i;

	/* Make sure it's really a fixedarray type */
	if (type->tclass != STRUCTS_TYPE_FIXEDARRAY) {
		errno = EINVAL;
		return (-1);
	}

	/* Copy elements into it */
	for (i = 0; i < length; i++) {
		const void *const from_elem = (char *)from + (i * etype->size);
		void *const to_elem = (char *)to + (i * etype->size);

		if ((*etype->copy) (etype, from_elem, to_elem) == -1)
			break;
	}

	/* If there was a failure, undo the half completed job */
	if (i < length) {
		while (i-- > 0)
			(*etype->uninit) (etype,
					  (char *)to + (i * etype->size));
		return (-1);
	}

	/* Done */
	return (0);
}

int structs_fixedarray_equal(const struct structs_type *type,
			     const void *v1, const void *v2)
{
	const struct structs_type *const etype = type->args[0].v;
	const unsigned int length = type->args[2].i;
	unsigned int i;

	/* Make sure it's really a fixedarray type */
	if (type->tclass != STRUCTS_TYPE_FIXEDARRAY)
		return (0);

	/* Compare individual elements */
	for (i = 0;
	     i < length && (*etype->equal) (etype,
					    (char *)v1 + (i * etype->size),
					    (char *)v2 + (i * etype->size));
	     i++) ;
	return (i == length);
}

int structs_fixedarray_encode(const struct structs_type *type,
			      struct structs_data *code, const void *data)
{
	const struct structs_type *const etype = type->args[0].v;
	const unsigned int length = type->args[2].i;
	const unsigned int bitslen = NUM_BYTES(length);
	struct structs_data *ecodes;
	unsigned char *bits;
	void *delem;
	unsigned int tlen;
	int r = -1;
	unsigned int i;

	/* Make sure it's really a fixedarray type */
	if (type->tclass != STRUCTS_TYPE_FIXEDARRAY) {
		errno = EINVAL;
		return (-1);
	}

	/* Get the default value for an element */
	if ((delem = calloc(1, etype->size)) == NULL)
		return (-1);
	if (structs_init(etype, NULL, delem) == -1)
		goto fail1;

	/* Create bit array. Each bit indicates an element that is present. */
	if ((bits = calloc(1, bitslen)) == NULL)
		goto fail2;
	memset(bits, 0, bitslen);
	tlen = bitslen;

	/* Create array of individual encodings, one per element */
	if ((ecodes = calloc(1, length * sizeof(*ecodes))) == NULL)
		goto fail3;
	for (i = 0; i < length; i++) {
		const void *const elem = (char *)data + (i * etype->size);
		struct structs_data *const ecode = &ecodes[i];

		/* Check for default value, leave out if same as */
		if ((*etype->equal) (etype, elem, delem) == 1) {
			memset(ecode, 0, sizeof(*ecode));
			continue;
		}
		bits[i / 8] |= (1 << (i % 8));

		/* Encode element */
		if ((*etype->encode) (etype, ecode, elem) == -1)
			goto fail4;
		tlen += ecode->length;
	}

	/* Allocate final encoded region */
	if ((code->data = calloc(1, tlen)) == NULL)
		goto fail4;

	/* Copy bits array */
	memcpy(code->data, bits, bitslen);
	code->length = bitslen;

	/* Copy encoded elements */
	for (i = 0; i < length; i++) {
		struct structs_data *const ecode = &ecodes[i];

		memcpy(code->data + code->length, ecode->data, ecode->length);
		code->length += ecode->length;
	}

	/* OK */
	r = 0;

	/* Clean up and exit */
fail4:	while (i-- > 0)
		free(ecodes[i].data);
	free(ecodes);
fail3:	free(bits);
fail2:	structs_free(etype, NULL, delem);
fail1:	free(delem);
	return (r);
}

int structs_fixedarray_decode(const struct structs_type *type,
			      const unsigned char *code, size_t cmax,
			      void *data, char *ebuf, size_t emax)
{
	const struct structs_type *const etype = type->args[0].v;
	const unsigned int length = type->args[2].i;
	const unsigned int bitslen = NUM_BYTES(length);
	const unsigned char *bits;
	int clen = 0;
	unsigned int i;

	/* Make sure it's really a fixedarray type */
	if (type->tclass != STRUCTS_TYPE_FIXEDARRAY) {
		errno = EINVAL;
		return (-1);
	}

	/* Get bits array */
	if (cmax < bitslen) {
		strncpy(ebuf, "encoded array is truncated", emax);
		errno = EINVAL;
		return (-1);
	}
	bits = code;
	code += bitslen;
	cmax -= bitslen;
	clen += bitslen;

	/* Decode elements */
	for (i = 0; i < length; i++) {
		void *const edata = (char *)data + (i * etype->size);
		int eclen;

		/* If element not present, assign it the default value */
		if ((bits[i / 8] & (1 << (i % 8))) == 0) {
			if (structs_init(etype, NULL, edata) == -1)
				goto fail;
			continue;
		}

		/* Decode element */
		if ((eclen = (*etype->decode) (etype,
					       code, cmax, edata, ebuf,
					       emax)) == -1)
			goto fail;

		/* Go to next encoded element */
		code += eclen;
		cmax -= eclen;
		clen += eclen;
		continue;

		/* Un-do work done so far */
fail:		while (i-- > 0) {
			structs_free(etype, NULL,
				     (char *)data + (i * etype->size));
		}
		return (-1);
	}

	/* Done */
	return (clen);
}

void structs_fixedarray_free(const struct structs_type *type, void *data)
{
	const struct structs_type *const etype = type->args[0].v;
	const unsigned int length = type->args[2].i;
	unsigned int i;

	/* Make sure it's really a fixedarray type */
	if (type->tclass != STRUCTS_TYPE_FIXEDARRAY)
		return;

	/* Free elements */
	for (i = 0; i < length; i++)
		(*etype->uninit) (etype, (char *)data + (i * etype->size));
}

/*******************************************************************************
 * END OF FILE
 ******************************************************************************/
