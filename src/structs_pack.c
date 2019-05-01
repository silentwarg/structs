/*******************************************************************************
 * HEADERS
 ******************************************************************************/

/* Standard Includes */
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <ctype.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

/* Project Includes */
#include <msgpack.h>

/* Module Includes */
#include "structs.h"
#include "structs_type_array.h"
#include "structs_type_struct.h"
#include "structs_type_union.h"

/*******************************************************************************
 * MACROS/VARIABLES
 ******************************************************************************/

/* Max parse depth */
#define MAX_UNPACK_STACK 32

/* Parse private context */
struct unpack_stackframe {
	/* type we're parsing */
	const struct structs_type *type;
	char *name;		/* element name */
	void *data;		/* data pointer */
	char *value;		/* character data */
	unsigned int value_len;	/* strlen(value) */
	unsigned int index;	/* fixed array index */
};

struct unpack_info {
	int error;
	int depth;
	const char *elem_tag;
	struct unpack_stackframe stack[MAX_UNPACK_STACK];
	structs_logger_t *logger;
};

/*******************************************************************************
 * FUNCTION DECLARATIONS
 ******************************************************************************/

/* Pack functions */
static int structs_pack_sub(const struct structs_type *type,
			    const void *data, const char *tag,
			    msgpack_packer * pk, const char **elems,
			    int is_array);

/* Unpack functions */
static void structs_unpack_object(struct unpack_info *info,
				  msgpack_object * obj);
static void structs_unpack_start(struct unpack_info *info, const char *key,
				 int key_len);
static void structs_unpack_end(struct unpack_info *info);
static void structs_unpack_next(struct unpack_info *info,
				const struct structs_type *type, void *data);
static void structs_unpack_nest(struct unpack_info *info,
				const struct structs_type **typep,
				void **datap);
static void structs_unpack_str_value(struct unpack_info *info,
				     const char *s, int len);
static void structs_unpack_unnest(struct unpack_info *info);
static void structs_unpack_pop(struct unpack_info *info);

/*******************************************************************************
 * FUNCTION DEFINITIONS
 ******************************************************************************/

/*
 * Output a structure in MSGPACK format
 */
int
structs_pack(const struct structs_type *type,
	     const char *elem_tag, const void *data, msgpack_packer * pk)
{
	static const char *all[] = { "", NULL };

	if ((!type) || (!elem_tag) || (!data) || (!pk)) {
		return (-1);
	}

	msgpack_pack_map(pk, 1);
	/* Output structure, and always show the opening and closing tags */
	return (structs_pack_sub(type, data, elem_tag, pk, all, 0));
}

/*
 * Output a sub-structure in MSGPACK
 */
