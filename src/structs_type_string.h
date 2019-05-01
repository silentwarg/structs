#ifndef _STRUCTS_TYPE_STRING_H_
#define _STRUCTS_TYPE_STRING_H_

/*******************************************************************************
 * STRING TYPES
 ******************************************************************************/

/*
 * Dynamically allocated string types
 *
 * Type-specific arguments:
 *  [int]           Whether to store empty string as NULL or ""
 */
extern structs_init_t structs_string_init;
extern structs_equal_t structs_string_equal;
extern structs_ascify_t structs_string_ascify;
extern structs_binify_t structs_string_binify;
extern structs_encode_t structs_string_encode;
extern structs_decode_t structs_string_decode;
extern structs_uninit_t structs_string_free;

#define STRUCTS_STRING_TYPE(asnull) {				\
		sizeof(char *),					\
			"string",				\
			STRUCTS_TYPE_PRIMITIVE,			\
			structs_string_init,			\
			structs_ascii_copy,			\
			structs_string_equal,			\
			structs_string_ascify,			\
			structs_string_binify,			\
			structs_string_encode,			\
			structs_string_decode,			\
			structs_string_free,			\
		{ { (void *)(asnull) }, { NULL }, { NULL } }	\
	}

/* A string type with allocation type "structs_type_string" and never NULL */
extern const struct structs_type structs_type_string;

/* A string type with allocation type "structs_type_string" and can be NULL */
extern const struct structs_type structs_type_string_null;

/*******************************************************************************
 * BOUNDED LENGTH STRING TYPES
 ******************************************************************************/

/*
 * Bounded length string types
 *
 * Type-specific arguments:
 *  [int] Size of string buffer
 */
extern structs_equal_t structs_bstring_equal;
extern structs_ascify_t structs_bstring_ascify;
extern structs_binify_t structs_bstring_binify;

#define STRUCTS_FIXEDSTRING_TYPE(bufsize) {		\
		(bufsize),				\
			"fixedstring",			\
			STRUCTS_TYPE_PRIMITIVE,		\
			structs_region_init,		\
			structs_region_copy,		\
			structs_bstring_equal,		\
			structs_bstring_ascify,		\
			structs_bstring_binify,		\
			structs_string_encode,		\
			structs_string_decode,		\
			structs_nothing_free,		\
		{ { NULL }, { NULL }, { NULL } }	\
	}

#endif /* _STRUCTS_TYPE_STRING_H_ */
/*******************************************************************************
 * END OF FILE
 ******************************************************************************/
