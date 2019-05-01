/*******************************************************************************
 * HEADERS
 ******************************************************************************/

/* Standard Includes */
#include <sys/types.h>
#include <net/ethernet.h>
#include <netinet/in.h>

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
#include <json.h>

/* Module Includes */
#include "structs.h"
#include "structs_type_array.h"
#include "structs_type_struct.h"
#include "structs_type_union.h"

/*******************************************************************************
 * MACROS/VARIABLES
 ******************************************************************************/

/*******************************************************************************
 * FUNCTION DECLARATIONS
 ******************************************************************************/

/* Max parse depth */
#define MAX_JSON_INPUT_STACK 32

/* Parse private context */
struct json_input_stackframe {
	/* type we're parsing */
	const struct structs_type *type;
	char *name;		/* element name */
	void *data;		/* data pointer */
	char *value;		/* character data */
	unsigned int value_len;	/* strlen(value) */
	unsigned int index;	/* fixed array index */
};

struct json_input_info {
	int error;
	int depth;
	const char *elem_tag;
	struct json_input_stackframe stack[MAX_JSON_INPUT_STACK];
	structs_logger_t *logger;
};

#define P_JSON_SET(json, tag, elem)				\
	do {							\
		if (json_typeof(json) == JSON_ARRAY)		\
			json_array_append_new(json, elem);	\
		else						\
			json_object_set_new(json, tag, elem);	\
	} while(0)

/*******************************************************************************
 * FUNCTION DEFINITIONS
 ******************************************************************************/

/* Output functions */
static int structs_json_output_sub(const struct structs_type *type,
				   const void *data, const char *tag,
				   json_t * json, const char **elems);

/* Input functions */
static void structs_json_input_data(struct json_input_info *info, json_t * obj);
static void structs_json_input_start(struct json_input_info *info,
				     const char *key, int key_len);
static void structs_json_input_end(struct json_input_info *info);
static void structs_json_input_next(struct json_input_info *info,
				    const struct structs_type *type,
				    void *data);
static void structs_json_input_nest(struct json_input_info *info,
				    const struct structs_type **typep,
				    void **datap);
static void structs_json_input_str_value(struct json_input_info *info,
					 const char *s, int len);
static void structs_json_input_unnest(struct json_input_info *info);
static void structs_json_input_pop(struct json_input_info *info);

/*******************************************************************************
 * JSON OUTPUT ROUTINES
 ******************************************************************************/

/*
 * Output a structure in JSON
 */
int structs_json_output(const struct structs_type *type,
			const char *elem_tag, const void *data, json_t * json)
{
	static const char *all[] = { "", NULL };

	if ((!type) || (!elem_tag) || (!data) || (!json)) {
		return (-1);
	}

	/* Output structure, and always
	   show the opening and closing tags */
	return (structs_json_output_sub(type, data, elem_tag, json, all));
}

/*
 * Output a sub-structure in JSON
 */