static int
structs_pack_sub(const struct structs_type *type,
		 const void *data, const char *tag,
		 msgpack_packer * pk, const char **elems, int is_array)
{
	int r = 0;

	/* Dereference through pointer(s) */
	while (type->tclass == STRUCTS_TYPE_POINTER) {
		type = type->args[0].v;
		data = *((void **)data);
	}

	/* Let's not duplicate key names for each array element. */
	if (!is_array) {
		msgpack_pack_str(pk, strlen(tag));
		msgpack_pack_str_body(pk, tag, strlen(tag));
	}

	/* Output element */
	switch (type->tclass) {
	case STRUCTS_TYPE_UNION:
		{
			const struct structs_union *const un = data;
			const struct structs_ufield *const fields =
			    type->args[0].v;
			const struct structs_ufield *field;

			/* Find field */
			for (field = fields; field->name != NULL
			     && strcmp(un->field_name, field->name) != 0;
			     field++) ;
			if (field->name == NULL)
				assert(0);

			msgpack_pack_map(pk, 1);
			/* Output chosen union field */
			r = structs_pack_sub(field->type, un->un,
					     field->name, pk, elems, 0);
			break;
		}

	case STRUCTS_TYPE_STRUCTURE:
		{
			const struct structs_field *field;
			int num_elems = 0;

			for (field = type->args[0].v;
			     field->name != NULL; field++) {
				num_elems++;
			}

			msgpack_pack_map(pk, num_elems);
			/* Do each structure field */
			for (field = type->args[0].v; field->name != NULL;
			     field++) {
				/* Do structure field */
				r = structs_pack_sub(field->type,
						     (char *)data +
						     field->offset,
						     field->name, pk, elems, 0);
				if (r == -1)
					break;
			}
			break;
		}

	case STRUCTS_TYPE_ARRAY:
		{
			const struct structs_type *const etype =
			    type->args[0].v;
			const char *elem_name = type->args[1].s;
			const struct structs_array *const ary = data;
			int i;

			msgpack_pack_array(pk, ary->length);
			/* Do elements in order */
			for (i = 0; i < ary->length; i++) {
				/* Output array element */
				r = structs_pack_sub(etype,
						     (char *)ary->elems
						     +
						     (i * etype->size),
						     elem_name, pk, elems, 1);
				if (r == -1)
					break;
			}
			break;
		}

	case STRUCTS_TYPE_FIXEDARRAY:
		{
			const struct structs_type *const etype =
			    type->args[0].v;
			const char *elem_name = type->args[1].s;
			const unsigned int length = type->args[2].i;
			unsigned int i;

			msgpack_pack_array(pk, length);
			/* Do elements in order */
			for (i = 0; i < length; i++) {
				r = structs_pack_sub(etype, (char *)data
						     +
						     (i * etype->size),
						     elem_name, pk, elems, 1);
				if (r == -1)
					break;
			}
			break;
		}

	case STRUCTS_TYPE_PRIMITIVE:
		{
			char *ascii;

			/* Get ascii string */
			if ((ascii = (*type->ascify) (type, data)) == NULL)
				return (-1);

			msgpack_pack_str(pk, strlen(ascii));
			msgpack_pack_str_body(pk, ascii, strlen(ascii));

			free(ascii);
			break;
		}

	default:
		assert(0);
	}
	return (r);
}

/*
 * Unpack MSGPACK format data to a structure
 */
int structs_unpack(const struct structs_type *type,
		   const char *elem_tag, void *data,
		   const char *input, size_t input_len,
		   structs_logger_t * logger)
{
	struct unpack_info *info = NULL;
	int esave, data_init = 0, retval = 0;
	msgpack_unpacked result;
	msgpack_unpack_return mpretval;
	msgpack_object obj;
	size_t off = 0;

	/* Special cases for logger */
	if (logger == STRUCTS_LOGGER_TRACE)
		logger = structs_trace_logger;
	else if (logger == STRUCTS_LOGGER_STDERR)
		logger = structs_stderr_logger;
	else
		logger = structs_null_logger;

	if ((!type) || (!elem_tag) || (!data) || (!input) || (input_len <= 0)) {
		return (-1);
	}

	/* Initialize data object if desired */
	if ((*type->init) (type, data) == -1) {
		esave = errno;
		(*logger) (LOG_ERR, "error initializing data: %s",
			   strerror(errno));
		errno = esave;
		retval = -1;
		goto done;
	}
	data_init = 1;

	/* Allocate info structure */
	if ((info = calloc(1, sizeof(*info))) == NULL) {
		esave = errno;
		(*logger) (LOG_ERR, "%s: %s", "calloc", strerror(errno));
		errno = esave;
		retval = -1;
		goto done;
	}
	info->logger = logger;
	info->elem_tag = elem_tag;
	info->stack[0].type = type;
	info->stack[0].data = data;

	/* Unpack the data */
	msgpack_unpacked_init(&result);
	mpretval = msgpack_unpack_next(&result, input, input_len, &off);
	if (mpretval != MSGPACK_UNPACK_SUCCESS) {
		msgpack_unpacked_destroy(&result);
		(*logger) (LOG_ERR, "error while unpacking data");
		retval = -1;
		goto done;
	}

	/* Decode the object and update the structure */
	structs_unpack_object(info, &result.data);

	/* Free the result */
	msgpack_unpacked_destroy(&result);
	if (info->error) {
		retval = -1;
		goto done;
	}

done:
	esave = errno;
	/* Free private parse info */
	if (info != NULL) {
		while (info->depth >= 0)
			structs_unpack_pop(info);
		free(info);
	}

	/* If error, free initialized data */
	//if ((retval != 0) && (data_init)) {
	//      (*type->uninit) (type, data);
	//}
	errno = esave;
	return (retval);
}

