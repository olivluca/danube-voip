/**
 * @file svd_ua.c
 * User Agent implementation.
 * It contains main User Agent (Client and Server) actions and callbacks.
 */

/*Includes {{{*/
#include "svd.h"
#include "svd_ua.h"
#include "svd_atab.h"
#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
/*}}}*/

/** @defgroup UAC_I User Agent Client internals.
 *  @ingroup UAC_P
 *  Internal helper functions using with UAC interface functions.
 *  @{*/
/** Make AUTHENTICATION.*/
static void
svd_authenticate( svd_t * svd, sip_account_t * account, nua_handle_t * nh, sip_t const *sip,
		tagi_t * tags );
/** @}*/


/** @defgroup UAS_I User Agent Server internals.
 *  @ingroup UAS_P
 *  Internal helper functions using with man callback.
 *  @{*/
/** Error indication.*/
static void
svd_i_error(int const status, char const * const phrase);
/** Incoming call INVITE.*/
static void
svd_i_invite( svd_t * const svd, nua_handle_t * nh, sip_t const *sip);
/** Incoming INVITE has been cancelled.*/
static void
svd_i_cancel (svd_t * const svd, nua_handle_t const * const nh);
/** Call state has changed.*/
static void
svd_i_state (int status, char const *phrase, nua_t * nua,
		svd_t * svd, nua_handle_t * const nh,
		sip_t const *sip, tagi_t tags[]);
/** Incoming BYE call hangup.*/
static void
svd_i_bye (nua_handle_t const * const nh, sip_account_t const * const account);
/** PRACK.*/
static void
svd_i_prack (svd_t * svd, nua_handle_t * nh, sip_account_t const * const account,
		sip_t const * const sip);
/** Make REGISTER SIP action.*/
static void svd_register (svd_t * const svd, sip_account_t * account);


/** Answer to outgoing INVITE.*/
static void
svd_r_invite( int status, char const *phrase, nua_t * nua, svd_t * svd,
		nua_handle_t * nh, sip_t const *sip,
		tagi_t tags[]);
/** Answer to nua_get_params() or nua_get_hparams().*/
static void
svd_r_get_params( int status, char const *phrase, nua_t * nua, svd_t * svd,
		nua_handle_t * nh, sip_account_t * account, sip_t const *sip,
		tagi_t tags[] );
/** Answer to nua_shutdown().*/
static void
svd_r_shutdown( int status, char const *phrase, nua_t * nua, svd_t * svd,
		nua_handle_t * nh, sip_account_t * account, sip_t const *sip,
		tagi_t tags[] );
/** Answer to outgoing REGISTER.*/
static void
svd_r_register (int status, char const *phrase, nua_t * nua, svd_t * svd,
		nua_handle_t * nh, sip_account_t * account, sip_t const *sip,
		tagi_t tags[], int const is_register);
/** Answer to outgoing BYE.*/
static void
svd_r_bye(int status, char const *phrase,
	  nua_handle_t const * const nh, sip_account_t const * const account);
/** Answer to outgoing INFO.*/
static void
svd_r_info(int status, char const * phrase, svd_t * const svd,
		nua_handle_t * nh, sip_account_t * account, sip_t const * sip);
/** @}*/


/** @defgroup UA_MAIN User Agent
 *  User agent - main SIP abstraction for the caller.
 *  @{*/
/** Maximum length of SDP string.*/
#define SDP_STR_MAX_LEN 512
/** Parse SDP string and set appropriate session parameters.*/
static void
svd_parse_sdp(svd_t * const svd, nua_handle_t * const nh, char const * str);
/** Create SDP string depends on codec choice policy.*/
static char *
svd_new_sdp_string (ab_chan_t const * const chan, sip_account_t const * const account);
/** @}*/

/** Sets the telephone even payload */
static void
svd_set_te_codec(sdp_session_t const * sdp_sess, sip_account_t const * const account, svd_chan_t * chan_ctx);

/****************************************************************************/

/**
 * It reacts on SIP events and call appropriate
 * functions from \c UAS.
 *
 * \param[in] 	event	occured event.
 * \param[in] 	status	status on event.
 * \param[in] 	phrase	status string on event.
 * \param[in] 	nua		nua parent object.
 * \param[in] 	svd		svd context structure.
 * \param[in] 	nh		event handle.
 * \param[in] 	chan	channel context structure.
 * \param[in] 	sip		sip headers.
 * \param[in] 	tags	event tags.
 * \remark
 *		Mainly just call in switch-case appropriate handler
 *		for occured event.
 */
