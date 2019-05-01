/*******************************************************************************
 * HEADERS
 ******************************************************************************/

/* Standard Includes */
#include <sys/types.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

/* Module Includes */
#include "structs.h"
#include "structs_type_array.h"
#include "structs_type_string.h"

/*******************************************************************************
 * DYNAMICALLY ALLOCATED STRING TYPE
 ******************************************************************************/

int structs_string_init(const struct structs_type *type, void *data)
{
	return (structs_string_binify(type, "", data, NULL, 0));
}

int structs_string_equal(const struct structs_type *type,
			 const void *v1, const void *v2)
{
	const int as_null = type->args[0].i;
	const char *const s1 = *((const char **)v1);
	const char *const s2 = *((const char **)v2);
	int empty1;
	int empty2;

	if (!as_null)
		return (strcmp(s1, s2) == 0);
	empty1 = (s1 == NULL) || *s1 == '\0';
	empty2 = (s2 == NULL) || *s2 == '\0';
	if (empty1 ^ empty2)
		return (0);
	if (empty1)
		return (1);
	return (strcmp(s1, s2) == 0);
}

char *structs_string_ascify(const struct structs_type *type, const void *data)
{
	const int as_null = type->args[0].i;
	const char *s = *((char **)data);

	if (as_null && s == NULL)
		s = "";
	return (strdup(s));
}

int structs_string_binify(const struct structs_type *type,
			  const char *ascii, void *data,
			  char *ebuf, size_t emax)
{
	const int as_null = type->args[0].i;
	char *s;

	if (as_null && *ascii == '\0')
		s = NULL;
	else if ((s = strdup(ascii)) == NULL)
		return (-1);
	*((char **)data) = s;
	return (0);
}

/*
 * This can be used by any type that wishes to encode its
 * value using its ASCII string representation.
 */
int structs_string_encode(const struct structs_type *type,
			  struct structs_data *code, const void *data)
{
	if ((code->data = (unsigned char *)structs_get_string(type,
							      NULL,
							      data)) == NULL)
		return (-1);
	code->length = strlen((char *)code->data) + 1;
	return (0);
}

/*
 * This can be used by any type that wishes to encode its
 * value using its ASCII string representation.
 */
int structs_string_decode(const struct structs_type *type,
			  const unsigned char *code, size_t cmax, void *data,
			  char *ebuf, size_t emax)
{
	size_t slen;

	/* Determine length of string */
	for (slen = 0; slen < cmax && ((char *)code)[slen] != '\0'; slen++) ;
	if (slen == cmax) {
		strncpy(ebuf, "encoded string is truncated", emax);
		errno = EINVAL;
		return (-1);
	}

	/* Set string value */
	if ((*type->binify) (type, (const char *)code, data, ebuf, emax) == -1)
		return (-1);

	/* Done */
	return (slen + 1);
}

void structs_string_free(const struct structs_type *type, void *data)
{
	char *const s = *((char **)data);

	if (s != NULL) {
		free(s);
		*((char **)data) = NULL;
	}
}

const struct structs_type structs_type_string = STRUCTS_STRING_TYPE(0);
const struct structs_type structs_type_string_null = STRUCTS_STRING_TYPE(1);

/*********************************************************************
	BOUNDED LENGTH STRING TYPE
*********************************************************************/

/* Find the length of STRING, but scan at most MAXLEN characters.
   If no '\0' or 0xFF terminator is found in that many characters,
   return MAXLEN.  */
size_t c_strnlen(const char *string, size_t maxlen)
{
	char *end;

	// Let's search for '\0'
	end = memchr(string, '\0', maxlen);
	if (end)
		return (end - string);
	// If '\0' is not found, let's search for 0xFF
	end = memchr(string, 0xFF, maxlen);
	return (end) ? (end - string) : maxlen;
}

static int hexval(int c)
{
	if ('0' <= c && c <= '9')
		return c - '0';
	return 10 + c - 'A';
}

/* Decode and copy a quoted-printable string */
static void decode_quopri(const char *s, char *out, unsigned int outlen)
{
	int i = 0;

	while (*s && i < outlen) {
		if (*s != '=')
			out[i++] = *s++;
		else if (*(s + 1) == '\r' && *(s + 2) == '\n')
			s += 3;
		else if (*(s + 1) == '\n')
			s += 2;
		else if (!strchr("0123456789ABCDEF", *(s + 1)))
			out[i++] = *s++;
		else if (!strchr("0123456789ABCDEF", *(s + 2)))
			out[i++] = *s++;
		else {
			out[i++] = 16 * hexval(*(s + 1)) + hexval(*(s + 2));
			s += 3;
		}
	}
}

/* Encode and copy a quoted-printable string. */
static void encode_quopri(const char *s, unsigned int slen,
			  char *out, unsigned int outlen)
{
	int n, i = 0, j = 0;

	for (n = 0; *s && j < slen && i < outlen; s++, j++) {
		if (*s == 10 || *s == 13) {
			out[i] = *s;
			n = 0;
			i++;
		} else if (*s < 32 || *s == 61 || *s > 126) {
			sprintf(&out[i], "=%02X", (unsigned char)*s);
			n += 3;
			i += 3;
		} else if (*s != 32 || (*(s + 1) != 10 && *(s + 1) != 13)) {
			out[i] = *s;
			n++;
			i++;
		} else {
			sprintf(&out[i], "=20");
			n += 3;
			i += 3;
		}
	}
	out[i] = '\0';
}

int structs_bstring_equal(const struct structs_type *type,
			  const void *v1, const void *v2)
{
	const char *const s1 = v1;
	const char *const s2 = v2;

	return (strncmp(s1, s2, type->size) == 0);
}

char *structs_bstring_ascify(const struct structs_type *type, const void *data)
{
	const char *const s = data;
	char *out;
	unsigned int outlen = (type->size * 3);
	unsigned int slen = c_strnlen(s, type->size);

	out = malloc(outlen + 1);
	if (!out)
		return strdup(" ");
	encode_quopri(s, slen, out, outlen);
	return (out);
}

int structs_bstring_binify(const struct structs_type *type,
			   const char *ascii, void *data,
			   char *ebuf, size_t emax)
{
	const size_t alen = strlen(ascii);

	if (alen > (type->size * 3)) {
		strncpy(ebuf,
			"string is too long for bounded length buffer", emax);
		return (-1);
	}

	decode_quopri(ascii, data, type->size);
	return (0);
}

/*******************************************************************************
 * END OF FILE
 ******************************************************************************/
