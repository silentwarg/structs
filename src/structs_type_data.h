#ifndef _STRUCTS_TYPE_DATA_H_
#define _STRUCTS_TYPE_DATA_H_

/*******************************************************************************
 * HEADERS
 ******************************************************************************/

/* Standard Includes */

/* Module Includes */

/*******************************************************************************
 * BINARY DATA TYPE
 ******************************************************************************/

/*
 * The data is a "struct structs_data".
 *
 * The ASCII form is base-64 or hex encoded.
 */

extern structs_copy_t structs_data_copy;
extern structs_equal_t structs_data_equal;
extern structs_ascify_t structs_data_ascify;
extern structs_binify_t structs_data_binify;
extern structs_encode_t structs_data_encode;
extern structs_decode_t structs_data_decode;
extern structs_uninit_t structs_data_free;

/*
 * Macro arguments:
 *  [const char *]      Encoding character set (or NULL for default
 *                      or empty string for hex encoding)
 *  [const char *]      Memory allocation type for byte buffer
 */
#define STRUCTS_DATA_TYPE(charset) {				\
		sizeof(struct structs_data),			\
			"data",					\
			STRUCTS_TYPE_PRIMITIVE,			\
			structs_region_init,			\
			structs_data_copy,			\
			structs_data_equal,			\
			structs_data_ascify,			\
			structs_data_binify,			\
			structs_data_encode,			\
			structs_data_decode,			\
			structs_data_free,			\
		{ { (void *)(charset) }, { NULL }, { NULL } }   \
	}

/*
 * Built-in type using default charset.
 */
extern const struct structs_type structs_type_data;

/*
 * Built-in type using hex ASCII encoding.
 */
extern const struct structs_type structs_type_hexdata;

/*******************************************************************************
 * FIXED LENGTH BINARY DATA TYPE
 ******************************************************************************/

/*
 * The data is a fixed length array of unsigned char.
 *
 * The ASCII form is (length * 2) hex digits.
 */
extern structs_ascify_t structs_fixeddata_ascify;
extern structs_binify_t structs_fixeddata_binify;
extern structs_encode_t structs_fixeddata_encode;
extern structs_decode_t structs_fixeddata_decode;

/*
 * Macro arguments:
 *  [int]       Length of array.
 */
#define STRUCTS_FIXEDDATA_TYPE(length) {		\
		(length),                               \
			"fixeddata",			\
			STRUCTS_TYPE_PRIMITIVE,		\
			structs_region_init,		\
			structs_region_copy,		\
			structs_region_equal,		\
			structs_fixeddata_ascify,	\
			structs_fixeddata_binify,	\
			structs_fixeddata_encode,	\
			structs_fixeddata_decode,	\
			structs_nothing_free,		\
		{ { NULL }, { NULL }, { NULL } }	\
	}

#endif /* _STRUCTS_TYPE_DATA_H_ */
/*******************************************************************************
 * END OF FILE
 ******************************************************************************/
