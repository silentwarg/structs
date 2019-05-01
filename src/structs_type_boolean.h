#ifndef _STRUCTS_TYPE_BOOLEAN_H_
#define _STRUCTS_TYPE_BOOLEAN_H_

/*******************************************************************************
 * HEADERS
 ******************************************************************************/

/* Standard Includes */

/* Module Includes */

/*******************************************************************************
 * BOOLEAN TYPES
 ******************************************************************************/

/*
 * Boolean value stored in a 'char' and output as "False" and "True"
 */
extern const struct structs_type structs_type_boolean_char;

/*
 * Boolean value stored in an 'int' and output as "False" and "True"
 */
extern const struct structs_type structs_type_boolean_int;

/*
 * Boolean value stored in a 'char' and output as "0" and "1"
 */
extern const struct structs_type structs_type_boolean_char_01;

/*
 * Boolean value stored in an 'int' and output as "0" and "1"
 */
extern const struct structs_type structs_type_boolean_int_01;

#endif /* _STRUCTS_TYPE_BOOLEAN_H_ */
/*******************************************************************************
 * END OF FILE
 ******************************************************************************/
