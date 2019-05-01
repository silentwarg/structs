#ifndef _STRUCTS_TYPE_TIME_H_
#define _STRUCTS_TYPE_TIME_H_

/*******************************************************************************
 * TIME TYPE
 ******************************************************************************/

/*
 * The data is a variable of type "time_t" which holds a time value
 * containing the number of seconds since 1/1/1970 GMT (as always).
 */

/*
 * GMT time: ASCII representation like "Sat Mar  2 23:29:28 GMT 2001".
 */
extern const struct structs_type structs_type_time_gmt;

/*
 * Local time: ASCII representation like "Sat Mar  2 15:29:28 PDT 2001".
 *
 * Note: using local time does not really work, because local time
 * zone abbrieviations are not uniquely decodable. Therefore local
 * time representations (such as with structs_type_time_local) should
 * be avoided except where it is known that both encoding and decoding
 * time zones are the same.
 */
extern const struct structs_type structs_type_time_local;

/*
 * GMT time represented in ISO-8601 format, e.g., "20010302T23:29:28".
 */
extern const struct structs_type structs_type_time_iso8601;

/*
 * GMT time represented as seconds since the epoch, e.g., "983575768".
 */
extern const struct structs_type structs_type_time_abs;

/*
 * GMT time represented relative to now, e.g., "3600" or "-123".
 *
 * Useful for expiration times, etc.
 */
extern const struct structs_type structs_type_time_rel;

#endif /* _STRUCTS_TYPE_TIME_H_ */
/*******************************************************************************
 * END OF FILE
 ******************************************************************************/
