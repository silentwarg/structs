#ifndef _STRUCTS_TYPE_ARRAY_H_
#define _STRUCTS_TYPE_ARRAY_H_

/*******************************************************************************
 * HEADERS
 ******************************************************************************/

/* Module Includes */
#include "structs_type_array_define.h"

/*******************************************************************************
 * VARIABLE LENGTH ARRAYS
 ******************************************************************************/

/*
 * Macro arguments:
 *  [const struct structs_type *]    Element type
 *  [const char *]                    Tag for individual elements
 */
#define STRUCTS_ARRAY_TYPE(etype, etag) {				\
		sizeof(struct structs_array),				\
		"array",						\
		STRUCTS_TYPE_ARRAY,					\
		structs_region_init,					\
		structs_array_copy,					\
		structs_array_equal,					\
		structs_notsupp_ascify,					\
		structs_notsupp_binify,					\
		structs_array_encode,					\
		structs_array_decode,					\
		structs_array_free,					\
		{ { (void *)(etype) }, { (void *)(etag) }, { NULL } }	\
	}

extern structs_copy_t structs_array_copy;
extern structs_equal_t structs_array_equal;
extern structs_encode_t structs_array_encode;
extern structs_decode_t structs_array_decode;
extern structs_uninit_t structs_array_free;

/*
 * Additional functions for handling arrays
 */
extern int structs_array_length(const struct structs_type *type,
				const char *name, const void *data);
extern int structs_array_setsize(const struct structs_type *type,
				 const char *name, unsigned int nitems,
				 void *data, int do_init);
extern int structs_array_reset(const struct structs_type *type,
			       const char *name, void *data);
extern int structs_array_insert(const struct structs_type *type,
				const char *name, unsigned int indx,
				void *data);
extern int structs_array_delete(const struct structs_type *type,
				const char *name, unsigned int indx,
				void *data);
extern int structs_array_prep(const struct structs_type *type, const char *name,
			      void *data);

/*******************************************************************************
 * FIXED LENGTH ARRAYS
 ******************************************************************************/

/*
 * Fixed length array type.
 */

/*
 * Macro arguments:
 *  [const struct structs_type *]    Element type
 *  [unsigned int]             Size of each element
 *  [unsigned int]             Length of array
 *  [const char *]             Tag for individual elements
 */
#define STRUCTS_FIXEDARRAY_TYPE(etype, esize, alen, etag) {		\
		(esize) * (alen),					\
			"fixedarray",					\
			STRUCTS_TYPE_FIXEDARRAY,			\
			structs_fixedarray_init,			\
			structs_fixedarray_copy,			\
			structs_fixedarray_equal,			\
			structs_notsupp_ascify,				\
			structs_notsupp_binify,				\
			structs_fixedarray_encode,			\
			structs_fixedarray_decode,			\
			structs_fixedarray_free,			\
		{ { (void *)(etype) }, { (void *)(etag) }, { (void *)(alen) } } \
	}

extern structs_init_t structs_fixedarray_init;
extern structs_copy_t structs_fixedarray_copy;
extern structs_equal_t structs_fixedarray_equal;
extern structs_encode_t structs_fixedarray_encode;
extern structs_decode_t structs_fixedarray_decode;
extern structs_uninit_t structs_fixedarray_free;

#endif /* _STRUCTS_TYPE_ARRAY_H_ */
/*******************************************************************************
 * END OF FILE
 ******************************************************************************/
