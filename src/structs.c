/*******************************************************************************
 * HEADERS
 ******************************************************************************/

/* Standard Includes */
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

/* Module Includes */
#include "structs.h"
#include "structs_type_int.h"
#include "structs_type_array.h"
#include "structs_type_string.h"
#include "structs_type_struct.h"
#include "structs_type_union.h"

/*******************************************************************************
 * MACROS/VARIABLES
 ******************************************************************************/

/* Special handling for array length as a read-only field */
static const struct structs_type structs_type_array_length = {
	sizeof(unsigned int),
	"uint",
	STRUCTS_TYPE_PRIMITIVE,
	structs_region_init,
	structs_region_copy,
	structs_region_equal,
	structs_int_ascify,
	structs_notsupp_binify,
	structs_region_encode_netorder,
	structs_notsupp_decode,
	structs_nothing_free,
	{ { (void *)2}, { (void *)0} }	/* args for structs_int_ascify */
};

/* Special handling for union field name as a read-only field */
static const struct structs_type structs_type_union_field_name = {
	sizeof(char *),
	"string",
	STRUCTS_TYPE_PRIMITIVE,
	structs_notsupp_init,
	structs_notsupp_copy,
	structs_string_equal,
	structs_string_ascify,
	structs_notsupp_binify,
	structs_notsupp_encode,
	structs_notsupp_decode,
	structs_nothing_free,
	{ { (void *)"union field_name"},	/* args for structs_type_string */
	 { (void *)0} }
};

/*******************************************************************************
 * FUNCTION DECLARATIONS
 ******************************************************************************/

/*******************************************************************************
 * FUNCTION DEFINITIONS
 ******************************************************************************/

/*
 * Initialize an item.
 */
int structs_init(const struct structs_type *type, const char *name, void *data)
{
	/* Find item */
	if ((type = structs_find(type, name, (const void **)&data, 0)) == NULL)
		return (-1);

	/* Initialize it */
	return ((*type->init) (type, data));
}

/*
 * Reset an item.
 */
int structs_reset(const struct structs_type *type, const char *name, void *data)
{
	void *temp;

	/* Find item */
	if ((type = structs_find(type, name, (const void **)&data, 0)) == NULL)
		return (-1);

	/* Make a temporary new copy */
	if ((temp = calloc(1, type->size)) == NULL)
		return (-1);
	memset(temp, 0, type->size);
	if ((*type->init) (type, temp) == -1) {
		free(temp);
		return (-1);
	}

	/* Replace existing item, freeing it first */
	(*type->uninit) (type, data);
	memcpy(data, temp, type->size);
	free(temp);
	return (0);
}

/*
 * Free an item, returning it to its initialized state.
 *
 * If "name" is NULL or empty string, the entire structure is free'd.
 */
int structs_free(const struct structs_type *type, const char *name, void *data)
{
	const int errno_save = errno;

	/* Find item */
	if ((type = structs_find(type, name, (const void **)&data, 0)) == NULL)
		return (-1);

	/* Free it */
	(*type->uninit) (type, data);
	errno = errno_save;
	return (0);
}

/*
 * Get a copy of an item.
 *
 * If "name" is NULL or empty string, the entire structure is copied.
 *
 * It is assumed that "to" points to a region big enough to hold
 * the copy of the item. "to" will not be free'd first, so it should
 * not already be initialized.
 *
 * Note: "name" is only applied to "from". The structs types of
 * "from.<name>" and "to" must be the same.
 */
int structs_get(const struct structs_type *type,
		const char *name, const void *from, void *to)
{
	/* Find item */
	if ((type = structs_find(type, name, (const void **)&from, 0)) == NULL)
		return (-1);

	/* Copy item */
	return ((*type->copy) (type, from, to));
}

/*
 * Set an item in a structure.
 *
 * If "name" is NULL or empty string, the entire structure is copied.
 *
 * It is assumed that "to" is already initialized.
 *
 * Note: "name" is only applied to "to". The structs types of
 * "from" and "to.<name>" must be the same.
 */
