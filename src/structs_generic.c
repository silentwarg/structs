/*******************************************************************************
 * HEADERS
 ******************************************************************************/

/* Standard Includes */
#include <sys/types.h>
#include <endian.h>
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

#ifndef BYTE_ORDER
#error BYTE_ORDER is undefined
#endif

/*******************************************************************************
 * FUNCTION DEFINITIONS
 ******************************************************************************/

int structs_region_init(const struct structs_type *type, void *data)
{
	memset(data, 0, type->size);
	return (0);
}

int structs_region_copy(const struct structs_type *type,
			const void *from, void *to)
{
	memcpy(to, from, type->size);
	return (0);
}

int structs_region_equal(const struct structs_type *type,
			 const void *v1, const void *v2)
{
	return (memcmp(v1, v2, type->size) == 0);
}

int structs_region_encode(const struct structs_type *type,
			  struct structs_data *code, const void *data)
{
	if ((code->data = calloc(1, type->size)) == NULL)
		return (-1);
	memcpy(code->data, data, type->size);
	code->length = type->size;
	return (0);
}

int structs_region_decode(const struct structs_type *type,
			  const unsigned char *code, size_t cmax, void *data,
			  char *ebuf, size_t emax)
{
	if (cmax < type->size) {
		strncpy(ebuf, "encoded data is truncated", emax);
		errno = EINVAL;
		return (-1);
	}
	memcpy(data, code, type->size);
	return (type->size);
}

int structs_region_encode_netorder(const struct structs_type *type,
				   struct structs_data *code, const void *data)
{
	if (structs_region_encode(type, code, data) == -1)
		return (-1);
#if BYTE_ORDER == LITTLE_ENDIAN
	{
		unsigned char temp;
		unsigned int i;

		for (i = 0; i < code->length / 2; i++) {
			temp = code->data[i];
			code->data[i] = code->data[code->length - 1 - i];
			code->data[code->length - 1 - i] = temp;
		}
	}
#endif
	return (0);
}

int structs_region_decode_netorder(const struct structs_type *type,
				   const unsigned char *code, size_t cmax,
				   void *data, char *ebuf, size_t emax)
{
#if BYTE_ORDER == LITTLE_ENDIAN
	unsigned char buf[16];
	unsigned char temp;
	unsigned int i;

	if (type->size > sizeof(buf)) {
		errno = ERANGE;	/* XXX oops fixed buffer is too small */
		return (-1);
	}
	if (cmax > type->size)
		cmax = type->size;
	memcpy(buf, code, cmax);
	for (i = 0; i < type->size / 2; i++) {
		temp = buf[i];
		buf[i] = buf[type->size - 1 - i];
		buf[type->size - 1 - i] = temp;
	}
	code = buf;
#endif
	return (structs_region_decode(type, code, cmax, data, ebuf, emax));
}

char *structs_notsupp_ascify(const struct structs_type *type, const void *data)
{
	errno = ENOSYS;
	return (NULL);
}

int structs_notsupp_init(const struct structs_type *type, void *data)
{
	errno = ENOSYS;
	return (-1);
}

int structs_notsupp_copy(const struct structs_type *type,
			 const void *from, void *to)
{
	errno = ENOSYS;
	return (-1);
}

int structs_notsupp_equal(const struct structs_type *type,
			  const void *v1, const void *v2)
{
	errno = ENOSYS;
	return (-1);
}

int structs_notsupp_binify(const struct structs_type *type,
			   const char *ascii, void *data,
			   char *ebuf, size_t emax)
{
	strncpy(ebuf,
		"parsing from ASCII is not supported by this structs type",
		emax);
	errno = ENOSYS;
	return (-1);
}

int structs_notsupp_encode(const struct structs_type *type,
			   struct structs_data *code, const void *data)
{
	errno = ENOSYS;
	return (-1);
}

int structs_notsupp_decode(const struct structs_type *type,
			   const unsigned char *code, size_t cmax, void *data,
			   char *ebuf, size_t emax)
{
	strncpy(ebuf,
		"binary decoding is not supported by this structs type", emax);
	errno = ENOSYS;
	return (-1);
}

void structs_nothing_free(const struct structs_type *type, void *data)
{
	return;
}

int structs_ascii_copy(const struct structs_type *type,
		       const void *from, void *to)
{
	char *ascii;
	int rtn;

	if ((ascii = (*type->ascify) (type, from)) == NULL)
		return (-1);
	rtn = (*type->binify) (type, ascii, to, NULL, 0);
	free(ascii);
	return (rtn);
}

/*******************************************************************************
 * END OF FILE
 ******************************************************************************/
