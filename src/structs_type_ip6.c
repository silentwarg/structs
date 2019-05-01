/*******************************************************************************
 * HEADERS
 ******************************************************************************/

/* Standard Includes */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <netdb.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

/* Module Includes */
#include "structs.h"
#include "structs_type_array.h"
#include "structs_type_ip6.h"

/*******************************************************************************
 * IPv6 ADDRESS TYPE
 ******************************************************************************/

static structs_ascify_t structs_ip6_ascify;
static structs_binify_t structs_ip6_binify;

static char *structs_ip6_ascify(const struct structs_type *type,
				const void *data)
{
	char *res;

	if ((res = calloc(1, INET6_ADDRSTRLEN + 1)) != NULL)
		inet_ntop(AF_INET6, data, res, INET6_ADDRSTRLEN);
	return (res);
}

static int
structs_ip6_binify(const struct structs_type *type,
		   const char *ascii, void *data, char *ebuf, size_t emax)
{
	switch (inet_pton(AF_INET6, ascii, data)) {
	case 0:
		strncpy(ebuf, "invalid IPv6 address", emax);
		errno = EINVAL;
		break;
	case 1:
		return (0);
	default:
		break;
	}
	return (-1);
}

const struct structs_type structs_type_ip6 = {
	sizeof(struct in6_addr),
	"ip6",
	STRUCTS_TYPE_PRIMITIVE,
	structs_region_init,
	structs_region_copy,
	structs_region_equal,
	structs_ip6_ascify,
	structs_ip6_binify,
	structs_region_encode,
	structs_region_decode,
	structs_nothing_free,
};

/*******************************************************************************
 * END OF FILE
 ******************************************************************************/