int structs_set(const struct structs_type *type,
		const void *from, const char *name, void *to)
{
	void *copy;

	/* Find item */
	if ((type = structs_find(type, name, (const void **)&to, 0)) == NULL)
		return (-1);

	/* Make a new copy of 'from' */
	if ((copy = calloc(1, type->size)) == NULL)
		return (-1);
	if ((*type->copy) (type, from, copy) == -1) {
		free(copy);
		return (-1);
	}

	/* Free overwritten item in 'to' */
	(*type->uninit) (type, to);

	/* Move new item in its place */
	memcpy(to, copy, type->size);

	/* Done */
	free(copy);
	return (0);
}

/*
 * Get the ASCII form of an item.
 */
char *structs_get_string(const struct structs_type *type,
			 const char *name, const void *data)
{
	/* Find item */
	if ((type = structs_find(type, name, (const void **)&data, 0)) == NULL)
		return (NULL);

	/* Ascify it */
	return ((*type->ascify) (type, data));
}

/*
 * Set an item's value from a string.
 *
 * The referred to item must be of a type that supports ASCII encoding,
 * and is assumed to be already initialized.
 */
int structs_set_string(const struct structs_type *type, const char *name,
		       const char *ascii, void *data, char *ebuf, size_t emax)
{
	void *temp;
	char dummy[1];

	/* Sanity check */
	if (ascii == NULL)
		ascii = "";
	if (ebuf == NULL) {
		ebuf = dummy;
		emax = sizeof(dummy);
	}

	/* Initialize error buffer */
	if (emax > 0)
		*ebuf = '\0';

	/* Find item */
	if ((type = structs_find(type, name, (const void **)&data, 1)) == NULL) {
		strncpy(ebuf, strerror(errno), emax);
		return (-1);
	}

	/* Binify item into temporary storage */
	if ((temp = calloc(1, type->size)) == NULL)
		return (-1);
	memset(temp, 0, type->size);
	if ((*type->binify) (type, ascii, temp, ebuf, emax) == -1) {
		free(temp);
		if (emax > 0 && *ebuf == '\0')
			strncpy(ebuf, strerror(errno), emax);
		return (-1);
	}

	/* Replace existing item, freeing it first */
	(*type->uninit) (type, data);
	memcpy(data, temp, type->size);
	free(temp);
	return (0);
}

/*
 * Get the binary encoded form of an item.
 */
int structs_get_binary(const struct structs_type *type, const char *name,
		       const void *data, struct structs_data *code)
{
	/* Find item */
	if ((type = structs_find(type, name, (const void **)&data, 0)) == NULL) {
		memset(code, 0, sizeof(*code));
		return (-1);
	}

	/* Encode it */
	if ((*type->encode) (type, code, data) == -1) {
		memset(code, 0, sizeof(*code));
		return (-1);
	}

	/* Done */
	return (0);
}

/*
 * Set an item's value from its binary encoded value.
 */
int structs_set_binary(const struct structs_type *type, const char *name,
		       const struct structs_data *code, void *data, char *ebuf,
		       size_t emax)
{
	void *temp;
	int clen;

	/* Initialize error buffer */
	if (emax > 0)
		*ebuf = '\0';

	/* Find item */
	if ((type = structs_find(type, name, (const void **)&data, 0)) == NULL) {
		strncpy(ebuf, strerror(errno), emax);
		return (-1);
	}

	/* Decode item into temporary storage */
	if ((temp = calloc(1, type->size)) == NULL)
		return (-1);
	memset(temp, 0, type->size);
	if ((clen = (*type->decode) (type, code->data,
				     code->length, temp, ebuf, emax)) == -1) {
		free(temp);
		if (emax > 0 && *ebuf == '\0')
			strncpy(ebuf, strerror(errno), emax);
		return (-1);
	}
	assert(clen <= code->length);

	/* Replace existing item, freeing it first */
	(*type->uninit) (type, data);
	memcpy(data, temp, type->size);
	free(temp);

	/* Done */
	return (clen);
}