void
svd_nua_callback (nua_event_t event, int status, char const * phrase,
		nua_t * nua, svd_t * svd, nua_handle_t * nh, sip_account_t * account,
		sip_t const * sip, tagi_t tags[] )
{/*{{{*/
DFS
	SU_DEBUG_3(("Event : %s\n",nua_event_name(event)));
	if(sip){
		SU_DEBUG_3(("---[ SIP ]---\n"));
		sl_sip_log(NULL, 3, "", sip, 1);
		SU_DEBUG_3(("---[ === ]---\n"));
	}
	switch (event) {

/************ Indications ************/
		case nua_i_error: /*< 0 Error indication */
			svd_i_error (status, phrase);
			break;
		case nua_i_invite: /*< 1 Incoming call INVITE */
			svd_i_invite (svd, nh, sip);
			break;
		case nua_i_cancel: /*< 2 Incoming INVITE has been cancelled */
			svd_i_cancel (svd, nh);
			break;
		case nua_i_ack: /*< 3 Final response to INVITE has been ACKed */
		case nua_i_fork:	/*< 4 Outgoing call has been forked */
			break;
		/* DEPRECATED *****/
		case nua_i_active:	/*< 5 A call has been activated */
		case nua_i_terminated:	/*< 6 A call has been terminated */
			break;
		/* DEPRECATED END */

		case nua_i_state: /*< 7 Call state has changed */
			svd_i_state (status, phrase, nua, svd, nh, sip, tags);
			break;
		case nua_i_outbound:	/*< 8 Status from outbound processing */
			break;
		case nua_i_bye: 	/*< 9 Incoming BYE call hangup */
			svd_i_bye (nh, account);
			break;
	/* Incoming set first in comment */
		case nua_i_options:	/*< 10 OPTIONS */
		case nua_i_refer:	/*< 11 REFER call transfer */
		case nua_i_publish:	/*< 12 PUBLISH */
			break;
		case nua_i_prack:  	/*< 13 PRACK */
			svd_i_prack (svd, nh, account, sip);
			break;
		case nua_i_info:	/*< 14 session INFO */
			break;
		case nua_i_update:	/*< 15 session UPDATE */
		case nua_i_message:	/*< 16 MESSAGE */
		case nua_i_chat:	/*< 17 chat MESSAGE  */
		case nua_i_subscribe:	/*< 18 SUBSCRIBE  */
		case nua_i_subscription:/*< 19 subscription to be authorized */
		case nua_i_notify:	/*< 20 event NOTIFY */
		case nua_i_method:	/*< 21 unknown method */
	/* NO Incoming set first in comment */
		case nua_i_media_error:	/*< 22 Offer-answer error indication */
			break;

/************ Responses ************/
		/*< 23 Answer to nua_set_params() or nua_get_hparams().*/
		case nua_r_set_params:
			break;
		/*< 24 Answer to nua_get_params() or nua_get_hparams().*/
		case nua_r_get_params:
			svd_r_get_params (status,phrase,nua,svd,nh,account,sip,
					tags);
			break;
		case nua_r_shutdown:	/*< 25 Answer to nua_shutdown() */
			svd_r_shutdown (status,phrase,nua,svd,nh,account,sip,tags);
			break;
		case nua_r_notifier:	/*< 26 Answer to nua_notifier() */
		case nua_r_terminate:	/*< 27 Answer to nua_terminate() */
		case nua_r_authorize:	/*< 28 Answer to nua_authorize()  */
			break;

/************ SIP Responses ************/
		case nua_r_register:/*< 29 Answer to outgoing REGISTER */
			svd_r_register(status, phrase, nua, svd,
					nh, account, sip, tags, 1);
			break;
		case nua_r_unregister:/*< 30 Answer to outgoing un-REGISTER */
			svd_r_register(status, phrase, nua, svd,
					nh, account, sip, tags, 0);
			break;
		case nua_r_invite:/*< 31 Answer to outgoing INVITE */
			svd_r_invite(status, phrase, nua, svd,
					nh, sip, tags);
			break;
		case nua_r_cancel:	/*< 32 Answer to outgoing CANCEL */
			break;
		case nua_r_bye:		/*< 33 Answer to outgoing BYE */
			svd_r_bye (status, phrase, nh, account);
			break;
		case nua_r_options:	/*< 34 Answer to outgoing OPTIONS */
		case nua_r_refer:	/*< 35 Answer to outgoing REFER */
		case nua_r_publish:	/*< 36 Answer to outgoing PUBLISH */
		case nua_r_unpublish:/*< 37 Answer to outgoing un-PUBLISH */
			break;
		case nua_r_info:	 /*< 38 Answer to outgoing INFO */
			svd_r_info (status, phrase, svd, nh, account, sip);
			break;
		case nua_r_prack:	/*< 39 Answer to outgoing PRACK */
		case nua_r_update:	 /*< 40 Answer to outgoing UPDATE */
		case nua_r_message:	/*< 41 Answer to outgoing MESSAGE */
		case nua_r_chat: /*< 42 Answer to outgoing chat message */
		case nua_r_subscribe:	/*< 43 Answer to outgoing SUBSCRIBE */
		case nua_r_unsubscribe:/*< 44 Answer to outgoing un-SUBSCRIBE */
		case nua_r_notify:	/*< 45 Answer to outgoing NOTIFY */
		case nua_r_method:/*< 46 Answer to unknown outgoing method */
		case nua_r_authenticate:/*< 47 Answer to nua_authenticate() */
			break;

		default:
			/* unknown event received */
		/* if unknown event and nh unknown - it should be destroyed
		 *(nua_handle_destroy) otherwise(related to an existing call or
		 * registration for instance). - ignore it.
		 */
			SU_DEBUG_2(("UNKNOWN EVENT : %d %s\n", status, phrase));
	}
DFE
}/*}}}*/

/**
 * Sends an outgoing INVITE request to internet SIP address
 *
 * \param[in] svd 		context pointer.
 * \param[in] chan_idx 	initiator channel index.
 * \param[in] to_str 	destination address.
 *
 * \remark
 * 		Uses global \ref g_conf strutcture.
 */
int
svd_invite_to (svd_t * const svd, int const chan_idx, char const * const to_str)
{/*{{{*/
	ab_chan_t * chan = &svd->ab->chans[chan_idx];
	svd_chan_t * chan_ctx = chan->ctx;
	nua_handle_t * nh = NULL;
	sip_to_t *to = NULL;
	sip_from_t *from = NULL;
	char * l_sdp_str = NULL;
	int err;
	int i;
	sip_account_t * account;
	struct dplan_record_s * dplan;
	int dplan_index=-1;
	int account_index=-1;
	int account_priority=0;
	int pri;
	char * to_address = NULL;
DFS
	/* find the account to use to place the call */
	
	/* first, try the dial plan */
	for (i=0; i<su_vector_len(g_conf.dial_plan); i++) {
		dplan = su_vector_item(g_conf.dial_plan, i);
		if (!strncmp(to_str, dplan->prefix, dplan->prefixlen)) {
			account = su_vector_item(g_conf.sip_account,dplan->account);
			if (account->enabled && account->registered) {
				account_index=dplan->account;
				dplan_index=i;
				break;
			}
		}
	}
	
	/* no entry in the dial plan, try accounts based on priority */
	if (dplan_index<0) {
		for (i=0; i<su_vector_len(g_conf.sip_account); i++) {
			account = su_vector_item(g_conf.sip_account,i);
			pri = account->outgoing_priority[chan_idx];
			if (account->enabled && account->registered &&
			  pri > 0 && (pri < account_priority || account_priority == 0)) {
				account_index = i;
				account_priority=pri;
			}
		}
	}
	
	if (account_index<0)
		goto __exit_fail;
	
	account = su_vector_item(g_conf.sip_account, account_index);
	from = sip_from_make(svd->home, account->user_URI);
	from->a_display = account->display;
	if (dplan_index<0) {
		asprintf(&to_address, "sip:%s@%s", to_str, account->sip_domain);
	} else {
		asprintf(&to_address, "sip:%s%s@%s", dplan->replace, to_str+dplan->prefixlen, account->sip_domain);
	}
	SU_DEBUG_9(("Going to call %s\n", to_address));

	to = sip_to_make(svd->home, to_address);
	if ( !to ) {
		SU_DEBUG_0 (("%s sip_to_make(): invalid address: %s\n",
				__func__, to_address));
		goto __exit_fail;
	}

	/* Try to make sense out of the URL */
	if (url_sanitize(to->a_url) < 0) {
		SU_DEBUG_0 ((LOG_FNC_A("url_sanitize()")));
		goto __exit_fail;
	}

	nh = nua_handle (svd->nua, account,
			NUTAG_URL(to->a_url),
			SIPTAG_TO(to),
			SIPTAG_FROM(from),
			TAG_NULL());
			
	su_free(svd->home, to);
	to = NULL;

	/* reset rtp-socket parameters */
	chan_ctx->account = account;
	err = svd_media_tapi_rtp_sock_rebinddev(chan_ctx);
	if (err){
		SU_DEBUG_1 ((LOG_FNC_A("can`t SO_BINDTODEVICE on RTP-socket")));
		goto __exit_fail;
	}

	chan_ctx->op_handle = nh;

	if( !nh){
		SU_DEBUG_1 ((LOG_FNC_A("can`t create handle")));
		goto __exit_fail;
	}

	l_sdp_str = svd_new_sdp_string (chan, account);
	if ( !l_sdp_str){
		goto __exit_fail;
	}
	
	chan_ctx->account = account;
	nua_invite( nh,
			TAG_IF (svd->outbound_ip[0], SOATAG_ADDRESS(svd->outbound_ip)),
			TAG_IF (account->outbound_proxy, NUTAG_PROXY(account->outbound_proxy)),		    
			SOATAG_AUDIO_AUX("telephone-event"),
			SOATAG_USER_SDP_STR(l_sdp_str),
			SOATAG_RTP_SORT (SOA_RTP_SORT_LOCAL),
			SOATAG_RTP_SELECT (SOA_RTP_SELECT_SINGLE),
			TAG_END() );
DFE
	free (l_sdp_str);
	free (to_address);
	return 0;

__exit_fail:
	if (nh){
		nua_handle_destroy(nh);
	}
	if (to){
		su_free(svd->home, to);
	}
	if (l_sdp_str){
		free (l_sdp_str);
	}
	if (to_address){
		free (to_address);
	}
DFE
	return -1;
}/*}}}*/