static void structs_unpack_object(struct unpack_info *info,
				  msgpack_object * obj)
{
	switch (obj->type) {
	case MSGPACK_OBJECT_STR:
		structs_unpack_str_value(info, obj->via.str.ptr,
					 obj->via.str.size);
		break;

	case MSGPACK_OBJECT_ARRAY:
		if (obj->via.array.size != 0) {
			msgpack_object *p = obj->via.array.ptr;
			msgpack_object *const pend =
			    obj->via.array.ptr + obj->via.array.size;
			for (; p < pend; ++p) {
				structs_unpack_start(info, NULL, 0);
				structs_unpack_object(info, p);
				structs_unpack_end(info);
			}
			if ((info->depth > 0) &&
			    (info->stack[info->depth - 1].name != NULL)) {
				free(info->stack[info->depth - 1].name);
				info->stack[info->depth - 1].name = NULL;
			}
		}
		break;

	case MSGPACK_OBJECT_MAP:
		if (obj->via.map.size != 0) {
			msgpack_object_kv *p = obj->via.map.ptr;
			msgpack_object_kv *const pend =
			    obj->via.map.ptr + obj->via.map.size;
			for (; p < pend; ++p) {
				structs_unpack_start(info, p->key.via.str.ptr,
						     p->key.via.str.size);
				structs_unpack_object(info, &p->val);
				structs_unpack_end(info);
			}
		}
		break;

	case MSGPACK_OBJECT_NIL:
	case MSGPACK_OBJECT_BOOLEAN:
	case MSGPACK_OBJECT_POSITIVE_INTEGER:
	case MSGPACK_OBJECT_NEGATIVE_INTEGER:
	case MSGPACK_OBJECT_FLOAT:
	case MSGPACK_OBJECT_BIN:
	case MSGPACK_OBJECT_EXT:
	default:
		break;
	}

	return;
}

static void structs_unpack_start(struct unpack_info *info,
				 const char *key, int key_len)
{
	struct unpack_stackframe *const frame = &info->stack[info->depth];
	const struct structs_type *type = frame->type;
	void *data = frame->data;
	void *mem;

	/* Skip if any errors */
	if (info->error != 0)
		return;

	if (key != NULL) {
		if ((mem = calloc(1, key_len + 1)) == NULL) {
			info->error = errno;
			(*info->logger) (LOG_ERR, "%s: %s", "calloc",
					 strerror(errno));
			return;
		}
		frame->name = mem;
		memcpy(frame->name, key, key_len);
		frame->name[key_len] = '\0';
	} else {
		if (info->depth == 0) {
			info->error = EINVAL;
			return;
		}
		struct unpack_stackframe *const last_frame =
		    &info->stack[info->depth - 1];
		frame->name = strdup(last_frame->name);
	}

	/* Handle the top level structure specially */
	if (info->depth == 0) {
		/* The top level tag must match what we expect */
		if (strcmp(frame->name, info->elem_tag) != 0) {
			(*info->logger) (LOG_ERR,
					 "expecting element \"%s\" here",
					 info->elem_tag);
			info->error = EINVAL;
			goto done;
		}
		/* Prep the top level data structure */
		structs_unpack_next(info, type, data);
		goto done;
	}

	structs_unpack_nest(info, &type, &data);
	if (info->error != 0)
		goto done;
	structs_unpack_next(info, type, data);
	if (info->error != 0)
		goto done;
	if ((type->tclass == STRUCTS_TYPE_ARRAY) ||
	    (type->tclass == STRUCTS_TYPE_FIXEDARRAY))
		return;
done:
	free(frame->name);
	frame->name = NULL;
	return;
}

static void structs_unpack_end(struct unpack_info *info)
{
	structs_unpack_unnest(info);
}

