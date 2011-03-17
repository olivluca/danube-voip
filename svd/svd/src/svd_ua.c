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

extern void
ring_timer_cb(su_root_magic_t *magic, su_timer_t *t, su_timer_arg_t *arg);

static void
vf_timer_cb(su_root_magic_t *magic, su_timer_t *t, su_timer_arg_t *arg);

/** @defgroup UAC_I User Agent Client internals.
 *  @ingroup UAC_P
 *  Internal helper functions using with UAC interface functions.
 *  @{*/
/** Create INVITE message.*/
static int
svd_pure_invite( svd_t * const svd, ab_chan_t * const chan,
		char const * const from_str, char const * const to_str );
/** Make AUTHENTICATION.*/
static void
svd_authenticate( svd_t * svd, nua_handle_t * nh, sip_t const *sip,
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
svd_i_invite( int status, char const * phrase, svd_t * const svd,
		nua_handle_t * nh, ab_chan_t * chan,
		sip_t const *sip, tagi_t tags[]);
/** Incoming INVITE has been cancelled.*/
static void
svd_i_cancel (nua_handle_t const * const nh, ab_chan_t * const chan);
/** Call state has changed.*/
static void
svd_i_state (int status, char const *phrase, nua_t * nua,
		svd_t * svd, nua_handle_t * const nh, ab_chan_t * chan,
		sip_t const *sip, tagi_t tags[]);
/** Incoming BYE call hangup.*/
static void
svd_i_bye (nua_handle_t const * const nh, ab_chan_t const * const chan);
/** PRACK.*/
static void
svd_i_prack (nua_handle_t * nh, ab_chan_t const * const chan,
		sip_t const * const sip);
/** Session INFO.*/
static void
svd_i_info(int status, char const * phrase, svd_t * const svd,
		nua_handle_t * nh, ab_chan_t * chan, sip_t const * sip);
/** Make REGISTER SIP action.*/
static void svd_register (svd_t * const svd);


/** Answer to outgoing INVITE.*/
static void
svd_r_invite( int status, char const *phrase, nua_t * nua, svd_t * svd,
		nua_handle_t * nh, ab_chan_t * chan, sip_t const *sip,
		tagi_t tags[]);
/** Answer to nua_get_params() or nua_get_hparams().*/
static void
svd_r_get_params( int status, char const *phrase, nua_t * nua, svd_t * svd,
		nua_handle_t * nh, ab_chan_t * chan, sip_t const *sip,
		tagi_t tags[] );
/** Answer to nua_shutdown().*/
static void
svd_r_shutdown( int status, char const *phrase, nua_t * nua, svd_t * svd,
		nua_handle_t * nh, ab_chan_t * chan, sip_t const *sip,
		tagi_t tags[] );
/** Answer to outgoing REGISTER.*/
static void
svd_r_register (int status, char const *phrase, nua_t * nua, svd_t * svd,
		nua_handle_t * nh, ab_chan_t * chan, sip_t const *sip,
		tagi_t tags[], int const is_register);
/** Answer to outgoing BYE.*/
static void
svd_r_bye(int status, char const *phrase,
	  nua_handle_t const * const nh, ab_chan_t const * const chan);
/** Answer to outgoing INFO.*/
static void
svd_r_info(int status, char const * phrase, svd_t * const svd,
		nua_handle_t * nh, ab_chan_t * chan, sip_t const * sip);
/** @}*/


/** @defgroup UA_MAIN User Agent
 *  User agent - main SIP abstraction for the caller.
 *  @{*/
/** Maximum length of SDP string.*/
#define SDP_STR_MAX_LEN 512
/** Parse SDP string and set appropriate session parameters.*/
static void
svd_parse_sdp(svd_t * const svd, ab_chan_t * const chan, char const * str);
/** Create SDP string depends on codec choice policy.*/
static char *
svd_new_sdp_string (ab_chan_t const * const chan);
/** @}*/

/** Time before elder VF side will try to reinvite it`s pair */
#define VF_REINVITE_SEC 10

/* Is this vf-side is elder than it`s pair */
static int
svd_vf_is_elder(ab_chan_t * const chan);

/** Place call for VF-channel */
static int
svd_place_vf_for(svd_t * const svd, ab_chan_t * const chan);

/** signal handler of SIGCHLD */
void svd_child_handler(int signum, siginfo_t * sinf, void * sctx);
/** context for signal handler of SIGCHLD */
static ab_t * g_ab = NULL;
/** context for vf_timer_cb */
static svd_t * g_svd = NULL;

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
		nua_t * nua, svd_t * svd, nua_handle_t * nh, ab_chan_t * chan,
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
			svd_i_invite (status, phrase, svd, nh, chan, sip, tags);
			break;
		case nua_i_cancel: /*< 2 Incoming INVITE has been cancelled */
			svd_i_cancel (nh, chan);
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
			svd_i_state (status, phrase, nua, svd,nh,chan,sip,tags);
			break;
		case nua_i_outbound:	/*< 8 Status from outbound processing */
			break;
		case nua_i_bye: 	/*< 9 Incoming BYE call hangup */
			svd_i_bye (nh, chan);
			break;
	/* Incoming set first in comment */
		case nua_i_options:	/*< 10 OPTIONS */
		case nua_i_refer:	/*< 11 REFER call transfer */
		case nua_i_publish:	/*< 12 PUBLISH */
			break;
		case nua_i_prack:  	/*< 13 PRACK */
			svd_i_prack (nh, chan, sip);
			break;
		case nua_i_info:	/*< 14 session INFO */
			svd_i_info (status, phrase, svd, nh, chan, sip);
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
			svd_r_get_params (status,phrase,nua,svd,nh,chan,sip,
					tags);
			break;
		case nua_r_shutdown:	/*< 25 Answer to nua_shutdown() */
			svd_r_shutdown (status,phrase,nua,svd,nh,chan,sip,tags);
			break;
		case nua_r_notifier:	/*< 26 Answer to nua_notifier() */
		case nua_r_terminate:	/*< 27 Answer to nua_terminate() */
		case nua_r_authorize:	/*< 28 Answer to nua_authorize()  */
			break;

/************ SIP Responses ************/
		case nua_r_register:/*< 29 Answer to outgoing REGISTER */
			svd_r_register(status, phrase, nua, svd,
					nh, chan, sip, tags, 1);
			break;
		case nua_r_unregister:/*< 30 Answer to outgoing un-REGISTER */
			svd_r_register(status, phrase, nua, svd,
					nh, chan, sip, tags, 0);
			break;
		case nua_r_invite:/*< 31 Answer to outgoing INVITE */
			svd_r_invite(status, phrase, nua, svd,
					nh, chan, sip, tags);
			break;
		case nua_r_cancel:	/*< 32 Answer to outgoing CANCEL */
			break;
		case nua_r_bye:		/*< 33 Answer to outgoing BYE */
			svd_r_bye (status, phrase, nh, chan);
			break;
		case nua_r_options:	/*< 34 Answer to outgoing OPTIONS */
		case nua_r_refer:	/*< 35 Answer to outgoing REFER */
		case nua_r_publish:	/*< 36 Answer to outgoing PUBLISH */
		case nua_r_unpublish:/*< 37 Answer to outgoing un-PUBLISH */
			break;
		case nua_r_info:	 /*< 38 Answer to outgoing INFO */
			svd_r_info (status, phrase, svd, nh, chan, sip);
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

/********************************************************************** UAC */

/**
 * Sends an outgoing INVITE request to localnet router.
 *
 * \param[in] svd 			context pointer.
 * \param[in] use_ff_FXO 	use first free fxo port on destination router.
 * \param[in] chan  		initiator channel.
 *
 * \remark
 * 		Uses global \ref g_conf.
 */
int
svd_invite (svd_t * const svd, int const use_ff_FXO, ab_chan_t * const chan)
{/*{{{*/
	svd_chan_t * chan_ctx = chan->ctx;
	char to_str[100];
	char from_str[100];
	char from_idx[ CHAN_ID_LEN ];
	int err;
DFS
	/* forming from string */
	snprintf (from_idx, CHAN_ID_LEN, "%d", chan->abs_idx);
	strcpy (from_str, "sip:");
	strcat (from_str, from_idx);
	strcat (from_str, "@");
	strcat (from_str, g_conf.self_ip);

	/* forming dest string */
	strcpy (to_str, "sip:");
	if (use_ff_FXO){
		strcat (to_str, FIRST_FREE_FXO);
	} else {
		strcat (to_str, chan_ctx->dial_status.chan_id);
	}
	strcat (to_str, "@");
	strcat (to_str, chan_ctx->dial_status.route_ip);

	/* set call type as local */
	chan_ctx->call_type = calltype_LOCAL;

	/* send invite request */
	err = svd_pure_invite (svd, chan, from_str, to_str );
	if (err){
		goto __exit_fail;
	}
DFE
	return 0;
__exit_fail:
DFE
	return -1;
}/*}}}*/

/**
 * Sends an outgoing INVITE request to internet SIP address
 * 		(set in address book).
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
DFS
	from = sip_from_make(svd->home, g_conf.sip_set.user_URI);

	to = sip_to_make(svd->home, to_str);
	if ( !to ) {
		SU_DEBUG_0 (("%s sip_to_make(): invalid address: %s\n",
				__func__, to_str));
		goto __exit_fail;
	}

	/* Try to make sense out of the URL */
	if (url_sanitize(to->a_url) < 0) {
		SU_DEBUG_0 ((LOG_FNC_A("url_sanitize()")));
		goto __exit_fail;
	}

	assert(chan_ctx->op_handle == NULL);

	nh = nua_handle (svd->nua, chan,
			NUTAG_URL(to->a_url),
			SIPTAG_TO(to),
			SIPTAG_FROM(from),
			TAG_NULL());

	su_free(svd->home, to);
	to = NULL;

	/* reset rtp-socket parameters */
	err = svd_media_vinetic_rtp_sock_rebinddev(chan_ctx);
	if (err){
		SU_DEBUG_1 ((LOG_FNC_A("can`t SO_BINDTODEVICE on RTP-socket")));
		goto __exit_fail;
	}

	chan_ctx->op_handle = nh;
	if( !nh){
		SU_DEBUG_1 ((LOG_FNC_A("can`t create handle")));
		goto __exit_fail;
	}

	chan_ctx->call_type = calltype_REMOTE;

	l_sdp_str = svd_new_sdp_string (chan);
	if ( !l_sdp_str){
		goto __exit_fail;
	}

	nua_invite( nh,
			TAG_IF (svd->outbound_ip[0], SOATAG_ADDRESS(svd->outbound_ip)),
			SOATAG_USER_SDP_STR(l_sdp_str),
			SOATAG_RTP_SORT (SOA_RTP_SORT_LOCAL),
			SOATAG_RTP_SELECT (SOA_RTP_SELECT_SINGLE),
			TAG_END() );
DFE
	free (l_sdp_str);
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
		err = svd_media_vinetic_rtp_sock_rebinddev(chan_ctx);
		if(err){
			SU_DEBUG_1 ((LOG_FNC_A("can`t SO_BINDTODEVICE on RTP-socket")));
			goto __exit;
		}

		/* have remote sdp make local */
		l_sdp_str = svd_new_sdp_string (chan);
		if ( !l_sdp_str){
			goto __exit;
		}

		nua_respond (chan_ctx->op_handle, status, phrase,
				SOATAG_RTP_SORT (SOA_RTP_SORT_LOCAL),
				SOATAG_RTP_SELECT (SOA_RTP_SELECT_SINGLE),
				TAG_IF((chan_ctx->call_type == calltype_REMOTE) &&
						svd->outbound_ip[0],
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
	assert ( chan );
	assert ( chan->ctx );
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
DFS
	if ( nua_handle_has_registrations (svd->op_reg)){
		nua_unregister(svd->op_reg,
				SIPTAG_CONTACT_STR("*"),
				TAG_NULL());
	} else {
		/* unregister all previously registered on server */
		sip_to_t * fr_to = NULL;
		fr_to = sip_to_make(svd->home, g_conf.sip_set.user_URI);
		if( !fr_to){
			SU_DEBUG_2((LOG_FNC_A(LOG_NOMEM)));
			goto __exit;
		}
		svd->op_reg = nua_handle( svd->nua, NULL,
				SIPTAG_TO(fr_to),
				SIPTAG_FROM(fr_to),
				TAG_NULL());
		if (svd->op_reg) {
			nua_unregister(svd->op_reg,
					SIPTAG_CONTACT_STR("*"),
					TAG_NULL());
		}
		su_free (svd->home, fr_to);
	}
__exit:
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
DFS
	nua_shutdown (svd->nua);
DFE
}/*}}}*/

/**
 * Check if given channel is elder than it`s pair.
 *
 * \param[in] chan channel to check.
 *
 * \retval 1 - Given channel is elder than it`s pair.
 * \retval 0 - Given channel is junior for it`s pair.
 */
static int
svd_vf_is_elder(ab_chan_t * const chan)
{/*{{{*/
	struct voice_freq_s * curr_rec = &g_conf.voice_freq[chan->abs_idx];
	int pair_route_id = -1; /* self */
	int pair_chan = strtol (curr_rec->pair_chan, NULL, 10);
	int vf_is_elder;

	if(curr_rec->pair_route){
		/* not self */
		pair_route_id = strtol(curr_rec->pair_route, NULL, 10);
	}

	if(pair_route_id == -1){
		/* self router - compare channels */
		vf_is_elder = pair_chan < chan->abs_idx;
	} else {
		/* not self router - compare routers */
		vf_is_elder = pair_route_id < strtol(g_conf.self_number, NULL, 10);
	}
	return vf_is_elder;
}/*}}}*/

/**
 * It create connections between voice frequency channel.
 *
 * \param[in] svd context pointer
 * \param[in,out] chan channel to operate on it
 * \retval 0 	etherything ok.
 * \retval -1	something nasty happens.
 */
static int
svd_place_vf_for(svd_t * const svd, ab_chan_t * const chan)
{/*{{{*/
	assert(chan != NULL);

	svd_chan_t * ctx = chan->ctx;
	struct voice_freq_s * curr_rec = &g_conf.voice_freq[chan->abs_idx];
	int i;
	int err;

	if(curr_rec->is_set && curr_rec->am_i_caller){
		/* get pair_chan */
		strcpy (ctx->dial_status.chan_id, curr_rec->pair_chan);
		/* get pair_route */
		if ( !curr_rec->pair_route){
			ctx->dial_status.route_ip = g_conf.self_ip;
			ctx->dial_status.dest_is_self = self_YES;
			err = 0;
		} else {
			int routers_num = g_conf.route_table.records_num;
			ctx->dial_status.dest_is_self = self_NO;
			err = 1;
			/* not self connect - find pair route ip from route_t */
			for(i=0; i<routers_num; i++){
				if( !strcmp(curr_rec->pair_route,
							g_conf.route_table.records[i].id)){
					ctx->dial_status.route_ip = g_conf.route_table.records[i].value;
					err = 0;
					break;
				}
			}
			/* set dest router to self if it is */
			if(!strcmp(curr_rec->pair_route, g_conf.self_number)){
				ctx->dial_status.dest_is_self = self_YES;
			}
		}

		/* place a call */
		if(err || svd_invite(svd, 0, chan)){
			SU_DEBUG_0 (("ERROR: Can`t place VF-call\n"));
			goto __exit_fail;
		}
	}
	return 0;
__exit_fail:
	return -1;
}/*}}}*/

/**
 * It create connections between voice frequency channels.
 *
 * \param[in] svd context pointer
 * \retval 0 	etherything ok.
 * \retval -1	something nasty happens.
 * \remark
 * 		There must be offhook on both chennels in the link.
 */
int
svd_place_vf (svd_t * const svd)
{/*{{{*/
	int i;
	int err;
	int chans_num;
DFS
	chans_num = svd->ab->chans_num;
	for(i=0; i<chans_num; i++){
		ab_chan_t * curr_chan = &svd->ab->chans[i];
		err = svd_place_vf_for(svd, curr_chan);
		if(err){
			goto __exit_fail;
		}
	}
DFE
	return 0;
__exit_fail:
DFE
	return -1;
}/*}}}*/

/**
 * Register user to server according to \ref g_conf settings if user
 * 		have no such registration.
 *
 * \param[in] svd context pointer
 */
static void
svd_register(svd_t * svd)
{/*{{{*/
DFS
	if ( !nua_handle_has_registrations (svd->op_reg) ) {
		sip_to_t * fr_to;
		fr_to = sip_to_make(svd->home, g_conf.sip_set.user_URI);
		svd->op_reg = nua_handle( svd->nua, NULL,
				SIPTAG_TO(fr_to),
				SIPTAG_FROM(fr_to),
				TAG_NULL());
		if (svd->op_reg) {
			nua_register(svd->op_reg, TAG_NULL());
		}
	}
DFE
}/*}}}*/

/**
 * Just make INVITE SIP action from address to address.
 *
 * \param[in] svd 		context svd struct.
 * \param[in] chan 		chan to operate on it.
 * \param[in] from_str	make INVITE _from_ SIP URI.
 * \param[in] to_str	make INVITE _to_ SIP URI.
 * \retval 0 	etherything ok.
 * \retval -1	something nasty happens.
 */
static int
svd_pure_invite( svd_t * const svd, ab_chan_t * const chan,
		char const * const from_str, char const * const to_str )
{/*{{{*/
	sip_to_t *to = NULL;
	sip_from_t *from = NULL;
	char * l_sdp_str = NULL;
	nua_handle_t * nh = NULL;
	svd_chan_t * chan_ctx = chan->ctx;
	int err;
DFS
	to = sip_to_make(svd->home, to_str);
	if ( !to ) {
		SU_DEBUG_0 (("%s sip_to_make(): invalid address: %s\n",
				__func__, to_str));
		goto __exit_fail;
	}

	from  = sip_from_make(svd->home, from_str);
	if ( !from ) {
		SU_DEBUG_0 (("%s sip_form_make(): invalid address: %s\n",
				__func__, from_str));
		goto __exit_fail;
	}

	/* Try to make sense out of the URL */
	if (url_sanitize(to->a_url) < 0) {
		SU_DEBUG_0 ((LOG_FNC_A("url_sanitize()")));
		goto __exit_fail;
	}

	if(chan_ctx->op_handle != NULL){
		nua_handle_destroy(chan_ctx->op_handle);
		chan_ctx->op_handle = NULL;
	}

	nh = nua_handle (svd->nua, chan,
			NUTAG_URL(to->a_url),
			SIPTAG_TO(to),
			SIPTAG_FROM(from),
			TAG_NULL());

	su_free(svd->home, to);
	to = NULL;
	su_free(svd->home, from);
	from = NULL;

	/* reset rtp-socket parameters */
	err = svd_media_vinetic_rtp_sock_rebinddev(chan_ctx);
	if (err){
		SU_DEBUG_1 ((LOG_FNC_A("can`t SO_BINDTODEVICE on RTP-socket")));
		goto __exit_fail;
	}

	chan_ctx->op_handle = nh;
	if( !nh){
		SU_DEBUG_1 ((LOG_FNC_A("can`t create handle")));
		goto __exit_fail;
	}

	l_sdp_str = svd_new_sdp_string (chan);
	if ( !l_sdp_str){
		goto __exit_fail;
	}

	SU_DEBUG_3 (("LOCAL SDP STRING : %s\n", l_sdp_str));

	nua_invite( nh,
			SOATAG_USER_SDP_STR(l_sdp_str),
			SOATAG_RTP_SORT (SOA_RTP_SORT_LOCAL),
			SOATAG_RTP_SELECT (SOA_RTP_SELECT_SINGLE),
			TAG_END() );

	if (l_sdp_str){
		free (l_sdp_str);
	}
DFE
	return 0;
__exit_fail:
	if (nh){
		nua_handle_destroy(nh);
	}
	if (to){
		su_free(svd->home, to);
	}
	if (from){
		su_free(svd->home, from);
	}
	if (l_sdp_str){
		free (l_sdp_str);
	}
DFE
	return -1;
}/*}}}*/

static const char *vf_tmr_req_string(enum vf_tmr_request req)
{
    switch (req)
    {
	case vf_tmr_reinvite:
            return "reinvite";
	case vf_tmr_nothing:
            return "nothing";
	default:
            break;
    }
    return "unknown";
}

static int
vf_timer_set(ab_chan_t *chan, enum vf_tmr_request req)
{
    int err;
    svd_chan_t *svd_chan = (svd_chan_t*)chan->ctx;

    if (svd_chan->vf_tmr_request != req)
	SU_DEBUG_4(( "%s(): [%02d], req: %s", __FUNCTION__,
		    chan->abs_idx, vf_tmr_req_string(req) ));

    svd_chan->vf_tmr_request = req;

    // re-set timer anyway
    // FIXME: use different intervals for different vf_tmr_request
    err = su_timer_set_interval(svd_chan->vf_tmr, vf_timer_cb, chan,
				VF_REINVITE_SEC*1000);
    if (err)
	SU_DEBUG_2 (("%s(): [%02d]: Can`t RE-set VF-timer: %d\n", __FUNCTION__,
		     chan->abs_idx, err));
    return err;
}

enum VF_ECHO { VF_ECHO_REQUEST, VF_ECHO_REPLY };

static void
vf_send_echo(ab_chan_t * const chan, enum VF_ECHO echo_type)
{
    char pd[128];
    snprintf(pd, sizeof(pd)-1, "VF[%02d] echo %s\n", chan->abs_idx,
	     (echo_type == VF_ECHO_REQUEST) ? "request" : "reply");

    // send INFO
    svd_chan_t *svd_chan = chan->ctx;
    nua_info(svd_chan->op_handle, SIPTAG_PAYLOAD_STR(pd), TAG_NULL());

    svd_chan->vf_echo_count++;
    SU_DEBUG_4(( "%s(): echo req count: %zu, %s", __FUNCTION__, svd_chan->vf_echo_count, pd ));
}

static void
vf_clean_echo(ab_chan_t * const chan)
{
    svd_chan_t *svd_chan = chan->ctx;
    svd_chan->vf_echo_count = 0;
    SU_DEBUG_4(("%s(): [%02d]", __FUNCTION__, chan->abs_idx));
}

/**
 * Place the call for the VF-channel to it`s pair.
 *
 * \remark
 *	Call parameters should be set outside.
 */
static void
vf_timer_cb(su_root_magic_t *magic, su_timer_t *t, su_timer_arg_t *arg)
{/*{{{*/
    ab_chan_t * chan = arg;
    svd_chan_t *svd_chan = (svd_chan_t*)chan->ctx;
    int err;

    if (svd_chan->vf_tmr_request != vf_tmr_nothing)
	SU_DEBUG_4(( "%s() : [%02d], req: %s", __FUNCTION__,
		    chan->abs_idx, vf_tmr_req_string(svd_chan->vf_tmr_request) ));

    switch (svd_chan->vf_tmr_request)
    {
	case vf_tmr_nothing:
	    vf_timer_set(chan, vf_tmr_nothing);

	    struct voice_freq_s * curr_rec = &g_conf.voice_freq[chan->abs_idx];
	    if (curr_rec->ping)
	    {
		vf_send_echo(chan, VF_ECHO_REQUEST);
		if (svd_chan->vf_echo_count > 3) // FIXME: parameter or constant
		{
		    SU_DEBUG_2(( "%s() : [%02d], counterpart do not respond "
				"to echo requests over 3 times, re-invite",
				__FUNCTION__, chan->abs_idx ));

		    // fall througw to reinvite
		}
		else
		{
		    break;
		}
	    }
	    else
	    {
		break;
	    }
	case vf_tmr_reinvite:
            vf_clean_echo(chan);
	    err = svd_place_vf_for(g_svd, chan);
	    if(err){
		SU_DEBUG_2 (("Can`t RE-invite on VF-pair\n"));
		vf_timer_set(chan, vf_tmr_reinvite);
	    }
	    else
		vf_timer_set(chan, vf_tmr_nothing);
	    break;
	default:
	    SU_DEBUG_2 (("Unknown vf_tmr_request: %d\n", svd_chan->vf_tmr_request));
	    vf_timer_set(chan, vf_tmr_nothing);
    }
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
svd_authenticate (svd_t * svd, nua_handle_t * nh, sip_t const *sip,
		tagi_t * tags)
{/*{{{*/
	sip_www_authenticate_t const *wa = sip->sip_www_authenticate;
	sip_proxy_authenticate_t const *pa = sip->sip_proxy_authenticate;
DFS
	tl_gets (tags,
			SIPTAG_WWW_AUTHENTICATE_REF(wa),
			SIPTAG_PROXY_AUTHENTICATE_REF(pa),
			TAG_NULL());
	if (wa){
		char * reply = NULL;
		sl_header_log(SU_LOG, 3, "Server auth: %s\n",(sip_header_t*)wa);
		reply = su_sprintf(svd->home, "%s:%s:%s:%s",
				wa->au_scheme, msg_params_find(wa->au_params, "realm="),
				g_conf.sip_set.user_name, g_conf.sip_set.user_pass);
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
 * Actions on incoming INVITE request to the FXS/FXO-channel.
 *
 * \param[in] 	svd		svd context structure.
 * \param[in] 	chan	channel context structure.
 * \param[in] 	nh		event handle.
 * \param[in] 	sip		sip headers.
 */
static void
svd_FXx_i_invite( svd_t * const svd, ab_chan_t * chan,
		nua_handle_t * nh, sip_t const *sip)
{/*{{{*/
	svd_chan_t * chan_ctx = chan->ctx;
DFS
	if( chan_ctx->op_handle ){
		/* user is busy */
		nua_respond(nh, SIP_486_BUSY_HERE, TAG_END());
		nua_handle_destroy(nh);
		goto __exit;
	}

	chan_ctx->op_handle = nh;
	nua_handle_bind (nh, chan);

	chan_ctx->call_type = calltype_LOCAL;
	if( !strcmp (g_conf.self_ip, sip->sip_from->a_url->url_host)){
		/* caller - the same router */
		chan_ctx->caller_router_is_self = 1;
	} else {
		chan_ctx->caller_router_is_self = 0;
	}

	if       (chan->parent->type == ab_dev_type_FXS){
		/* start ringing */
		if (0 != ab_FXS_line_ring (chan, ab_chan_ring_RINGING)){
			SU_DEBUG_1(("can`t ring to on [%02d]\n", chan->abs_idx));
		}
	} else if(chan->parent->type == ab_dev_type_FXO){
		/* do offhook */
		if ( 0!= ab_FXO_line_hook (chan, ab_chan_hook_OFFHOOK)){
			SU_DEBUG_1(("can`t offhook on [%02d]\n", chan->abs_idx));
		}
	}
__exit:
DFE
}/*}}}*/

/**
 * Actions on incoming INVITE request to the VF-channel.
 *
 * \param[in] 	svd		svd context structure.
 * \param[in] 	chan	channel context structure.
 * \param[in] 	nh		event handle.
 * \param[in] 	sip		sip headers.
 */
static void
svd_VF_i_invite( svd_t * const svd, ab_chan_t * chan,
		nua_handle_t * nh, sip_t const *sip)
{/*{{{*/
	svd_chan_t * chan_ctx = chan->ctx;
	sip_from_t const *from = sip->sip_from;
	int known_caller;
	int routers_num;
	int pair_chan;
	int pair_route_id;
	int vf_is_valid;
	int abs_chan_idx = chan->abs_idx;
	int proper_pair_router;
	int proper_pair_chan;
	char * rec_pair_route = g_conf.voice_freq[abs_chan_idx].pair_route;
	int i;
DFS
	/* TEST IF IT IS VALID PAIR */
	/* get pair_chan and pair route from invite */
	pair_chan = strtol (from->a_url->url_user, NULL, 10);

	if (g_conf.self_ip == g_conf.lo_ip) {
		pair_route_id = -1; /* self */
		known_caller = 1;
	} else {
		known_caller = 0;
		routers_num = g_conf.route_table.records_num;
		for(i=0; i<routers_num; i++){
			if( !strcmp(from->a_url->url_host, g_conf.route_table.records[i].value)){
				pair_route_id = strtol(g_conf.route_table.records[i].id, NULL, 10);
				if( !strcmp(g_conf.route_table.records[i].id, g_conf.self_number)){
					pair_route_id = -1; /* it is self in net with many routers */
				}
				known_caller = 1;
				break;
			}
		}
	}
	proper_pair_router =
		/* we know that caller */
		known_caller &&
		/* it is self router or */
		(((pair_route_id == -1) && (!rec_pair_route)) ||
		/* it is proper local net router */
		  ((pair_route_id != -1) && rec_pair_route &&
		   (pair_route_id == strtol(rec_pair_route, NULL, 10))));

	proper_pair_chan =
		(strtol(g_conf.voice_freq[abs_chan_idx].pair_chan, NULL, 10) == pair_chan);

	vf_is_valid =
		/* this vf should be set as working */
		g_conf.voice_freq[abs_chan_idx].is_set &&
		/* we have call from proper pair */
		proper_pair_router && proper_pair_chan;

	if( !vf_is_valid){
		nua_respond(nh, SIP_486_BUSY_HERE, TAG_END());
		nua_handle_destroy(nh);
		goto __exit;
	}

	/* Reconnect or just drop new handler */
	if(chan_ctx->call_state == nua_callstate_ready){
		/* reconnect in any case */
		/* drop previous connection */
		ab_chan_media_deactivate (chan);
		svd_clear_call (svd,chan);
	} else {
		/* first call */
		/* kill if first invite from junior */
		if(svd_vf_is_elder(chan)){
			nua_handle_destroy(nh);
			goto __exit;
		}
	}

	chan_ctx->op_handle = nh;
	nua_handle_bind (nh, chan);

	chan_ctx->call_type = calltype_LOCAL;
	if( !strcmp (g_conf.self_ip, from->a_url->url_host)){
		/* caller - the same router */
		chan_ctx->caller_router_is_self = 1;
	} else {
		chan_ctx->caller_router_is_self = 0;
	}
	/* answer */
	svd_answer(svd, chan, SIP_200_OK);
__exit:
DFE
}/*}}}*/

/**
 * Actions on incoming INVITE request from the outside net.
 *
 * \param[in] 	svd		svd context structure.
 * \param[in] 	nh		event handle.
 * \param[in] 	sip		sip headers.
 */
static void
svd_REM_i_invite( svd_t * const svd, nua_handle_t * nh, sip_t const *sip)
{/*{{{*/
	ab_chan_t * chan;
	svd_chan_t * chan_ctx;
	sip_to_t const *to = sip->sip_to;
	char user_URI [USER_URI_LEN];
DFS
	/* remote call */
	if( !g_conf.sip_set.all_set){
		nua_respond(nh, SIP_500_INTERNAL_SERVER_ERROR, TAG_END());
		nua_handle_destroy(nh);
		goto __exit;
	}

	memset(user_URI, 0, USER_URI_LEN);
	strcpy (user_URI, to->a_url->url_scheme);
	strcat (user_URI, ":");
	strcat (user_URI, to->a_url->url_user);
	strcat (user_URI, "@");
	strcat (user_URI, to->a_url->url_host);
	if ( !strcmp (g_conf.sip_set.user_URI, user_URI)){
		/* external call to this router - get sip_chan */
		chan = svd->ab->pchans[g_conf.sip_set.sip_chan];
		if( !chan){
			nua_respond(nh, SIP_500_INTERNAL_SERVER_ERROR, TAG_END());
			nua_handle_destroy(nh);
			goto __exit;
		}
		chan_ctx = chan->ctx;
	} else {
		/* external call to another router in this network */
		nua_handle_destroy(nh);
		goto __exit;
	}

	if( chan_ctx->op_handle ){
		/* user is busy */
		nua_respond(nh, SIP_486_BUSY_HERE, TAG_END());
		nua_handle_destroy(nh);
		goto __exit;
	}

	chan_ctx->op_handle = nh;
	nua_handle_bind (nh, chan);

	chan_ctx->call_type = calltype_REMOTE;

	/* start ringing */
	if (0!= ab_FXS_line_ring (chan, ab_chan_ring_RINGING)){
		SU_DEBUG_1(("can`t ring to on [%02d]\n", chan->abs_idx));
	}
__exit:
DFE
}/*}}}*/

/**
 * Actions on incoming INVITE request.
 *
 * \param[in] 	status	status on event.
 * \param[in] 	phrase	status string on event.
 * \param[in] 	svd		svd context structure.
 * \param[in] 	nh		event handle.
 * \param[in] 	chan	channel context structure.
 * \param[in] 	sip		sip headers.
 * \param[in] 	tags	event tags.
 *
 * \todo
 * 		Think about race conditions in this section.
 * 		At first look it is not so creepy.
 */
static void
svd_i_invite( int status, char const * phrase, svd_t * const svd,
		nua_handle_t * nh, ab_chan_t * chan, sip_t const *sip, tagi_t tags[])
{/*{{{*/
	sip_to_t const *to = sip->sip_to;
DFS
	if( !strcmp (g_conf.self_ip, to->a_url->url_host) ){
		/* local call (local network) */
		/* *
		 * Get requested chan number, it can be:
		 * '$FIRST_FREE_FXO@..' - first free fxo
		 * 'xx@..'              - absolute channel number
		 * */
		if (isdigit(to->a_url->url_user[0])){
			/* 'xx@..'  - absolute channel number */
			unsigned char abs_chan_idx = strtol (to->a_url->url_user, NULL, 10);
			ab_chan_t * req_chan = svd->ab->pchans[abs_chan_idx];
			if( !req_chan){
				nua_respond (nh, SIP_500_INTERNAL_SERVER_ERROR, TAG_END());
				nua_handle_destroy (nh);
				goto __exit;
			} else if((req_chan->parent->type == ab_dev_type_FXS) ||
					  (req_chan->parent->type == ab_dev_type_FXO) ){
				/* FXS or FXO from local net */
				svd_FXx_i_invite (svd, req_chan, nh, sip);
			} else if(req_chan->parent->type == ab_dev_type_VF){
				/* VF from local net */
				svd_VF_i_invite (svd, req_chan, nh, sip);
			}
		} else if ( !strcmp(to->a_url->url_user, FIRST_FREE_FXO )){
			/* '$FIRST_FREE_FXO@..' - first free fxo */
			int chan_idx = get_FF_FXO_idx (svd->ab, -1);
			if( chan_idx == -1 ){
				/* all chans busy */
				nua_respond(nh, SIP_486_BUSY_HERE, TAG_END());
				nua_handle_destroy(nh);
				goto __exit;
			} else {
				/* FXO from local net - nh test again in func */
				svd_FXx_i_invite (svd, &svd->ab->chans[chan_idx], nh, sip);
			}
		} else {
			/* unknown user */
			SU_DEBUG_2(("Incoming call to unknown user \"%s\" on this host\n",
					to->a_url->url_user));
			goto __exit;
		}
	} else {
		/* remote call */
		svd_REM_i_invite (svd, nh, sip);
	}
__exit:
DFE
}/*}}}*/

/**
 * Incoming CANCEL.
 *
 * \param[in] 	nh		event handle.
 * \param[in] 	chan	channel context structure.
 */
static void
svd_i_cancel (nua_handle_t const * const nh, ab_chan_t * const chan)
{/*{{{*/
DFS
	assert (chan);
	assert (((svd_chan_t *)(chan->ctx))->op_handle == nh);
	SU_DEBUG_3 (("CANCEL received\n"));
	/* if FXS - stop ringing and destroy handle */
	if(chan->parent->type == ab_dev_type_FXS){
		int err;
		err = ab_FXS_line_ring (chan, ab_chan_ring_MUTE);
		if(err){
			SU_DEBUG_3 (("Can`t mutes ring on [%02d]: %s\n",
					chan->abs_idx, ab_g_err_str));
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
		nua_handle_t * const nh, ab_chan_t * chan, sip_t const *sip,
		tagi_t tags[])
{/*{{{*/
	char const * l_sdp = NULL;
	char const * r_sdp = NULL;
	int ss_state = nua_callstate_init;
	int err;
DFS
	tl_gets( tags, NUTAG_CALLSTATE_REF (ss_state),
			SOATAG_LOCAL_SDP_STR_REF (l_sdp),
			SOATAG_REMOTE_SDP_STR_REF (r_sdp),
			TAG_END() );

	if( (!chan) && (nh)){
		/* for incoming calls? */
		chan = nua_handle_magic(nh);
	}

	SU_DEBUG_4(("CALLSTATE NAME : %s\n", nua_callstate_name(ss_state)));

	if (r_sdp) {
		SU_DEBUG_4(("Remote sdp:\n%s\n", r_sdp));
		/* parse incoming sdp (offer or answer)
		 * and set remote host/port/first_pt */
		svd_parse_sdp(svd, chan, r_sdp);
	}
	if (l_sdp) {
		SU_DEBUG_4(("Local sdp:\n%s\n", l_sdp));
	}

	((svd_chan_t*)(chan->ctx))->call_state = ss_state;

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
			if( chan->parent->type == ab_dev_type_FXO){
				/* start timer there if fxo */
				svd_chan_t * ctx = chan->ctx;
				err = su_timer_set_interval(ctx->ring_tmr, ring_timer_cb,
						chan, RING_WAIT_DROP*1000);
				if (err){
					SU_DEBUG_2 (("su_timer_set_interval ERROR on [%02d] : %d\n",
								chan->abs_idx, err));
					ctx->ring_state = ring_state_NO_TIMER_INVITE_SENT;
				} else {
					ctx->ring_state = ring_state_TIMER_UP_INVITE_SENT;
					SU_DEBUG_4(("%s():%d RING_STATE [%02d] IS %d\n",
							__func__, __LINE__, chan->abs_idx, ctx->ring_state));
				}
			}
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
			if(chan->parent->type == ab_dev_type_FXO){
				/* answer on call */
				svd_answer (svd, chan, SIP_200_OK);
			}
			break;

	/* 2XX sent */
		case nua_callstate_completed:
			break;
/*}}}*/

	/* 2XX received, ACK sent, or vice versa */
		case nua_callstate_ready:{/*{{{*/
			svd_chan_t * ctx = chan->ctx;
			if( chan->parent->type == ab_dev_type_FXS){
				/* stop playing any tone on the chan */
				if(ab_FXS_line_tone (chan, ab_chan_tone_MUTE)){
					SU_DEBUG_2(("can`t stop playing tone on [%02d]\n",
							chan->abs_idx));
				}
				/* stop playing tone */
				SU_DEBUG_3(("stop playing tone on [%02d]\n", chan->abs_idx));
			} else if(chan->parent->type == ab_dev_type_FXO){
				/* kill ring_thread if it is up */
				if(ctx->ring_state == ring_state_TIMER_UP_INVITE_SENT){
					err = su_timer_reset(ctx->ring_tmr);
					if (err){
						SU_DEBUG_2 (("su_timer_reset ERROR on [%02d] : %d\n",
									chan->abs_idx, err));
					}
					ctx->ring_state = ring_state_NO_TIMER_INVITE_SENT;
					SU_DEBUG_4(("%s():%d RING_STATE [%02d] IS %d\n",
							__func__, __LINE__, chan->abs_idx, ctx->ring_state));
				}
				/* offhook */
				err = ab_FXO_line_hook( chan, ab_chan_hook_OFFHOOK );
				if ( !err){
					SU_DEBUG_3(("do offhook on [%02d]\n", chan->abs_idx));
				} else {
					SU_DEBUG_3(("can`t offhook on [%02d]: %s\n",
							chan->abs_idx, ab_g_err_str));
				}
			}/* vf no need in special preparations */

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
			svd_chan_t * ctx = chan->ctx;

			/* deactivate media */
			ab_chan_media_deactivate (chan);

			if (chan->parent->type == ab_dev_type_FXO){
				/* clear call params */
				svd_clear_call (svd, chan);

				if(ctx->ring_state == ring_state_TIMER_UP_INVITE_SENT){
					SU_DEBUG_4(("%s():%d RING_STATE [%02d] IS %d\n",
							__func__, __LINE__, chan->abs_idx, ctx->ring_state));
					/* kill ring_timer if it is up
					 * if we have no ready state for some reasons
					 */
					err = su_timer_reset(ctx->ring_tmr);
					if (err){
						SU_DEBUG_2 (("su_timer_reset ERROR on [%02d] : %d\n",
									chan->abs_idx, err));
					}
				}
				ctx->ring_state = ring_state_NO_RING_BEFORE;
				SU_DEBUG_4(("%s():%d RING_STATE [%02d] IS %d\n",
						__func__, __LINE__, chan->abs_idx, ctx->ring_state));
 				/* do it any case, even if it onhooked already */
				err = ab_FXO_line_hook (chan, ab_chan_hook_ONHOOK);
				if(err){
					SU_DEBUG_2(("Can`t onhook on [%02d]\n",chan->abs_idx));
				}
				SU_DEBUG_3(("onhook on [%02d]\n",chan->abs_idx));
			} else if(chan->parent->type == ab_dev_type_FXS){
				/* clear call params */
				svd_clear_call (svd, chan);

				/* stop ringing */
				int err;
				err = ab_FXS_line_ring (chan, ab_chan_ring_MUTE);
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
			} else if(chan->parent->type == ab_dev_type_VF){
				/* Re-invite the pair */
				if(ctx->op_handle){
					/* remove handle from the last try */
					nua_handle_destroy (ctx->op_handle);
					ctx->op_handle = NULL;
				}
				if(svd_vf_is_elder(chan)){
					/* reinvite just if it is the elder side */
					g_svd = svd;
					int err = vf_timer_set(chan, vf_tmr_reinvite);
                                        if (!err)
					    SU_DEBUG_3 (("[%02d]: set timer on %d sec before reinvite "
							 "VF-junior\n", chan->abs_idx, VF_REINVITE_SEC));

				}
			}
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
svd_i_bye(nua_handle_t const * const nh, ab_chan_t const * const chan)
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
svd_i_prack (nua_handle_t * nh, ab_chan_t const * const chan,
		sip_t const * const sip)
{/*{{{*/
	sip_rack_t const * rack;
DFS
	rack = sip->sip_rack;
	SU_DEBUG_3 (("received PRACK %u\n", rack ? rack->ra_response : 0));
	if (chan == NULL){
		nua_handle_destroy(nh);
	}
DFE
}/*}}}*/

/**
 * Get digit from channels round buffer and dial it in given mode.
 *
 * \param[in] 	chan		channel to operate on.
 * \param[in] 	pulseMode	should we dial in pulse.
 * \remark
 *	It is also mutes the outcoming voice to do not affect tone
 *	detection on the channel.
 *	It is forks for dial because diling get some time, and if we will
 *	dial in the main process it would not process the in/out voice traffic.
 *	It sets highest priority of RR scheduler because if we need dial in pulse,
 *	time intervals of on/off-hooks should be certain as we want (with normal -
 *	not real time scheduller we got time lags).
 */
void
svd_get_digit(ab_chan_t * const chan, int const pulseMode)
{/*{{{*/
	svd_chan_t * const ctx = chan->ctx;
	pid_t pid;
	struct sigaction act;

	memset (&act, 0, sizeof(act));
	act.sa_flags = SA_SIGINFO | SA_NOCLDSTOP;
	act.sa_sigaction = svd_child_handler;
	sigaction (SIGCHLD, &act, NULL);

	/* holding media while dialing a digit */
	ab_chan_media_volume (chan,-24, g_conf.audio_prms[chan->abs_idx].dec_dB);

	pid = fork();
	if(pid == 0){       /* child */
		/* set highest real-time priority */
		struct sched_param prm;
		int err;
		prm.sched_priority = sched_get_priority_min(SCHED_RR);
		err = sched_setscheduler (0, SCHED_RR, &prm);
		if(err){
			SU_DEBUG_2 (("ERROR Can`t set highest RR sched priority"
						" for p-dialing: %s\n",strerror(errno)));
		}
		ab_FXO_line_digit (chan, 1, &ctx->dial_rbuf[ctx->dial_get_idx],
				0, 0, pulseMode);
		/* dirty hack, again */
		exit(chan->abs_idx);
	} else if(pid < 0){ /* error */
		SU_DEBUG_2 (("ERROR Can`t fork for dial\n"));
	}
}/*}}}*/

/**
 * Add digit to the channels round buffer.
 *
 * \param[in]		svd			context.
 * \param[in,out] 	chan		channel to operate on.
 * \param[in]		digit		digit to put in buffer.
 * \param[in]		pulseMode	should we dial in pulse.
 */
void
svd_put_digit(svd_t * const svd, ab_chan_t * const chan, char digit,
		int const pulseMode)
{/*{{{*/
	svd_chan_t * const ctx = chan->ctx;
	int eq = (ctx->dial_put_idx == ctx->dial_get_idx);

	ctx->dial_rbuf [ctx->dial_put_idx] = digit;
	ctx->dial_put_idx = (ctx->dial_put_idx+1) % DIAL_RBUF_LEN;

	/* dirty hack, but other options even worse :( */
	g_ab = svd->ab;
	if(eq){
		/* should start dialing */
		svd_get_digit(chan, pulseMode);
	}
}/*}}}*/

/**
 * Do some, then child finishing dial a digit.
 *
 * \param[in] 	signum	signal number.
 * \param[in] 	sinf	signal additional info.
 * \param[in] 	sctx	signal context.
 * \remark
 *	It get the channel abs_idx from the child return code.
 *	It is also restore volume level on the channel.
 *	If dial buf is not empty, it initiate next dial.
 */
void
svd_child_handler(int signum, siginfo_t * sinf, void * sctx)
{/*{{{*/
	int status;
	ab_chan_t * const chan = g_ab->pchans[sinf->si_status];
	svd_chan_t * const ctx = g_ab->pchans[sinf->si_status]->ctx;

	/* Bury the child */
	wait(&status);

	ctx->dial_get_idx = (ctx->dial_get_idx+1) % DIAL_RBUF_LEN;

	ab_chan_media_volume (chan, g_conf.audio_prms[sinf->si_status].enc_dB,
			g_conf.audio_prms[sinf->si_status].dec_dB);

	if(ctx->dial_get_idx != ctx->dial_put_idx){
		/* we have some to dial */
		svd_get_digit(chan, 1);
	}
}/*}}}*/

/**
 * Incoming INFO request.
 *
 * \param[in] 	status	status on event.
 * \param[in] 	phrase	status string on event.
 * \param[in] 	svd		svd context structure.
 * \param[in] 	nh		event handle.
 * \param[in] 	chan	channel context structure.
 * \param[in] 	sip		sip headers.
 * \remark
 * 		This is not using now. But later it can be used for DIGIT transmition.
 */
static void
svd_i_info(int status, char const * phrase, svd_t * const svd,
		nua_handle_t * nh, ab_chan_t * chan, sip_t const * sip)
{/*{{{*/
DFS

    if(chan->parent->type == ab_dev_type_FXO)
    {
	if(sip && sip->sip_payload && sip->sip_payload->pl_data)
	{
	    /* should be sended if can`t recognize info payload
	     * (but auto-acc always send 200)
	     nua_respond(nh, SIP_415_UNSUPPORTED_MEDIA, TAG_END());
	     */

	    int tone = -1;
	    char digit = -1;

	    sscanf(sip->sip_payload->pl_data, INFO_STR, &tone, &digit);
	    SU_DEBUG_3 (("%d:'%c'... ",tone, digit));

	    if((!tone) || (g_conf.fxo_PSTN_type[chan->abs_idx] == pstn_type_PULSE_ONLY))
	    {
		/* dialed in pulse - should redial in pulse */
		/* or pulse PSTN only - also should redial in pulse */
		SU_DEBUG_3 (("Dial in pulse:'%c'\n", digit));
		/* put digit in queue on dial */
		svd_put_digit(svd, chan, digit, 1);
	    } else {
		/* PSTN recognizes tones and we dialed in tone
		 * - should not dial anything - pass-through */
		SU_DEBUG_3 (("DON`T Dial anything\n"));
	    }
	}
    }
    else if(chan->parent->type == ab_dev_type_VF)
    {
	struct voice_freq_s * curr_rec = &g_conf.voice_freq[chan->abs_idx];
	if (curr_rec->ping)
	{
	    if(sip && sip->sip_payload && sip->sip_payload->pl_data)
	    {
		ssize_t chan_idx = -1;

		char echo_type[ strlen(sip->sip_payload->pl_data) + 1 ];
		int rc = sscanf(sip->sip_payload->pl_data, "VF\[%zd\] echo %s",
				&chan_idx, &echo_type);

		if (rc == 2)
		{
		    if (strcmp(echo_type, "request") == 0)
		    {
			vf_clean_echo(chan);
			vf_send_echo(chan, VF_ECHO_REPLY);
		    }
		    else if (strcmp(echo_type, "reply") == 0)
		    {
			vf_clean_echo(chan);
		    }
		    else
		    {
			SU_DEBUG_2 (("%s(): [%02d]: Unrecognized echo message: %s\n",
				     __FUNCTION__, chan->abs_idx, echo_type));
		    }
		}
		else
		{
		    SU_DEBUG_2 (("%s(): [%02d]: Unrecognized INFO message: %s\n",
				 __FUNCTION__, chan->abs_idx, sip->sip_payload->pl_data));
		}
	    }
	}
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
		 nua_handle_t * nh, ab_chan_t * chan, sip_t const *sip,
		 tagi_t tags[])
{/*{{{*/
	char buff [256];
	char const * l_sdp_str = NULL;
DFS
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
		nua_handle_t * nh, ab_chan_t * chan, sip_t const *sip,
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
		nua_handle_t * nh, ab_chan_t * chan, sip_t const *sip,
		tagi_t tags[], int const is_register)
{/*{{{*/
DFS
	if(is_register){
		SU_DEBUG_3(("REGISTER: %03d %s\n", status, phrase));
	} else {
		SU_DEBUG_3(("UN-REGISTER: %03d %s\n", status, phrase));
	}

	if (status == 200) {
		sip_contact_t *m = sip ? sip->sip_contact : NULL;
		for (; m; m = m->m_next){
			sl_header_log(SU_LOG, 3, "\tContact: %s\n",
					(sip_header_t*)m);
			strcpy(svd->outbound_ip, m->m_url->url_host);
		}
		if( !is_register){
			sleep(1);
			svd_register (svd);
		}
	} else if (status == 401 || status == 407){
		svd_authenticate (svd, nh, sip, tags);
	} else if (status >= 300) {
		nua_handle_destroy (nh);
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
		nua_handle_t * nh, ab_chan_t * chan, sip_t const *sip,
		tagi_t tags[])
{/*{{{*/
DFS
	SU_DEBUG_3(("got answer on INVITE: %03d %s\n", status, phrase));

	if (status >= 300) {
		if (status == 401 || status == 407) {
			svd_authenticate (svd, nh, sip, tags);
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
		if(chan->parent->type == ab_dev_type_VF &&
				chan->ctx && ((svd_chan_t*)(chan->ctx))->vf_tmr){
			/*connection restored*/
                        vf_timer_set(chan, vf_tmr_nothing);
			SU_DEBUG_3(("Connection on chan [%02d] restored\n", chan->abs_idx));
		}
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
	  nua_handle_t const * const nh, ab_chan_t const * const chan)
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
		nua_handle_t * nh, ab_chan_t * chan, sip_t const * sip)
{/*{{{*/
DFS
	SU_DEBUG_3(("got answer on INFO: %d, %s\n",status,phrase));
DFE
}/*}}}*/

/**
 * Sets channel RTP parameters from SDP string.
 *
 * \param[in] 		svd		svd context structure.
 * \param[in,out] 	chan	channel context structure.
 * \param[in] 		str		SDP string for parsing.
 * \remark
 * 		It sets chan-ctx port, host and payload from SDP string.
 */
static void
svd_parse_sdp(svd_t * const svd, ab_chan_t * const chan, char const *str)
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
		svd_chan_t * chan_ctx = chan->ctx;
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

		SU_DEBUG_5(("Got remote %s:%d with coder/payload [%s/%d], fmtp: %s\n",
				chan_ctx->remote_host,
				chan_ctx->remote_port,
				chan_ctx->sdp_cod_name,
				chan_ctx->sdp_payload,
				sdp_sess->sdp_media->m_rtpmaps->rm_fmtp));
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
svd_new_sdp_string (ab_chan_t const * const chan)
{/*{{{*/
	svd_chan_t * ctx = chan->ctx;
	char * ret_str = NULL;
	codec_t * cp = NULL;
	int limit = SDP_STR_MAX_LEN;
	int ltmp;
	int i;
	long media_port = ctx->rtp_port;
	enum calltype_e const ct = ctx->call_type;

	int is_vf = 0;

	if(chan->parent->type == ab_dev_type_VF){
		is_vf = 1;
	}

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

	if       (ct == calltype_LOCAL){
		cp = &g_conf.int_codecs[0];
	} else if(ct == calltype_REMOTE){
		cp = &g_conf.sip_set.ext_codecs[0];
	} else {
		SU_DEBUG_0((LOG_FNC_A("ERROR: calltype still UNDEFINED")));
		goto __exit_fail_allocated;
	}
	if(is_vf){
		cp = &g_conf.voice_freq[chan->abs_idx].vf_codec;
	}

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
	for(i=0; cp[i].type != cod_type_NONE; i++){
		char pld_str[SDP_STR_MAX_LEN];
		memset(pld_str, 0, sizeof(pld_str));
		ltmp = snprintf(pld_str, SDP_STR_MAX_LEN, " %d", cp[i].user_payload);
		if((ltmp == -1) || (ltmp >= SDP_STR_MAX_LEN)){
			SU_DEBUG_0((LOG_FNC_A(
					"ERROR: Codac PAYLOAD string buffer too small")));
			goto __exit_fail_allocated;
		} else if(ltmp >= limit){
			SU_DEBUG_0((LOG_FNC_A("ERROR: SDP string buffer too small")));
			goto __exit_fail_allocated;
		}
		strncat(ret_str, pld_str, limit);
		limit -= ltmp;
		if(is_vf){
			break;
		}
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
	for(i=0; cp[i].type != cod_type_NONE; i++){
		char rtp_str[SDP_STR_MAX_LEN];
		cod_prms_t const * cod_pr = NULL;

		cod_pr = svd_cod_prms_get(cp[i].type, NULL);
		if( !cod_pr){
			SU_DEBUG_0((LOG_FNC_A("ERROR: Codec type UNKNOWN")));
			goto __exit_fail_allocated;
		}

		memset(rtp_str, 0, sizeof(rtp_str));
		ltmp = snprintf(rtp_str, SDP_STR_MAX_LEN, "a=rtpmap:%d %s/%d\r\n",
				cp[i].user_payload, cod_pr->sdp_name, cod_pr->rate);
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
					cp[i].user_payload, cod_pr->fmtp_str);
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
		if(is_vf){
			break;
		}
	}

	return ret_str;
__exit_fail_allocated:
	free(ret_str);
__exit_fail:
	return NULL;
}/*}}}*/