/**
 * Answers on  call (or just leave).
 *
 * \param[in,out] 	svd		context svd stucture.
 * \param[in,out] 	chan	chan to operate on it.
 * \param[in] 		status	status on event.
 * \param[in] 		phrase	event phrase.
 * \retval 0 we do not have calls to answer (normal case).
 * \retval 1 we answer on some call (also normal case).
 * \sa svd_i_invite().
 */
int
svd_answer (svd_t * const svd, ab_chan_t * const chan, int status,
		char const *phrase)
{/*{{{*/
	int call_answered = 0;
	char * l_sdp_str = NULL;
	svd_chan_t * chan_ctx = chan->ctx;
	int err;
DFS
	if ( chan_ctx->op_handle ){
		/* we have call to answer */

		call_answered = 1;

		/* reset rtp-socket parameters */
		err = svd_media_tapi_rtp_sock_rebinddev(chan_ctx);
		if(err){
			SU_DEBUG_1 ((LOG_FNC_A("can`t SO_BINDTODEVICE on RTP-socket")));
			goto __exit;
		}

		/* have remote sdp make local */
		l_sdp_str = svd_new_sdp_string (chan, chan_ctx->account);
		if ( !l_sdp_str){
			goto __exit;
		}

		nua_respond (chan_ctx->op_handle, status, phrase,
				SOATAG_AUDIO_AUX("telephone-event"),
				SOATAG_RTP_SORT (SOA_RTP_SORT_LOCAL),
				SOATAG_RTP_SELECT (SOA_RTP_SELECT_SINGLE),
				TAG_IF(svd->outbound_ip[0],
						SOATAG_ADDRESS(svd->outbound_ip)),
				SOATAG_USER_SDP_STR (l_sdp_str),
				TAG_END());

		/* set proper payload type to chan */
		nua_get_hparams( chan_ctx->op_handle,
				SOATAG_LOCAL_SDP_STR(NULL), TAG_NULL() );
	}
__exit:
	if (l_sdp_str){
		free (l_sdp_str);
	}
DFE
	return call_answered;
}/*}}}*/

/**
 * Sends a BYE request to an operation handle on the chan.
 *
 * \param[in,out] svd	context svd stucture.
 * \param[in,out] chan	chan to operate on it.
 * \remark
 * 		It also clears the chan call info from previous call.
 */
void
svd_bye (svd_t * const svd, ab_chan_t * const chan)
{/*{{{*/
DFS
	if (!chan) {
		SU_DEBUG_0(("svd_bye called without chan!\n"));
		return;
	}
	if (!chan->ctx) {
		SU_DEBUG_0(("svd_bye called without chan->ctx!\n"));
		return;
	}

	svd_chan_t * chan_ctx = chan->ctx;

	if (chan_ctx->op_handle){
		nua_bye(chan_ctx->op_handle, TAG_END());
	} else {
		/* just clear call params */
		svd_clear_call (svd, chan);
	}
DFE
}/*}}}*/

/**
 * Unregister all previously registered users from server (\ref g_conf).
 *
 * \param[in] svd context pointer
 * \remark
 *		It initiates nua_r_unregister event and in its handler svd_register()
 *		make a new registration. That long way is necessary, because we do
 *		not need multiple old registrations on the server.
 * \sa svd_register().
 */
void
svd_refresh_registration (svd_t * const svd)
{/*{{{*/
	int i;
DFS
	for (i=0; i<su_vector_len(g_conf.sip_account); i++) {
		sip_account_t * account = su_vector_item(g_conf.sip_account, i);
		account->registered = 0;
		account->reg_tmr = su_timer_create(su_root_task(svd->root), 30000);
		if (!account->enabled)
			continue;
		if ( nua_handle_has_registrations (account->op_reg)){
			nua_unregister(account->op_reg,
				SIPTAG_CONTACT_STR("*"),
				TAG_NULL());
		} else {
			/* unregister all previously registered on server */
			sip_to_t * fr_to = NULL;
			fr_to = sip_to_make(svd->home, account->user_URI);
			if( !fr_to){
			      SU_DEBUG_2((LOG_FNC_A(LOG_NOMEM)));
			      continue;
			}
			account->op_reg = nua_handle( svd->nua, NULL,
				SIPTAG_TO(fr_to),
				SIPTAG_FROM(fr_to),
				TAG_NULL());
			if (account->op_reg) {
			        nua_handle_bind(account->op_reg, account);
				nua_unregister(account->op_reg,
					SIPTAG_CONTACT_STR("*"),
					TAG_NULL());
			}
			su_free (svd->home, fr_to);
		}
	}
DFE
	return;
}/*}}}*/

/**
 * It shotdown all the SIP stack.
 * We cann add there necessary actions before quit.
 *
 * \param[in] svd context pointer
 * \remark
 * 		It is not using in normal behavior, but in \ref svd_destroy().
 */
void
svd_shutdown(svd_t * svd)
{/*{{{*/
	int i;
	sip_account_t * account;
DFS
	for (i=0; i<su_vector_len(g_conf.sip_account); i++) {
		account = su_vector_item(g_conf.sip_account, i);
		su_timer_destroy(account->reg_tmr);
	}
	nua_shutdown (svd->nua);
	svd->nua = NULL;
DFE
}/*}}}*/

/**
 * Register user to server according to \ref g_conf settings if user
 * 		have no such registration.
 *
 * \param[in] svd context pointer
 */
