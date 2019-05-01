/*******************************************************************************
 * HEADERS
 ******************************************************************************/

/* Standard Includes */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/ethernet.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

/* Module Includes */
#include "structs.h"
#include "structs_type_array.h"
#include "structs_type_ip4.h"

/*******************************************************************************
 * IPv4 ADDRESS TYPE
 ******************************************************************************/

static structs_ascify_t structs_ip4_ascify;
static structs_binify_t structs_ip4_binify;

static char *structs_ip4_ascify(const struct structs_type *type,
				const void *data)
{
	const unsigned char *const bytes
	    = (const unsigned char *)&((const struct in_addr *)data)->s_addr;
	char buf[16];

	/* Don't use inet_ntoa(), it's not thread safe */
	snprintf(buf, sizeof(buf), "%u.%u.%u.%u",
		 bytes[0], bytes[1], bytes[2], bytes[3]);
	return (strdup(buf));
}

static int structs_ip4_binify(const struct structs_type *type,
			      const char *ascii, void *data,
			      char *ebuf, size_t emax)
{
	char buf[32];
	char *s;
	char *t;
	int val;
	int i;

	/* Trim whitespace */
	while (isspace(*ascii))
		ascii++;
	strncpy(buf, ascii, sizeof(buf));
	for (i = strlen(buf); i > 0 && isspace(buf[i - 1]); i--) ;
	buf[i] = '\0';

	/* Parse each byte */
	for (s = buf, i = 0; i < 4; s = t, i++) {
		for (t = s; isdigit(*t); t++) ;
		if (*t != ((i < 3) ? '.' : '\0'))
			goto bogus;
		if (t - s < 1 || t - s > 3)
			goto bogus;
		*t++ = '\0';
		if ((val = atoi(s)) > 255)
			goto bogus;
		((unsigned char *)data)[i] = (unsigned char)val;
	}

	/* OK */
	return (0);

bogus:
	strncpy(ebuf, "invalid IP address", emax);
	errno = EINVAL;
	return (-1);
}

const struct structs_type structs_type_ip4 = {
	sizeof(struct in_addr),
	"ip4",
	STRUCTS_TYPE_PRIMITIVE,
	structs_region_init,
	structs_region_copy,
	structs_region_equal,
	structs_ip4_ascify,
	structs_ip4_binify,
	structs_region_encode,
	structs_region_decode,
	structs_nothing_free,
};

/*******************************************************************************
 * END OF FILE
 ******************************************************************************/
