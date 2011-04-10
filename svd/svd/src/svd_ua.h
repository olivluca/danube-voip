/**
 * @file svd_ua.h
 * User agent interface.
 * It contains two parts: Client and Server. (UAC and UAS)
 * in SIP terminology.
 */

#ifndef __SVD_UA_H__
#define __SVD_UA_H__

#include "sofia.h"

/** Use first free fxo channel on self router - marker */
#define FIRST_FREE_FXO "fxo"

/** @defgroup UAC_P User Agent Client interface.
 *  @ingroup UA_MAIN
 *  User Agent Client actions.
 *  @{*/
/** Make INVITE SIP request with given destination address.*/
int  svd_invite_to (svd_t * const svd, int const chan_idx,
		char const * const to_str);
/** Make answer to SIP call.*/
int  svd_answer (svd_t * const svd, ab_chan_t * const chan,
		int status, char const *phrase);
/** Make BYE SIP action.*/
void svd_bye (svd_t * const svd, ab_chan_t * const chan);
/** Make un-REGISTER and REGISTER again on SIP server.*/
void svd_refresh_registration (svd_t * const svd);
/** Shutdown SIP stack.*/
void svd_shutdown (svd_t * const svd);
/** @}*/


/** @defgroup UAS_P User Agent Server interface.
 *  @ingroup UA_MAIN
 *  User Agent Server actions.
 *  @{*/
/** Callback for react on SIP events.*/
void svd_nua_callback (nua_event_t  event,int status,char const * phrase,
		nua_t * nua, svd_t * svd, nua_handle_t * nh, sip_account_t * account,
		sip_t const * sip, tagi_t tags[]);
/** @}*/

#endif /* __SVD_UA_H__ */