static void structs_unpack_next(struct unpack_info *info,
				const struct structs_type *type, void *data)
{
	/* Dereference through pointer(s) */
	while (type->tclass == STRUCTS_TYPE_POINTER) {
		type = type->args[0].v;
		data = *((void **)data);
	}

	/* If next item is an array, re-initialize it */
	switch (type->tclass) {
	case STRUCTS_TYPE_ARRAY:
		(*type->uninit) (type, data);
		memset(data, 0, type->size);
		break;
	case STRUCTS_TYPE_FIXEDARRAY:
		{
			void *mem;

			/* Get temporary region for newly initialized array */
			if ((mem = calloc(1, type->size)) == NULL) {
				info->error = errno;
				(*info->logger) (LOG_ERR, "%s: %s",
						 "error initializing new array",
						 strerror(errno));
				return;
			}

			/* Initialize new array */
			if ((*type->init) (type, mem) == -1) {
				info->error = errno;
				(*info->logger) (LOG_ERR, "%s: %s",
						 "error initializing new array",
						 strerror(errno));
				free(mem);
				return;
			}

			/* Replace existing array with fresh one */
			(*type->uninit) (type, data);
			memcpy(data, mem, type->size);
			free(mem);

			/* Remember that we're on the first element */
			info->stack[info->depth + 1].index = 0;
			break;
		}
	default:
		break;
	}

	/* Check stack overflow */
	if (info->depth == MAX_UNPACK_STACK - 1) {
		(*info->logger) (LOG_ERR,
				 "maximum parse stack depth (%d) exceeded",
				 MAX_UNPACK_STACK);
		info->error = EMLINK;
		return;
	}

	/* Continue in a new stack frame */
	info->depth++;
	info->stack[info->depth].type = type;
	info->stack[info->depth].data = data;
}

static void structs_unpack_nest(struct unpack_info *info,
				const struct structs_type **typep, void **datap)
{
	struct unpack_stackframe *const frame = &info->stack[info->depth];
	const struct structs_type *type;
	const char *name = frame->name;
	void *data;

	/* Check type type */
	switch (frame->type->tclass) {
	case STRUCTS_TYPE_STRUCTURE:
	case STRUCTS_TYPE_UNION:
		{
			/* Find field; for unions, adjust the field type if necessary */
			type = frame->type;
			data = frame->data;
			if ((type =
			     structs_find(type, name, (const void **)&data,
					  1)) == NULL) {
				if (errno == ENOENT) {
					(*info->logger) (LOG_ERR,
							 "element \"%s\" is not"
							 " expected here",
							 name);
					info->error = EINVAL;
					return;
				}
				(*info->logger) (LOG_ERR, "error"
						 " initializing union field \"%s\": %s",
						 name, strerror(errno));
				info->error = errno;
				return;
			}
			break;
		}

	case STRUCTS_TYPE_ARRAY:
		{
			const struct structs_type *const etype =
			    frame->type->args[0].v;
			const char *elem_name = frame->type->args[1].s;
			struct structs_array *const ary = frame->data;
			void *mem;

			/* Expand the array by one */
			if ((mem =
			     realloc(ary->elems,
				     (ary->length + 1) * etype->size)) ==
			    NULL) {
				info->error = errno;
				(*info->logger) (LOG_ERR, "%s: %s",
						 "realloc", strerror(errno));
				return;
			}
			ary->elems = mem;

			/* Initialize the new element */
			memset((char *)ary->elems + (ary->length * etype->size),
			       0, etype->size);
			if ((*etype->init) (etype,
					    (char *)ary->elems +
					    (ary->length * etype->size)) ==
			    -1) {
				info->error = errno;
				(*info->logger) (LOG_ERR, "%s: %s",
						 "error initializing new array element",
						 strerror(errno));
				return;
			}

			/* Parse the element next */
			type = etype;
			data = (char *)ary->elems + (ary->length * etype->size);
			ary->length++;
			break;
		}

	case STRUCTS_TYPE_FIXEDARRAY:
		{
			const struct structs_type *const etype =
			    frame->type->args[0].v;
			const char *elem_name = frame->type->args[1].s;
			const unsigned int length = frame->type->args[2].i;

			/* Check index vs. array length */
			if (frame->index >= length) {
				(*info->logger) (LOG_ERR, "too many"
						 " elements in fixed array (length %u)",
						 length);
				info->error = EINVAL;
				return;
			}

			/* Parse the element next */
			type = etype;
			data =
			    (char *)frame->data + (frame->index * etype->size);
			frame->index++;
			break;
		}

	case STRUCTS_TYPE_PRIMITIVE:
		(*info->logger) (LOG_ERR,
				 "element \"%s\" is not expected here", name);
		info->error = EINVAL;
		return;

	default:
		assert(0);
		return;
	}

	/* Done */
	*typep = type;
	*datap = data;
}