static void
svd_register(svd_t * svd, sip_account_t * account)
{/*{{{*/
DFS
	if ( !nua_handle_has_registrations (account->op_reg) ) {
		sip_to_t * fr_to;
		fr_to = sip_to_make(svd->home, account->user_URI);
		fr_to->a_display = account->display;
		account->op_reg = nua_handle( svd->nua, NULL,
			SIPTAG_TO(fr_to),
			SIPTAG_FROM(fr_to),
			TAG_NULL());
		nua_handle_bind(account->op_reg, account);	
		if (account->op_reg) {
			nua_register(account->op_reg, 
			    NUTAG_REGISTRAR(account->registrar),
			    NUTAG_M_USERNAME(account->name),
			    TAG_NULL());
		}
	}
		
DFE
}/*}}}*/

/**
 * Make athentication with parameters from \ref g_conf.
 *
 * \param[in] svd 		context svd struct.
 * \param[in] nh		nua handle to operate on it.
 * \param[in] sip 		SIP headers.
 * \param[in] tags 		event tags.
 */
static void
svd_authenticate (svd_t * svd, sip_account_t * account, nua_handle_t * nh, sip_t const *sip,
		tagi_t * tags)
{/*{{{*/
	sip_www_authenticate_t const *wa = sip->sip_www_authenticate;
	sip_proxy_authenticate_t const *pa = sip->sip_proxy_authenticate;
DFS
	tl_gets (tags,
			SIPTAG_WWW_AUTHENTICATE_REF(wa),
			SIPTAG_PROXY_AUTHENTICATE_REF(pa),
			TAG_NULL());
	if (!wa) wa=pa;		
	if (wa){
		char * reply = NULL;
		sl_header_log(SU_LOG, 3, "Server auth: %s\n",(sip_header_t*)wa);
		reply = su_sprintf(svd->home, "%s:%s:%s:%s",
				wa->au_scheme, msg_params_find(wa->au_params, "realm="),
				account->user_name, account->user_pass);
		if (reply){
			SU_DEBUG_4(("AUTHENTICATING WITH '%s'.\n", reply));
			nua_authenticate (nh, NUTAG_AUTH(reply), TAG_END());
			su_free(svd->home, reply);
		}
	}
DFE
}/*}}}*/

/**
 * Prints verbose error information to stdout.
 *
 * \param[in] status 	error status value.
 * \param[in] phrase 	error message.
 */
static void
svd_i_error(int const status, char const * const phrase)
{/*{{{*/
DFS
	SU_DEBUG_2(("NUA STACK ERROR : %03d %s\n", status, phrase));
DFE
}/*}}}*/

/**
 * Actions on incoming INVITE request.
 *
 * \param[in] 	svd		svd context structure.
 * \param[in] 	nh		event handle.
 * \param[in] 	sip		sip headers.
 */
static void
svd_i_invite( svd_t * const svd, nua_handle_t * nh, sip_t const *sip)
{/*{{{*/
	ab_chan_t * chan;
	svd_chan_t * chan_ctx;
	sip_account_t * sip_account;
	char const *contact = sip->sip_request->rq_url->url_user;
	sip_from_t const * from = sip->sip_from;
	sip_p_asserted_identity_t const * pai = sip_p_asserted_identity( sip );
	sip_remote_party_id_t * rpi = sip_remote_party_id( sip );
	char *cid_display=NULL;
	url_t *cid_from=NULL;
	char *cid=NULL;
	char *cname=NULL;
	char *cname2=NULL;
	int i;
	unsigned char found = 0;
DFS
	/* remote call */
	sip_account = NULL;
	SU_DEBUG_0(("INCOMING CALL TO  %s\n", contact));
	for (i=0; i<su_vector_len(g_conf.sip_account); i++) {
		sip_account_t * temp_account = su_vector_item(g_conf.sip_account, i);
		if( !temp_account->enabled)
		  continue;

		if ( !strcmp(temp_account->name, contact)){
			/* found the account the call is coming from */
			sip_account = temp_account;
			break;
		}
	}
	
	if (!sip_account) {
		nua_respond(nh, SIP_500_INTERNAL_SERVER_ERROR, TAG_END());
		nua_handle_destroy(nh);
		goto __exit;
	}
	
	if (pai) {
	  SU_DEBUG_9(("Call with p-asserted-identity %s  %s:%s@%s\n",
		      pai->paid_display, pai->paid_url->url_scheme, pai->paid_url->url_user,
		      pai->paid_url->url_host));
	  cid_display = (char *)pai->paid_display;
	  cid_from = (url_t *)pai->paid_url;
	} else if (rpi) {  
	  SU_DEBUG_9(("Call with remote party id %s  %s:%s@%s\n",
		      rpi->rpid_display , rpi->rpid_url->url_scheme, rpi->rpid_url->url_user,
		      rpi->rpid_url->url_host));
	  cid_display = (char *)rpi->rpid_display;
	  cid_from = rpi->rpid_url;
	} else {
	  SU_DEBUG_9(("Using from for the caller id\n"));
	  cid_display = (char *)from->a_display;
	  cid_from = (url_t *)from->a_url;
	}
	     
	/* use remote user as caller id, but check if it's numeric */
	if (cid_from) {
		cid = strdup(cid_from->url_user);
		for (i=0; i<strlen(cid); i++) {
			if ((cid[i] < '0' || cid[i] > '9') && (cid[i] != '+' || i>0)) {
				cid[i] = 0;
				break;
			}
		}
	}
	/* Try to use the Display name as caller name, removing " */
	if (cid_display) {
		cname = strdup(cid_display);
		cname2 = cname;
		if (cname2[0] == 34)
			cname2++;
		int cl=strlen(cname2);
		if (cl>0 && cname2[cl-1] == 34)
			cname2[cl-1] = 0;
	}
	
	if (!cname2 || cname2[0] == 0) {
		if (cname)
			free(cname);
		/* no "Display name", use the remote user as caller name */
		asprintf(&cname,"%s@%s", cid_from->url_user, cid_from->url_host);
		cname2 = cname;
	}
	
	
	/* same name as number, only send the number */
	if (cid && cid[0] && cname2 && !strncmp(cname2, cid, strlen(cid)))
		cname2 = NULL;
	
	/* without a number some phones won't ring, provide a dummy number */
	if ((!cid || cid[0] == 0) && cname2 && cname2[0] != 0) {
		if (cid) 
		      free(cid);
		cid = strdup("0");
	}
	
	SU_DEBUG_9(("===========> Using cid %s, caller name %s\n",cid, cname2));
	for (i=0; i<g_conf.channels; i++) {
		chan = &svd->ab->chans[i];
		chan_ctx = chan->ctx;
		if (sip_account->ring_incoming[i] && !(chan_ctx->op_handle) && !chan_ctx->off_hook) {
		  ab_FXS_line_ring(chan, ab_chan_ring_RINGING, cid, cname2);
		  chan_ctx->op_handle = nh;
		  chan_ctx->account = sip_account;
		  found = 1;
		}  
	}
	
	/* no channel available */
	if( !found) {
		/* user is busy */
		nua_respond(nh, SIP_486_BUSY_HERE, TAG_END());
		nua_handle_destroy(nh);
		goto __exit;
	}

	nua_handle_bind (nh, sip_account);

__exit:
	if (cid)
	  free(cid);
	if (cname)
	  free(cname);
DFE
}/*}}}*/

