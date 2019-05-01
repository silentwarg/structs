#ifndef _STRUCTS_TYPE_ARRAY_DEFINE_H_
#define _STRUCTS_TYPE_ARRAY_DEFINE_H_

/*******************************************************************************
 * HEADERS
 ******************************************************************************/

/* Standard Includes */
#include <sys/types.h>

/*******************************************************************************
 * MACROS/VARIABLES
 ******************************************************************************/

/*
 * The data must be a 'struct structs_array', or a structure
 * defined using the DEFINE_STRUCTS_ARRAY() macro.
 */
typedef struct structs_array {
	unsigned int length;	/* number of elements in array */
	void *elems;		/* array elements */
} structs_array;

/*
 * Use this to get 'elems' declared as the right type
 */
#define DEFINE_STRUCTS_ARRAY(name, etype)				\
	struct name {							\
		unsigned int   length;     /* number of elements in array */ \
		etype   *elems;     /* array elements */                \
	}

#define DEFINE_STRUCTS_ARRAY_T(name, etype)				\
	typedef struct name {						\
		unsigned int   length;     /* number of elements in array */ \
		etype   *elems;     /* array elements */                \
	} name

#define DEFINE_STRUCTS_CARRAY(name, etype)				\
	struct name {							\
		unsigned int   length;     /* number of elements in array */ \
		const etype *elems; /* array elements */                \
	}

#define DEFINE_STRUCTS_CARRAY_T(name, etype)				\
	typedef struct name {						\
		unsigned int   length;     /* number of elements in array */ \
		const etype *elems; /* array elements */                \
	} name

#endif /* _STRUCTS_TYPE_ARRAY_DEFINE_H_ */
/*******************************************************************************
 * END OF FILE
 ******************************************************************************/