static void structs_unpack_str_value(struct unpack_info *info,
				     const char *s, int len)
{
	struct unpack_stackframe *const frame = &info->stack[info->depth];
	void *mem;

	/* Skip if any errors */
	if (info->error)
		return;

	/* Expand buffer and append character data */
	if ((mem = realloc(frame->value, frame->value_len + len + 1)) == NULL) {
		info->error = errno;
		(*info->logger) (LOG_ERR, "%s: %s", "realloc", strerror(errno));
		return;
	}
	frame->value = mem;
	memcpy(frame->value + frame->value_len, (char *)s, len);
	frame->value[frame->value_len + len] = '\0';
	frame->value_len += len;
}

static void structs_unpack_unnest(struct unpack_info *info)
{
	struct unpack_stackframe *const frame = &info->stack[info->depth];
	const struct structs_type *type;
	const char *name = frame->name;
	const char *s;
	char ebuf[64];
	void *data;

	/* Skip if any errors */
	if (info->error)
		return;

	/* Get current type and data */
	data = frame->data;
	type = frame->type;

	/*
	 * Convert from ASCII if possible, otherwise check only whitespace.
	 * For unions, we allow the field name tag to be omitted if you
	 * want to use the default field, which must have primitive type.
	 */
	switch (type->tclass) {
	case STRUCTS_TYPE_UNION:
		{
			const struct structs_ufield *const field =
			    type->args[0].v;

			/* Check to see if there's any non-whitespace text */
			if (frame->value == NULL)
				goto done;
			for (s = frame->value; *s != '\0' && isspace(*s); s++) ;
			if (*s == '\0')
				goto done;

			/* Default field must have primitive type */
			if (field->type->tclass != STRUCTS_TYPE_PRIMITIVE)
				break;

			/* Switch the union to the default field */
			if (structs_union_set(type, NULL, data, field->name) ==
			    -1) {
				info->error = errno;
				(*info->logger) (LOG_ERR,
						 "%s: %s", "structs_union_set",
						 strerror(errno));
				return;
			}

			/* Point at the field instead of the union */
			type = field->type;
			data = ((const struct structs_union *)data)->un;

			/* FALLTHROUGH */
		}
	case STRUCTS_TYPE_PRIMITIVE:
		if (structs_set_string(type, NULL,
				       frame->value, data, ebuf,
				       sizeof(ebuf)) == -1) {
			info->error = errno;
			(*info->logger) (LOG_ERR,
					 "error in \"%s\" element data"
					 " \"%s\": %s",
					 name,
					 frame->value ==
					 NULL ? "" : frame->value, ebuf);
			return;
		}
		goto done;
	default:
		break;
	}

	/* There shouldn't be any non-whitespace text here */
	if (frame->value != NULL) {
		for (s = frame->value; *s != '\0' && isspace(*s); s++) ;
		if (*s != '\0') {
			(*info->logger) (LOG_ERR,
					 "extra garbage within \"%s\" element",
					 name);
			info->error = EINVAL;
			return;
		}
	}

done:
	structs_unpack_pop(info);
}

static void structs_unpack_pop(struct unpack_info *info)
{
	assert(info->depth >= 0);
	struct unpack_stackframe *const frame = &info->stack[info->depth];
	if (frame->value != NULL)
		free(frame->value);
	memset(frame, 0, sizeof(*frame));
	info->depth--;
}

/*******************************************************************************
 * END OF FILE
 ******************************************************************************/