static void
svd_i_cancel (svd_t * const svd, nua_handle_t const * const nh)
{/*{{{*/
	int i;
	ab_chan_t * chan;
	svd_chan_t * chan_ctx;
	
DFS
	SU_DEBUG_3 (("CANCEL received\n"));
	/* Stop ringing all channels tied to this account */
	for (i=0; i<g_conf.channels; i++) {
		chan = &svd->ab->chans[i];
		chan_ctx = chan->ctx;
		if (chan_ctx->op_handle == nh) {
			int err;
			err = ab_FXS_line_ring (chan, ab_chan_ring_MUTE, NULL, NULL);
			if(err){
			      SU_DEBUG_3 (("Can`t mutes ring on [%02d]: %s\n",
					      chan->abs_idx , ab_g_err_str));
			}
			chan_ctx->op_handle = NULL;
			svd_clear_call(svd, chan);
		}  
	}
DFE
}/*}}}*/

/**
 * Callback issued for any change in operation state.
 *
 * \param[in] 	status	status on event.
 * \param[in] 	phrase	status string on event.
 * \param[in] 	nua		nua parent object.
 * \param[in] 	svd		svd context structure.
 * \param[in] 	nh		event handle.
 * \param[in] 	chan	channel context structure.
 * \param[in] 	sip		sip headers.
 * \param[in] 	tags	event tags.
 */
static void
svd_i_state(int status, char const *phrase, nua_t * nua, svd_t * svd,
		nua_handle_t * const nh, sip_t const *sip,
		tagi_t tags[])
{/*{{{*/
	char const * l_sdp = NULL;
	char const * r_sdp = NULL;
	int ss_state = nua_callstate_init;
	int err;
	int i;
	ab_chan_t * chan;
	svd_chan_t * chan_ctx;
DFS
	SU_DEBUG_4(("CALLSTATE NAME : %s\n", nua_callstate_name(ss_state)));

	/* no event handle, ignore */
	if (!nh) {
		SU_DEBUG_4(("CALLSTATE without event handle, ignoring\n"));
		return;
	}
	
	tl_gets( tags, NUTAG_CALLSTATE_REF (ss_state),
			SOATAG_LOCAL_SDP_STR_REF (l_sdp),
			SOATAG_REMOTE_SDP_STR_REF (r_sdp),
			TAG_END() );

	/* find the channel associated to this event handle */		
	for (i=0; i<g_conf.channels; i++) {
		chan = &svd->ab->chans[i];
		chan_ctx = chan->ctx;
		if (chan_ctx->op_handle == nh)
			break;
	}
	
	/* channel not found, ignore */
	if (chan_ctx->op_handle!=nh) {
		SU_DEBUG_4(("CALLSTATE: no channel bound to event handle, ignoring\n"));
		return;
	}
	

	if (r_sdp) {
		SU_DEBUG_4(("Remote sdp:\n%s\n", r_sdp));
		/* parse incoming sdp (offer or answer)
		 * and set remote host/port/first_pt */
		svd_parse_sdp(svd, nh, r_sdp);
	}
	if (l_sdp) {
		SU_DEBUG_4(("Local sdp:\n%s\n", l_sdp));
	}

	switch (ss_state) {
	/* Initial state */
		case nua_callstate_init:
			break;
	/* 401/407 received */
		case nua_callstate_authenticating:
			break;

/*{{{ WE CALL */
	/* INVITE sent */
		case nua_callstate_calling:
			break;

	/* 18X received */
		case nua_callstate_proceeding:
			if( chan->parent->type == ab_dev_type_FXS){
				/* play ringback */
				err = ab_FXS_line_tone (chan, ab_chan_tone_RINGBACK);
				if(err){
					SU_DEBUG_2(("can`t play ringback on [%02d]\n",
							chan->abs_idx));
				}
				/* play ringback */
				SU_DEBUG_3(("play ringback on [%02d]\n",chan->abs_idx));
			}
			break;

	/* 2XX received */
		case nua_callstate_completing:
			nua_ack(nh, TAG_END());
			break;
/*}}}*/

/*{{{ WE ANSWERS */
	/* INVITE received */
		case nua_callstate_received:
			nua_respond(nh, SIP_180_RINGING, TAG_END());
			break;

	/* 18X sent (w/SDP) */
		case nua_callstate_early:
			break;

	/* 2XX sent */
		case nua_callstate_completed:
			break;
/*}}}*/

	/* 2XX received, ACK sent, or vice versa */
		case nua_callstate_ready:{/*{{{*/
			/* stop playing any tone on the chan */
			if(ab_FXS_line_tone (chan, ab_chan_tone_MUTE)){
				SU_DEBUG_2(("can`t stop playing tone on [%02d]\n",
						chan->abs_idx));
			}
			/* stop playing tone */
			SU_DEBUG_3(("stop playing tone on [%02d]\n", chan->abs_idx));

			if(ab_chan_media_activate (chan)){
				SU_DEBUG_1(("media_activate error : %s\n", ab_g_err_str));
			}
			break;
		 }/*}}}*/

	/* BYE sent */
		case nua_callstate_terminating:
			break;

	/* BYE complete */
		case nua_callstate_terminated:{/*{{{*/
			SU_DEBUG_4 (("call on [%02d] terminated\n", chan->abs_idx));

			/* deactivate media */
			ab_chan_media_deactivate (chan);

			/* clear call params */
			svd_clear_call (svd, chan);

			/* stop ringing */
			int err;
			err = ab_FXS_line_ring (chan, ab_chan_ring_MUTE, NULL, NULL);
			if (err){
				SU_DEBUG_2 (("Can`t stop ringing on [%02d]\n",
						chan->abs_idx));
			}
			/* playing busy tone on the chan */
			if(ab_FXS_line_tone (chan, ab_chan_tone_BUSY)){
				SU_DEBUG_2(("can`t playing busy tone on [%02d]\n",
						chan->abs_idx));
			}
			/* playing busy tone */
			SU_DEBUG_3(("playing busy tone on [%02d]\n", chan->abs_idx));
			break;
		}/*}}}*/
	}
DFE
}/*}}}*/

/**
 * Incoming BYE request.\ Note, call state related actions are
 * done in the \ref svd_i_state() callback.
 *
 * \param[in] 	nh		event handle.
 * \param[in] 	chan	channel context structure.
 */
static void
svd_i_bye(nua_handle_t const * const nh, sip_account_t const * const account)
{/*{{{*/
DFS
	SU_DEBUG_3 (("BYE received\n"));
DFE
}/*}}}*/

