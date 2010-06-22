#ifndef __AB_ERR_H__
#define __AB_ERR_H__

/** Set the error index and string globally and to object if it exists */
#define ab_err_set(err_idx, str)					\
	do {											\
		strncpy(ab_g_err_str,(str),ERR_STR_LENGTH);	\
		ab_g_err_idx = (err_idx);					\
	} while(0)

#endif /* __AB_ERR_H__ */