static int structs_json_output_sub(const struct structs_type *type,
				   const void *data, const char *tag,
				   json_t * json, const char **elems)
{
	int r = 0;

	/* Dereference through pointer(s) */
	while (type->tclass == STRUCTS_TYPE_POINTER) {
		type = type->args[0].v;
		data = *((void **)data);
	}

	/* Output element */
	switch (type->tclass) {
	case STRUCTS_TYPE_UNION:
		{
			const struct structs_union *const un = data;
			const struct structs_ufield *const fields =
			    type->args[0].v;
			const struct structs_ufield *field;
			json_t *jsonu;

			/* Find field */
			for (field = fields; field->name != NULL
			     && strcmp(un->field_name, field->name) != 0;
			     field++) ;
			if (field->name == NULL)
				assert(0);

			jsonu = json_object();
			/* Output chosen union field */
			r = structs_json_output_sub(field->type, un->un,
						    field->name, jsonu, elems);
			if (r == -1) {
				json_decref(jsonu);
				break;
			}

			P_JSON_SET(json, tag, jsonu);
			break;
		}

	case STRUCTS_TYPE_STRUCTURE:
		{
			const struct structs_field *field;
			json_t *jsons;

			jsons = json_object();

			/* Do each structure field */
			for (field = type->args[0].v; field->name != NULL;
			     field++) {
				/* Do structure field */
				r = structs_json_output_sub(field->type,
							    (char *)data +
							    field->offset,
							    field->name, jsons,
							    elems);
				/* Bail out if there was an error */
				if (r == -1) {
					json_decref(jsons);
					break;
				}
			}

			P_JSON_SET(json, tag, jsons);
			break;
		}

	case STRUCTS_TYPE_ARRAY:
		{
			const struct structs_type *const etype =
			    type->args[0].v;
			const char *elem_name = type->args[1].s;
			const struct structs_array *const ary = data;
			json_t *jsonarr;
			int i;

			jsonarr = json_array();

			/* Do elements in order */
			for (i = 0; i < ary->length; i++) {
				/* Output array element */
				r = structs_json_output_sub(etype,
							    (char *)ary->elems
							    +
							    (i * etype->size),
							    elem_name, jsonarr,
							    elems);

				/* Bail out if there was an error */
				if (r == -1) {
					json_decref(jsonarr);
					break;
				}
			}

			P_JSON_SET(json, tag, jsonarr);
			break;
		}

	case STRUCTS_TYPE_FIXEDARRAY:
		{
			const struct structs_type *const etype =
			    type->args[0].v;
			const char *elem_name = type->args[1].s;
			const unsigned int length = type->args[2].i;
			json_t *jsonarr;
			unsigned int i;

			jsonarr = json_array();

			/* Do elements in order */
			for (i = 0; i < length; i++) {
				r = structs_json_output_sub(etype, (char *)data
							    +
							    (i * etype->size),
							    elem_name, jsonarr,
							    elems);
				/* Bail out if there was an error */
				if (r == -1) {
					json_decref(jsonarr);
					break;
				}
			}

			P_JSON_SET(json, tag, jsonarr);
			break;
		}

	case STRUCTS_TYPE_PRIMITIVE:
		{
			char *ascii;

			/* Get ascii string */
			if ((ascii = (*type->ascify) (type, data)) == NULL)
				return (-1);

			if ((strstr(type->name, "int") != NULL) ||
			    (strstr(type->name, "uint") != NULL) ||
			    (strstr(type->name, "hint") != NULL)) {
				json_int_t val = strtoll(ascii, NULL, 0);
				P_JSON_SET(json, tag, json_integer(val));
			} else if (!strncmp(type->name, "float", 5) ||
				   !strncmp(type->name, "double", 6)) {
				double val = strtod(ascii, NULL);
				P_JSON_SET(json, tag, json_real(val));
			} else if (!strncmp(type->name, "boolean", 7)) {
				int val = type->args[0].i ?
				    *((unsigned int *)data) :
				    *((unsigned char *)data);
				P_JSON_SET(json, tag, json_boolean(val));
			} else {
				P_JSON_SET(json, tag, json_string(ascii));
			}

			free(ascii);
			break;
		}

	default:
		assert(0);
	}
	return (r);
}

/*
 * Parse JSON format data to a structure
 */
int structs_json_input(const struct structs_type *type,
		       const char *elem_tag, void *data,
		       const char *input, size_t input_len,
		       structs_logger_t * logger)
{
	struct json_input_info *info;
	int esave, data_init = 0, retval = 0;
	json_t *result;
	json_error_t error;

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

	/* Parse the JSON data */
	result = json_loadb(input, input_len, 0, &error);
	if (result == NULL) {
		(*logger) (LOG_ERR,
			   "error while parsing JSON data: %s", error.text);
		retval = -1;
		goto done;
	}

	/* Decode the object and update the structure */
	structs_json_input_data(info, result);

	/* Free the result */
	json_decref(result);
	if (info->error) {
		retval = -1;
		goto done;
	}

done:
	esave = errno;
	/* Free private parse info */
	if (info != NULL) {
		while (info->depth >= 0)
			structs_json_input_pop(info);
		free(info);
	}

	/* If error, free initialized data */
	//if ((retval != 0) && (data_init)) {
	//      (*type->uninit) (type, data);
	//}
	errno = esave;
	return (retval);
}