/**
 * Incoming PRACK request.\ Note, call state related actions are
 * done in the \ref svd_i_state() callback.
 *
 * \param[in] 	nh		event handle.
 * \param[in] 	chan	channel context structure.
 * \param[in] 	sip		sip headers.
 */
static void
svd_i_prack (svd_t * svd, nua_handle_t * nh, sip_account_t const * const account,
		sip_t const * const sip)
{/*{{{*/
	sip_rack_t const * rack;
	ab_chan_t * chan=NULL;
	int i;
DFS
	rack = sip->sip_rack;
	SU_DEBUG_3 (("received PRACK %u\n", rack ? rack->ra_response : 0));
	for (i=0; i<g_conf.channels; i++) {
	  if (((svd_chan_t *)(svd->ab->chans[i].ctx))->op_handle == nh) {
	    chan = &svd->ab->chans[i];
	    break;
	    }
	}    
	if (chan == NULL){
		nua_handle_destroy(nh);
	}
DFE
}/*}}}*/

/**
 * Result callback for nua_r_get_params() request.
 *
 * \param[in] 	status	status on event.
 * \param[in] 	phrase	status string on event.
 * \param[in] 	nua		nua parent object.
 * \param[in] 	svd		svd context structure.
 * \param[in] 	nh		event handle.
 * \param[in] 	chan	channel context structure.
 * \param[in] 	sip		sip headers.
 * \param[in] 	tags	event tags.
 * \remark
 * 		It sets the chan->ctx payload (RTP codec type from sdp string),
 * 		and show all tags.
 */
void
svd_r_get_params(int status, char const *phrase, nua_t * nua, svd_t * svd,
		 nua_handle_t * nh, sip_account_t * account, sip_t const *sip,
		 tagi_t tags[])
{/*{{{*/
	char buff [256];
	char const * l_sdp_str = NULL;
	ab_chan_t * chan;
	svd_chan_t * chan_ctx;
	int i;
DFS
	for (i=0; i<g_conf.channels; i++) {
	  chan = &svd->ab->chans[i];
	  chan_ctx = chan->ctx;
	  if (chan_ctx->op_handle == nh)
	    break;
	}  
	
	/* no channel for this handle, ignore */
	if (chan_ctx->op_handle != nh) {
		SU_DEBUG_4(("svd_r_get_params: no channel with this handle, ignoring\n"));
		return;
	}
	
	tl_gets( tags, SOATAG_LOCAL_SDP_STR_REF(l_sdp_str),
			TAG_NULL() );

	if (l_sdp_str){
		sdp_parser_t * remote_sdp = NULL;
		sdp_session_t * sdp_sess = NULL;
		const char * pa_error = NULL;

		remote_sdp = sdp_parse (svd->home, l_sdp_str, strlen(l_sdp_str),
				sdp_f_insane);

		pa_error = sdp_parsing_error (remote_sdp);
		if (pa_error) {
			SU_DEBUG_1(("%s(): Error parsing SDP: %s\n",
					__func__, pa_error));
		} else {
			sdp_sess = sdp_session (remote_sdp);
			if (sdp_sess && sdp_sess->sdp_media &&
					sdp_sess->sdp_media->m_rtpmaps){
				svd_chan_t * chan_ctx = (svd_chan_t *)(chan->ctx);
				chan_ctx->sdp_payload = sdp_sess->sdp_media->m_rtpmaps->rm_pt;
				memset(chan_ctx->sdp_cod_name,0,sizeof(chan_ctx->sdp_cod_name));
				if(strlen(sdp_sess->sdp_media->m_rtpmaps->rm_encoding) <
						COD_NAME_LEN){
					strncpy(chan_ctx->sdp_cod_name,
							sdp_sess->sdp_media->m_rtpmaps->rm_encoding,
							COD_NAME_LEN);
				} else {
					SU_DEBUG_0((LOG_FNC_A("ERROR: Too long rm_encoding")));
				}
				SU_DEBUG_5(("rm_fmtp: %s\n"
						,sdp_sess->sdp_media->m_rtpmaps->rm_fmtp));
				svd_set_te_codec(sdp_sess, account, chan_ctx);
			}
		}
		sdp_parser_free (remote_sdp);
	}

	while(tags){
		t_snprintf(tags, buff, 256);
		SU_DEBUG_3 (("%s\n",buff));
		tags = tl_next(tags);
	}
DFE
}/*}}}*/

/**
 * Result callback for nua_shutdowns() request.
 *
 * \param[in] 	status	status on event.
 * \param[in] 	phrase	status string on event.
 * \param[in] 	nua		nua parent object.
 * \param[in] 	svd		svd context structure.
 * \param[in] 	nh		event handle.
 * \param[in] 	chan	channel context structure.
 * \param[in] 	sip		sip headers.
 * \param[in] 	tags	event tags.
 */
static void
svd_r_shutdown( int status, char const *phrase, nua_t * nua, svd_t * svd,
		nua_handle_t * nh, sip_account_t * account, sip_t const *sip,
		tagi_t tags[] )
{/*{{{*/
DFS
	/*
	 * 100 - shutdown started
	 * 101 - shutdown in progress
	 * 200 - shutdown successful
	 * 500 - shutdown timeout after 30 seconds
	 */
	if       (status == 100){
		int i;
		int j;
		/* send bye to all on all handlers */
		j = svd->ab->chans_num;
		for (i=0; i<j; i++){
			svd_bye(svd, &svd->ab->chans[i]);
		}
	} else if(status == 101){
		return;
	} else if(status == 200){
		nua_destroy(svd->nua);
		svd->nua = NULL;
	} else if(status == 500){
		return;
	}
	su_root_break(svd->root);
DFE
}/*}}}*/

/**
 * Timer callback, retries registration.
 * \param[in] magic	svd pointer.
 * \param[in] t		initiator timer.
 * \param[in] arg	account pointer.
 */
void
reg_timer_cb (su_root_magic_t *magic, su_timer_t *t, su_timer_arg_t *arg)
{/*{{{*/
	svd_t * svd = magic;
	sip_account_t * account = arg;
	
	SU_DEBUG_3(("Retrying registration to %s, user_URI %s\n", account->registrar, account->user_URI));
	svd_register(svd,account);
}/*}}}*/

/**
 * Callback on nua-(un)register event.
 *
 * \param[in] 	status	status on event.
 * \param[in] 	phrase	status string on event.
 * \param[in] 	nua		nua parent object.
 * \param[in] 	svd		svd context structure.
 * \param[in] 	nh		event handle.
 * \param[in] 	chan	channel context structure.
 * \param[in] 	sip		sip headers.
 * \param[in] 	tags	event tags.
 * \param[in] 	is_register try we register(1) on unregister(0).
 */