/*
 * Test for equality.
 */
int structs_equal(const struct structs_type *type,
		  const char *name, const void *data1, const void *data2)
{
	/* Find items */
	if (structs_find(type, name, (const void **)&data1, 0) == NULL)
		return (-1);
	if ((type = structs_find(type, name, (const void **)&data2, 0)) == NULL)
		return (-1);

	/* Compare them */
	return ((*type->equal) (type, data1, data2));
}

/*
 * Find an item in a structure.
 */
const struct structs_type *structs_find(const struct structs_type *type,
					const char *name, const void **datap,
					int set_union)
{
	const void *data = *datap;
	const char *next;

	/* Empty string means stop recursing */
	if (name == NULL || *name == '\0')
		return (type);

	/* Primitive types don't have sub-elements */
	if (type->tclass == STRUCTS_TYPE_PRIMITIVE) {
		errno = ENOENT;
		return (NULL);
	}

	/* Dereference through pointer(s) */
	while (type->tclass == STRUCTS_TYPE_POINTER) {
		type = type->args[0].v;
		data = *((void **)data);
	}

	/* Get next name component */
	if ((next = strchr(name, STRUCTS_SEPARATOR)) != NULL)
		next++;

	/* Find element of aggregate structure update type and data */
	switch (type->tclass) {
	case STRUCTS_TYPE_ARRAY:
		{
			const struct structs_type *const etype =
			    type->args[0].v;
			const struct structs_array *const ary = data;
			unsigned long index;
			char *eptr;

			/* Special handling for "length" */
			if (strcmp(name, "length") == 0) {
				type = &structs_type_array_length;
				data = (void *)&ary->length;
				break;
			}

			/* Decode an index */
			index = strtoul(name, &eptr, 10);
			if (!isdigit(*name)
			    || eptr == name
			    || (*eptr != '\0' && *eptr != STRUCTS_SEPARATOR)) {
				errno = ENOENT;
				return (NULL);
			}
			if (index >= ary->length) {
				errno = EDOM;
				return (NULL);
			}
			type = etype;
			data = (char *)ary->elems + (index * etype->size);
			break;
		}
	case STRUCTS_TYPE_FIXEDARRAY:
		{
			const struct structs_type *const etype =
			    type->args[0].v;
			const unsigned int length = type->args[2].i;
			unsigned long index;
			char *eptr;

			/* Special handling for "length" */
			if (strcmp(name, "length") == 0) {
				type = &structs_type_array_length;
				data = (void *)&type->args[2].i;
				break;
			}

			/* Decode an index */
			index = strtoul(name, &eptr, 10);
			if (!isdigit(*name)
			    || eptr == name
			    || (*eptr != '\0' && *eptr != STRUCTS_SEPARATOR)) {
				errno = ENOENT;
				return (NULL);
			}
			if (index >= length) {
				errno = EDOM;
				return (NULL);
			}
			type = etype;
			data = (char *)data + (index * etype->size);
			break;
		}
	case STRUCTS_TYPE_STRUCTURE:
		{
			const struct structs_field *field;

			/* Find the field */
			for (field = type->args[0].v; field->name != NULL;
			     field++) {
				const size_t fnlen = strlen(field->name);

				/* Handle field names with separator in them */
				if (strncmp(name, field->name, fnlen) == 0
				    && (name[fnlen] == '\0'
					|| name[fnlen] == STRUCTS_SEPARATOR)) {
					next = (name[fnlen] != '\0') ?
					    name + fnlen + 1 : NULL;
					break;
				}
			}
			if (field->name == NULL) {
				errno = ENOENT;
				return (NULL);
			}
			type = field->type;
			data = (char *)data + field->offset;
			break;
		}
	case STRUCTS_TYPE_UNION:
		{
			const struct structs_ufield *const fields =
			    type->args[0].v;
			struct structs_union *const un = (void *)data;
			const size_t oflen = strlen(un->field_name);
			const struct structs_ufield *ofield;
			const struct structs_ufield *field;
			void *new_un;
			void *data2;

			/* Special handling for "field_name" */
			if (strcmp(name, "field_name") == 0) {
				type = &structs_type_union_field_name;
				data = (void *)&un->field_name;
				break;
			}

			/* Find the old field */
			for (ofield = fields; ofield->name != NULL; ofield++) {
				if (strcmp(ofield->name, un->field_name) == 0)
					break;
			}
			if (ofield->name == NULL) {
				assert(0);
				errno = EINVAL;
				return (NULL);
			}

			/*
			 * Check if the union is already set to the desired field,
			 * handling field names with the separator char in them.
			 */
			if (strncmp(name, un->field_name, oflen) == 0
			    && (name[oflen] == '\0'
				|| name[oflen] == STRUCTS_SEPARATOR)) {
				next =
				    (name[oflen] !=
				     '\0') ? name + oflen + 1 : NULL;
				field = ofield;
				goto union_done;
			}

			/* Is modifying the union to get the right name acceptable? */
			if (!set_union) {
				errno = ENOENT;
				return (NULL);
			}

			/* Find the new field */
			for (field = fields; field->name != NULL; field++) {
				const size_t fnlen = strlen(field->name);

				/* Handle field names with separator in them */
				if (strncmp(name, field->name, fnlen) == 0
				    && (name[fnlen] == '\0'
					|| name[fnlen] == STRUCTS_SEPARATOR)) {
					next = (name[fnlen] != '\0') ?
					    name + fnlen + 1 : NULL;
					break;
				}
			}
			if (field->name == NULL) {
				errno = ENOENT;
				return (NULL);
			}

			/* Create a new union with the new field type */
			if ((new_un = calloc(1, field->type->size)) == NULL)
				return (NULL);
			if ((*field->type->init) (field->type, new_un) == -1) {
				free(new_un);
				return (NULL);
			}

			/* See if name would be found with new union instead of old */
			data2 = new_un;
			if (next != NULL
			    && structs_find(field->type, next,
					    (const void **)&data2, 1) == NULL) {
				(*field->type->uninit) (field->type, new_un);
				free(new_un);
				return (NULL);
			}

			/* Replace existing union with new one having desired type */
			(*ofield->type->uninit) (ofield->type, un->un);
			free(un->un);
			un->un = new_un;
			*((const char **)&un->field_name) = field->name;

union_done:
			/* Continue recursing */
			type = field->type;
			data = un->un;
			break;
		}
	default:
		assert(0);
		return (NULL);
	}

	/* Recurse on sub-element */
	if ((type =
	     structs_find(type, next, (const void **)&data, set_union)) == NULL)
		return (NULL);

	/* Done */
	*datap = data;
	return (type);
}