static void structs_json_input_data(struct json_input_info *info, json_t * obj)
{
	json_t *jvalue;
	size_t index, length;
	char buf[32];
	const char *key, *str;

	switch (json_typeof(obj)) {
	case JSON_OBJECT:
		length = json_object_size(obj);
		if (length != 0) {
			json_object_foreach(obj, key, jvalue) {
				structs_json_input_start(info, key,
							 strlen(key));
				structs_json_input_data(info, jvalue);
				structs_json_input_end(info);
			}
		}
		break;

	case JSON_ARRAY:
		length = json_array_size(obj);
		if (length != 0) {
			json_array_foreach(obj, index, jvalue) {
				structs_json_input_start(info, NULL, 0);
				structs_json_input_data(info, jvalue);
				structs_json_input_end(info);
			}
			if ((info->depth > 0) &&
			    (info->stack[info->depth - 1].name != NULL)) {
				free(info->stack[info->depth - 1].name);
				info->stack[info->depth - 1].name = NULL;
			}
		}
		break;

	case JSON_STRING:
		str = json_string_value(obj);
		structs_json_input_str_value(info, str, strlen(str));
		break;

	case JSON_INTEGER:
		snprintf(buf, sizeof(buf), "%lld",
			 (long long)json_integer_value(obj));
		structs_json_input_str_value(info, buf, strlen(buf));
		break;

	case JSON_REAL:
		snprintf(buf, sizeof(buf), "%g", (double)json_real_value(obj));
		structs_json_input_str_value(info, buf, strlen(buf));
		break;

	case JSON_TRUE:
		snprintf(buf, sizeof(buf), "1");
		structs_json_input_str_value(info, buf, strlen(buf));
		break;

	case JSON_FALSE:
		snprintf(buf, sizeof(buf), "0");
		structs_json_input_str_value(info, buf, strlen(buf));
		break;

	case JSON_NULL:
	default:
		break;
	}

	return;
}

static void structs_json_input_start(struct json_input_info *info,
				     const char *key, int key_len)
{
	struct json_input_stackframe *const frame = &info->stack[info->depth];
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
		struct json_input_stackframe *const last_frame =
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
		structs_json_input_next(info, type, data);
		goto done;
	}

	structs_json_input_nest(info, &type, &data);
	if (info->error != 0)
		goto done;
	structs_json_input_next(info, type, data);
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

static void structs_json_input_end(struct json_input_info *info)
{
	structs_json_input_unnest(info);
}

static void structs_json_input_next(struct json_input_info *info,
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
	if (info->depth == MAX_JSON_INPUT_STACK - 1) {
		(*info->logger) (LOG_ERR,
				 "maximum parse stack depth (%d) exceeded",
				 MAX_JSON_INPUT_STACK);
		info->error = EMLINK;
		return;
	}

	/* Continue in a new stack frame */
	info->depth++;
	info->stack[info->depth].type = type;
	info->stack[info->depth].data = data;
}

static void structs_json_input_nest(struct json_input_info *info,
				    const struct structs_type **typep,
				    void **datap)
{
	struct json_input_stackframe *const frame = &info->stack[info->depth];
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

static void structs_json_input_str_value(struct json_input_info *info,
					 const char *s, int len)
{
	struct json_input_stackframe *const frame = &info->stack[info->depth];
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

static void structs_json_input_unnest(struct json_input_info *info)
{
	struct json_input_stackframe *const frame = &info->stack[info->depth];
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
	structs_json_input_pop(info);
}

static void structs_json_input_pop(struct json_input_info *info)
{
	assert(info->depth >= 0);
	struct json_input_stackframe *const frame = &info->stack[info->depth];
	if (frame->value != NULL)
		free(frame->value);
	memset(frame, 0, sizeof(*frame));
	info->depth--;
}

/*
 * Get the JSON form of an item.
 */
json_t *structs_get_json(const struct structs_type *type,
			 const char *name, const void *data)
{
	int retval;
	json_t *json = json_object();

	/* Find item */
	if ((type = structs_find(type, name, (const void **)&data, 0)) == NULL)
		return (NULL);

	/* Get JSON */
	retval = structs_json_output(type, name, data, json);
	if (retval == -1) {
		return (NULL);
	}

	return json;
}

/*******************************************************************************
 * END OF FILE
 ******************************************************************************/