static void
svd_r_register(int status, char const *phrase, nua_t * nua, svd_t * svd,
		nua_handle_t * nh, sip_account_t * account, sip_t const *sip,
		tagi_t tags[], int const is_register)
{/*{{{*/
DFS
	if(is_register){
		SU_DEBUG_3(("REGISTER: %03d %s\n", status, phrase));
	} else {
		SU_DEBUG_3(("UN-REGISTER: %03d %s\n", status, phrase));
	}
	/* keep track of last reply (for future web status) */
	if (account->registration_reply)
		free(account->registration_reply);
	asprintf(&account->registration_reply, "%03d %s", status, phrase);
	
	account->registered = 0;
	if (status == 200) {
		sip_contact_t *m = sip ? sip->sip_contact : NULL;
		for (; m; m = m->m_next){
			sl_header_log(SU_LOG, 3, "\tContact: %s\n",
					(sip_header_t*)m);
			strcpy(svd->outbound_ip, m->m_url->url_host);
		}
		account->registered = is_register;
		if( !is_register){
			sleep(1);
			svd_register (svd, account);
		}
	} else if (status == 401 || status == 407){
		svd_authenticate (svd, account, nh, sip, tags);
	} else if (status >= 300) {
		//retry registration after 30 seconds
		nua_handle_destroy (nh);
		account->op_reg = NULL;
		su_timer_set(account->reg_tmr, reg_timer_cb, account);
	}
DFE
}/*}}}*/

/**
 * Callback for an outgoing INVITE request.
 *
 * \param[in] 	status	status on event.
 * \param[in] 	phrase	status string on event.
 * \param[in] 	nua		nua parent object.
 * \param[in] 	svd		svd context structure.
 * \param[in] 	nh		event handle.
 * \param[in] 	chan	channel context structure.
 * \param[in] 	sip		sip headers.
 * \param[in] 	tags	event tags.
 * \remark
 * 		Make authentications if we need it.
 */
static void
svd_r_invite( int status, char const *phrase, nua_t * nua, svd_t * svd,
		nua_handle_t * nh, sip_t const *sip,
		tagi_t tags[])
{/*{{{*/
	ab_chan_t * chan;
	svd_chan_t * chan_ctx;
	sip_account_t * account;
	int i;
DFS
	SU_DEBUG_3(("got answer on INVITE: %03d %s\n", status, phrase));

	for (i=0; i<g_conf.channels; i++) {
		chan = &svd->ab->chans[i];
		chan_ctx = chan->ctx;
		if (chan_ctx->op_handle == nh) 
		  break;
	}
	
	if (chan_ctx->op_handle != nh) {
		SU_DEBUG_0(("svd_r_invite, no channel with this handle, ignoring!\n"));
		return;
	}
	  
	account = chan_ctx->account;
	if (status >= 300) {
		if (status == 401 || status == 407) {
			svd_authenticate (svd, account, nh, sip, tags);
		} else if (status == 486){
			if(chan->parent->type == ab_dev_type_FXS){
				/* busy - play busy tone */
				/* playing busy tone on the chan */
				if(ab_FXS_line_tone (chan, ab_chan_tone_BUSY)){
					SU_DEBUG_2(("can`t playing busy tone on [%02d]\n",
							chan->abs_idx));
				}
				/* playing busy tone */
				SU_DEBUG_3(("playing busy tone on [%02d]\n", chan->abs_idx));
			}
		}
	}
	if(status == 200){
	  //FIXME vf was here
	}
DFE
}/*}}}*/

/**
 * Callback for an outgoing BYE request.
 *
 * \param[in] 	status	status on event.
 * \param[in] 	phrase	status string on event.
 * \param[in] 	nh		event handle.
 * \param[in] 	chan	channel context structure.
 */
static void
svd_r_bye(int status, char const *phrase,
	  nua_handle_t const * const nh, sip_account_t const * const account)
{/*{{{*/
DFS
	SU_DEBUG_3(("got answer on BYE: %03d %s\n", status, phrase));
DFE
}/*}}}*/

/**
 * Callback for an outgoing INFO request.
 *
 * \param[in] 	status	status on event.
 * \param[in] 	phrase	status string on event.
 * \param[in] 	svd		svd context structure.
 * \param[in] 	nh		event handle.
 * \param[in] 	chan	channel context structure.
 * \param[in] 	sip		sip headers.
 * \remark
 * 		Not using yet. But in the future it can be using for
 * 		EVENTS transmission.
 */
static void
svd_r_info(int status, char const * phrase, svd_t * const svd,
		nua_handle_t * nh, sip_account_t * account, sip_t const * sip)
{/*{{{*/
DFS
	SU_DEBUG_3(("got answer on INFO: %d, %s\n",status,phrase));
DFE
}/*}}}*/

/**
 * Sets channel RTP parameters from SDP string.
 *
 * \param[in] 		svd		svd context structure.
 * \param[in] 		nh		nua handle of this call.
 * \param[in] 		str		SDP string for parsing.
 * \remark
 * 		It sets chan-ctx port, host and payload from SDP string.
 */
static void
svd_parse_sdp(svd_t * const svd, nua_handle_t * const nh, const char * str)
{/*{{{*/
	sdp_parser_t * remote_sdp = NULL;
	sdp_session_t * sdp_sess = NULL;
	sdp_connection_t * sdp_connection = NULL;
	const char * pa_error = NULL;
DFS
	remote_sdp = sdp_parse (svd->home, str, strlen(str), sdp_f_insane);

	pa_error = sdp_parsing_error (remote_sdp);
	if (pa_error) {
		SU_DEBUG_1(("%s(): Error parsing SDP: %s\n",
				__func__, pa_error));
		goto __exit;
	}

	sdp_sess = sdp_session (remote_sdp);
	sdp_connection = sdp_media_connections (sdp_sess->sdp_media);

	if (sdp_sess && sdp_sess->sdp_media->m_port &&
			sdp_connection && sdp_connection->c_address) {
		/* if this is an incoming call, potentially more than one channel
		can answer, so we set sdp parameters for all channels involved in
		this call (checking the nua handle) */
		int i;
		for (i=0; i<g_conf.channels; i++) {
			ab_chan_t * chan = &svd->ab->chans[i];
			svd_chan_t * chan_ctx = chan->ctx;
			if (chan_ctx->op_handle == nh) {
				chan_ctx->remote_port = sdp_sess->sdp_media->m_port;
				chan_ctx->remote_host = su_strdup(svd->home,sdp_connection->c_address);
				chan_ctx->sdp_payload = sdp_sess->sdp_media->m_rtpmaps->rm_pt;
				memset(chan_ctx->sdp_cod_name, 0, sizeof(chan_ctx->sdp_cod_name));
				if(strlen(sdp_sess->sdp_media->m_rtpmaps->rm_encoding) <
						sizeof(chan_ctx->sdp_cod_name)){
					strcpy(chan_ctx->sdp_cod_name,
							sdp_sess->sdp_media->m_rtpmaps->rm_encoding);
				} else {
					SU_DEBUG_0(("ERROR: SDP CODNAME string size too small\n"));
					goto __exit;
				}
				svd_set_te_codec(sdp_sess, chan_ctx->account, chan_ctx);
				SU_DEBUG_5(("Set parameters for channel %d, remote %s:%d with coder/payload [%s/%d], fmtp: %s, telephone-event: %d\n",
						i,
						chan_ctx->remote_host,
						chan_ctx->remote_port,
						chan_ctx->sdp_cod_name,
						chan_ctx->sdp_payload,
						sdp_sess->sdp_media->m_rtpmaps->rm_fmtp,
						chan_ctx->te_payload));
			}
		}
	}
__exit:
	sdp_parser_free (remote_sdp);
DFE
	return;
}/*}}}*/


