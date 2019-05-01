#ifndef _STRUCTS_TYPE_POINTER_H_
#define _STRUCTS_TYPE_POINTER_H_

/*******************************************************************************
 * POINTER TYPES
 ******************************************************************************/

/*
 * Structs type for a pointer to something.
 */

extern structs_init_t structs_pointer_init;
extern structs_copy_t structs_pointer_copy;
extern structs_equal_t structs_pointer_equal;
extern structs_ascify_t structs_pointer_ascify;
extern structs_binify_t structs_pointer_binify;
extern structs_encode_t structs_pointer_encode;
extern structs_decode_t structs_pointer_decode;
extern structs_uninit_t structs_pointer_free;

/*
 * Macro arguments:
 *  [const struct structs_type *] - Referent type
 */
#define STRUCTS_POINTER_TYPE(reftype) {				\
		sizeof(void *),					\
			"pointer",				\
			STRUCTS_TYPE_POINTER,			\
			structs_pointer_init,			\
			structs_pointer_copy,			\
			structs_pointer_equal,			\
			structs_pointer_ascify,			\
			structs_pointer_binify,			\
			structs_pointer_encode,			\
			structs_pointer_decode,			\
			structs_pointer_free,			\
		{ { (void *)(reftype) }, { NULL }, { NULL } }   \
	}

#endif /* _STRUCTS_TYPE_POINTER_H_ */
/*******************************************************************************
 * END OF FILE
 ******************************************************************************/