/*
 * Traverse a structure.
 */

struct structs_trav {
	char **list;
	unsigned int len;
	unsigned int alloc;
};

static int structs_trav(struct structs_trav *t, const char *name,
			const struct structs_type *type, const void *data);

int structs_traverse(const struct structs_type *type,
		     const void *data, char ***listp)
{
	struct structs_trav t;

	/* Initialize traversal structure */
	memset(&t, 0, sizeof(t));

	/* Recurse */
	if (structs_trav(&t, "", type, data) == -1) {
		while (t.len > 0)
			free(t.list[--t.len]);
		free(t.list);
		return (-1);
	}

	/* Return the result */
	*listp = t.list;
	return (t.len);
}

static int structs_trav(struct structs_trav *t, const char *name,
			const struct structs_type *type, const void *data)
{
	const char *const dot = (*name == '\0' ? "" : ".");
	char *ename;
	int i;

	/* Dereference through pointer(s) */
	while (type->tclass == STRUCTS_TYPE_POINTER) {
		type = type->args[0].v;
		data = *((void **)data);
	}

	switch (type->tclass) {
	case STRUCTS_TYPE_PRIMITIVE:
		{
			/* Grow list as necessary */
			if (t->len == t->alloc) {
				unsigned int new_alloc;
				char **new_list;

				new_alloc = (t->alloc + 32) * 2;
				if ((new_list =
				     realloc(t->list,
					     new_alloc * sizeof(*t->list))) ==
				    NULL)
					return (-1);
				t->list = new_list;
				t->alloc = new_alloc;
			}

			/* Add new name to list */
			if ((t->list[t->len] = strdup(name)) == NULL)
				return (-1);
			t->len++;

			/* Done */
			return (0);
		}

	case STRUCTS_TYPE_ARRAY:
		{
			const struct structs_type *const etype =
			    type->args[0].v;
			const struct structs_array *const ary = data;

			/* Iterate over array elements */
			for (i = 0; i < ary->length; i++) {
				const void *const edata
				    = (char *)ary->elems + (i * etype->size);

				if (asprintf(&ename, "%s%s%d", name, dot, i) ==
				    -1)
					return (-1);
				if (structs_trav(t, ename, etype, edata) == -1) {
					free(ename);
					return (-1);
				}
				free(ename);
			}

			/* Done */
			return (0);
		}

	case STRUCTS_TYPE_FIXEDARRAY:
		{
			const struct structs_type *const etype =
			    type->args[0].v;
			const unsigned int length = type->args[2].i;

			/* Iterate over array elements */
			for (i = 0; i < length; i++) {
				const void *const edata
				    = (char *)data + (i * etype->size);

				if (asprintf(&ename, "%s%s%d", name, dot, i) ==
				    -1)
					return (-1);
				if (structs_trav(t, ename, etype, edata) == -1) {
					free(ename);
					return (-1);
				}
				free(ename);
			}

			/* Done */
			return (0);
		}

	case STRUCTS_TYPE_STRUCTURE:
		{
			const struct structs_field *field;

			/* Iterate over structure fields */
			for (field = type->args[0].v; field->name != NULL;
			     field++) {
				const void *const edata =
				    (char *)data + field->offset;

				if (asprintf
				    (&ename, "%s%s%s", name, dot,
				     field->name) == -1)
					return (-1);
				if (structs_trav(t, ename, field->type, edata)
				    == -1) {
					free(ename);
					return (-1);
				}
				free(ename);
			}

			/* Done */
			return (0);
		}

	case STRUCTS_TYPE_UNION:
		{
			const struct structs_ufield *const fields =
			    type->args[0].v;
			const struct structs_union *const un = data;
			const struct structs_ufield *field;

			/* Find field */
			for (field = fields; field->name != NULL
			     && strcmp(un->field_name, field->name) != 0;
			     field++) ;
			if (field->name == NULL) {
				assert(0);
				errno = EINVAL;
				return (-1);
			}

			/* Do selected union field */
			if (asprintf(&ename, "%s%s%s", name, dot, field->name)
			    == -1)
				return (-1);
			if (structs_trav(t, ename, field->type, un->un) == -1) {
				free(ename);
				return (-1);
			}
			free(ename);

			/* Done */
			return (0);
		}

	default:
		assert(0);
		errno = EDOM;
		return (-1);
	}
}

/*******************************************************************************
 * BUILT-IN LOGGERS
 ******************************************************************************/

void structs_null_logger(int sev, const char *fmt, ...)
{
}

void structs_stderr_logger(int sev, const char *fmt, ...)
{
	static const char *const sevs[] = {
		"emerg", "alert", "crit", "err",
		"warning", "notice", "info", "debug"
	};
	static const int num_sevs = sizeof(sevs) / sizeof(*sevs);
	va_list args;

	va_start(args, fmt);
	if (sev < 0)
		sev = 0;
	if (sev >= num_sevs)
		sev = num_sevs - 1;
	fprintf(stderr, "%s: ", sevs[sev]);
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	va_end(args);
}

void structs_trace_logger(int sev, const char *fmt, ...)
{
}

/*******************************************************************************
 * END OF FILE
 ******************************************************************************/
