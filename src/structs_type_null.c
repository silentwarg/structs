/*******************************************************************************
 * HEADERS
 ******************************************************************************/

/* Standard Includes */
#include <sys/types.h>

/* Module Includes */
#include "structs.h"
#include "structs_type_null.h"

/*******************************************************************************
 * NULL TYPE
 ******************************************************************************/

const struct structs_type structs_type_null = {
	0,
	"null",
	STRUCTS_TYPE_PRIMITIVE,
	structs_notsupp_init,
	structs_notsupp_copy,
	structs_notsupp_equal,
	structs_notsupp_ascify,
	structs_notsupp_binify,
	structs_notsupp_encode,
	structs_notsupp_decode,
	structs_nothing_free,
};

/*******************************************************************************
 * END OF FILE
 ******************************************************************************/
