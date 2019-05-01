#ifndef _STRUCTS_PACK_H_
#define _STRUCTS_PACK_H_

/*******************************************************************************
 * HEADERS
 ******************************************************************************/

/* Project Includes */
#include <msgpack.h>

/*******************************************************************************
 * MSGPACK API
 ******************************************************************************/

/*
 * Output a data structure as MSGPACK format.
 *
 * The MSGPACK document element is an "elem_tag" element.
 *
 * If "elems" is non-NULL, it must point to a NULL terminated list of
 * elements to output. Only elements appearing in the list are output.
 *
 * Returns 0 if successful, otherwise -1 and sets errno.
 */
extern int structs_pack(const struct structs_type *type,
			const char *elem_tag, const void *data,
			msgpack_packer * pk);

/*
 * Parse MSGPACK formatted data to structure.
 *
 * Returns 0 if successful, otherwise -1 and sets errno.
 */
extern int structs_unpack(const struct structs_type *type,
			  const char *elem_tag, void *data,
			  const char *input, size_t input_len,
			  structs_logger_t * logger);

#endif /* _STRUCTS_MSGPACK_H_ */
/*******************************************************************************
 * END OF FILE
 ******************************************************************************/
