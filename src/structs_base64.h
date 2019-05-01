#ifndef _STRUCTS_BASE64_H_
#define _STRUCTS_BASE64_H_

/*******************************************************************************
 * BASE 64 ENCODER/DECODER
 ******************************************************************************/

/*
 * Base64 encoder and decoder filters
 */

/*
 * Get a new base 64 encoder, using the supplied character set if
 * charset != NULL, or the default charset if charset == NULL.
 *
 * A charset is an ASCII string with 65 unique characters. The
 * 65th character is used as a pad character when the input length
 * is not a multiple of 3.
 */
extern struct filter *b64_encoder_create(const char *charset);

/*
 * Get a new base 64 decoder, using the supplied character set if
 * charset != NULL, or the default charset if charset == NULL.
 *
 * If 'strict' is zero, then any characters in the input which
 * are not valid charset characters are ignored; otherwise, they
 * cause an error to be returned with errno == EINVAL.
 */
extern struct filter *b64_decoder_create(const char *charset, int strict);

/*
 * The RFC 2045 base 64 character set.
 */
extern const char b64_rfc2045_charset[];

#endif /* _STRUCTS_BASE64_H_ */
/*******************************************************************************
 * END OF FILE
 ******************************************************************************/
