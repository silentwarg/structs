#ifndef _STRUCTS_JSON_H_
#define _STRUCTS_JSON_H_

/*******************************************************************************
 * HEADERS
 ******************************************************************************/

/* Project Includes */
#include <json.h>

/*******************************************************************************
 * JSON API
 ******************************************************************************/

/*
 * Output a data structure as JSON.
 *
 * The JSON document element is an "elem_tag" element.
 *
 * Returns 0 if successful, otherwise -1 and sets errno.
 */
extern int structs_json_output(const struct structs_type *type,
			       const char *elem_tag, const void *data,
			       json_t * json);

/*
 * Parse JSON formatted data to structure.
 *
 * Returns 0 if successful, otherwise -1 and sets errno.
 */
extern int structs_json_input(const struct structs_type *type,
			      const char *elem_tag, void *data,
			      const char *input, size_t input_len,
			      structs_logger_t * logger);

/*
 * Get the JSON form of an item, in a string allocated.
 *
 * The caller is reponsible for freeing the returned JSON object.
 *
 * Returns the JSON object if successful, otherwise NULL and sets errno.
 */
extern json_t *structs_get_json(const struct structs_type *type,
				const char *name, const void *data);

#endif /* _STRUCTS_JSON_H_ */
/*******************************************************************************
 * END OF FILE
 ******************************************************************************/