/**
 * Creates SDP string for given channel context.
 *
 * \param[in] 	chan	channel with connection parameters.
 * \remark
 * 		Memory should be freed outside of the function.
 */
static char *
svd_new_sdp_string (ab_chan_t const * const chan, sip_account_t const * const account)
{/*{{{*/
	svd_chan_t * ctx = chan->ctx;
	char * ret_str = NULL;
	int limit = SDP_STR_MAX_LEN;
	int ltmp;
	int i;
	long media_port = ctx->rtp_port;

#if 0
	FOR EXAMPLE
"v=0\r\n"
"m=audio %d RTP/AVP 18 8 0\r\n"
"a=rtpmap:18 G729/8000\r\n"
"a=rtpmap:8 PCMA/8000\r\n"
"a=rtpmap:0 PCMU/8000\r\n"
#endif

	ret_str = malloc (SDP_STR_MAX_LEN);
	if( !ret_str){
		SU_DEBUG_1 ((LOG_FNC_A(LOG_NOMEM_A("sdp_str"))));
		goto __exit_fail;
	}
	memset (ret_str, 0, SDP_STR_MAX_LEN);

	ltmp = snprintf (ret_str, limit,
			"v=0\r\n"
			"m=audio %d RTP/AVP",media_port);
	if(ltmp > -1 && ltmp < limit){
		limit -= ltmp;
	} else {
		SU_DEBUG_0((LOG_FNC_A("ERROR: SDP string buffer too small")));
		goto __exit_fail_allocated;
	}

// "m=audio %d RTP/AVP 18 8 0\r\n"
	for(i=0; account->codecs[i] != cod_type_NONE; i++){
		char pld_str[SDP_STR_MAX_LEN];
		memset(pld_str, 0, sizeof(pld_str));
		ltmp = snprintf(pld_str, SDP_STR_MAX_LEN, " %d", g_conf.codecs[account->codecs[i]].user_payload);
		if((ltmp == -1) || (ltmp >= SDP_STR_MAX_LEN)){
			SU_DEBUG_0((LOG_FNC_A(
					"ERROR: Codec PAYLOAD string buffer too small")));
			goto __exit_fail_allocated;
		} else if(ltmp >= limit){
			SU_DEBUG_0((LOG_FNC_A("ERROR: SDP string buffer too small")));
			goto __exit_fail_allocated;
		}
		strncat(ret_str, pld_str, limit);
		limit -= ltmp;
	}
	if(limit > strlen("\r\n")){
		strcat(ret_str,"\r\n");
		limit -= strlen("\r\n");
	} else {
		SU_DEBUG_0((LOG_FNC_A("ERROR: SDP string buffer too small")));
		goto __exit_fail_allocated;
	}

//"a=rtpmap:18 G729/8000\r\n"
//"a=rtpmap:8 PCMA/8000\r\n"
//"a=rtpmap:0 PCMU/8000\r\n"
//"a=fmtp:100 mode=20\r\n"
	for(i=0; account->codecs[i] != cod_type_NONE; i++){
		char rtp_str[SDP_STR_MAX_LEN];
		cod_prms_t const * cod_pr = NULL;
		codec_t * cp = &g_conf.codecs[account->codecs[i]];

		cod_pr = svd_cod_prms_get(account->codecs[i], NULL);
		if( !cod_pr){
			SU_DEBUG_0((LOG_FNC_A("ERROR: Codec type UNKNOWN")));
			goto __exit_fail_allocated;
		}

		memset(rtp_str, 0, sizeof(rtp_str));
		ltmp = snprintf(rtp_str, SDP_STR_MAX_LEN, "a=rtpmap:%d %s/%d\r\n",
				cp->user_payload, cod_pr->sdp_name, cod_pr->rate);
		if((ltmp == -1) || (ltmp >= SDP_STR_MAX_LEN)){
			SU_DEBUG_0((LOG_FNC_A("ERROR: RTPMAP string buffer too small")));
			goto __exit_fail_allocated;
		} else if(ltmp >= limit){
			SU_DEBUG_0((LOG_FNC_A("ERROR: SDP string buffer too small")));
			goto __exit_fail_allocated;
		}
		strncat(ret_str, rtp_str, limit);
		limit -= ltmp;

		if(cod_pr->fmtp_str[0]){
			memset(rtp_str, 0, sizeof(rtp_str));
			ltmp = snprintf(rtp_str, SDP_STR_MAX_LEN, "a=fmtp:%d %s\r\n",
					cp->user_payload, cod_pr->fmtp_str);
			if((ltmp == -1) || (ltmp >= SDP_STR_MAX_LEN)){
				SU_DEBUG_0((LOG_FNC_A(
						"ERROR: RTPMAP string buffer too small")));
				goto __exit_fail_allocated;
			} else if(ltmp >= limit){
				SU_DEBUG_0((LOG_FNC_A("ERROR: SDP string buffer too small")));
				goto __exit_fail_allocated;
			}
			strncat(ret_str, rtp_str, limit);
			limit -= ltmp;
		}
	}

	return ret_str;
__exit_fail_allocated:
	free(ret_str);
__exit_fail:
	return NULL;
}/*}}}*/

/**
 * Sets payload for telephone-event (dtmf tones according to rfc2883).
 *
 * \param[in] 	sdp_sess	parsed sdp session parameters.
 * \param[in]	account		sip account
 * \param[in]	chan_ctx	channel context to receive the payload.
 */
static void
svd_set_te_codec(sdp_session_t const * sdp_sess, sip_account_t const * const account, svd_chan_t * chan_ctx)
{/*{{{*/
	sdp_rtpmap_t * rtpmap;
DFS
	if (account->dtmf == dtmf_2883) {
		for (rtpmap = sdp_sess->sdp_media->m_rtpmaps; rtpmap !=NULL ; rtpmap = rtpmap->rm_next) {
			SU_DEBUG_9(("Checking media for telephone-event: %s\n", rtpmap->rm_encoding));
			if (!strcmp("telephone-event",rtpmap->rm_encoding)) {
				chan_ctx->te_payload=rtpmap->rm_pt;
				SU_DEBUG_9(("\tfound!\n"));
				break;
			}
		}
	}
DFE
	return;
}/*}}}*/
