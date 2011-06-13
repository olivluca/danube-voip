/**
 * @file svd_atab.c
 * ATA board implementation.
 * It contains ATA board interfaice and internals implementation.
 */

/*{{{ INCLUDES */
#include "svd.h"
#include "svd_cfg.h"
#include "ab_api.h"
#include "svd_ua.h"
#include "svd_atab.h"
#include "svd_led.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <net/if.h>

#include "drv_tapi_io.h"
/*}}}*/

/** @defgroup MEDIA_INT Media helpers (internals).
 *  @ingroup MEDIA
 *  It contains helper functions for media turning and activation.
 *  @{*/
/** Initiate voice and fax codec parameters on given channel.*/
int svd_prepare_chan_codecs( ab_chan_t * const chan, jb_prms_t ** jpb );
/** @}*/

/** @defgroup ATA_EVT ATA board events (internals).
 *  @ingroup ATA_B
 *  It contains callbacks for events on ata board processing.
 *  @{*/
/** Handle events on ATA board.*/
static int svd_atab_handler (su_root_magic_t * root, su_wait_t * w,
		su_wakeup_arg_t * user_data);
/** Process FXS Offhook event.*/
static int svd_handle_event_FXS_OFFHOOK
		( svd_t * const svd, int const chan_idx );
/** Process FXS Onhook event.*/
static int svd_handle_event_FXS_ONHOOK
		( svd_t * const svd, int const chan_idx );
/** Process FXS digit event.*/
static int svd_handle_event_FXS_DIGIT_X
		( svd_t * const svd, int const chan_idx, long const data );
/** Process FXS Faxmodem CED event.*/
static int svd_handle_event_FM_CED
		( svd_t * const svd, int const chan_idx, long const data );
/** @}*/



/** @defgroup DIAL_SEQ Dial Sequence (internals).
 *  @ingroup ATA_B
 *  It contains state machine to process digits in dial sequence.
 *  @{*/
/** Process digit in dialed sequence.*/
static int svd_handle_digit
		( svd_t * const svd, int const chan_idx, long const digit );
/** @}*/



/** @defgroup RTP_H RTP flow handlers and others (internals).
 *  @ingroup ATA_B
 *  @ingroup MEDIA
 *  Handlers called then some data placed in RTP message box.
 *  And RTP socket opened when registers the media \ref svd_media_register().
 *  @{*/
/** Maximum size of RTP packet.*/
#define BUFF_PER_RTP_PACK_SIZE 512
/** Move RTP data from channel to RTP socket.*/
static int svd_media_tapi_handle_local_data (su_root_magic_t * root,
		su_wait_t * w, su_wakeup_arg_t * user_data );
/** Move RTP data from RTP socket to channel.*/
static int svd_media_tapi_handle_remote_data (su_root_magic_t * root,
		su_wait_t * w, su_wakeup_arg_t * user_data );
/** Open RTP socket.*/
static int svd_media_tapi_open_rtp (svd_chan_t * const chan_ctx);
/** @}*/



/** @defgroup ATA_OTHER_I Other internals.
 *  @ingroup ATA_B
 *  Other internal helper functions.
 *  @{*/
/** Init chans context from \ref g_conf.*/
static int svd_chans_init ( svd_t * const svd );
/** Attach callback for ATA devices.*/
static int attach_dev_cb ( svd_t * const svd );
/** @}*/


/**
 * \param[in,out] svd routine context structure.
 * \retval 0	if etherything ok.
 * \retval -1	if something nasty happens.
 * \remark
 *		It uses \ref g_conf to get \c route_id_len.
 * 		It creates ab structure
 *  	It creates svd_channel_status structures
 *  	It registers waits on vinetic dev files
 */
int
svd_atab_create ( svd_t * const svd )
{/*{{{*/
	int err;
#if 0	
	int i;
	int ch_num;
#endif	
DFS
	assert (svd);
	assert (svd->ab);

	/** it uses !!g_conf to get route_id_len */
	err = svd_chans_init (svd);
	if (err){
		SU_DEBUG_0 ((LOG_FNC_A("channels init error")));
		goto __exit_fail;
	}

	err = attach_dev_cb (svd);
	if (err){
		SU_DEBUG_0 ((LOG_FNC_A("dev callback attach error")));
		goto __exit_fail;
	}
DFE
	return 0;
__exit_fail:
DFE
	SU_DEBUG_0 ((">> reason [%d]: %s\n",ab_g_err_idx,ab_g_err_str));
	return -1;
}/*}}}*/

/**
 * It destroy the ab struct and free memory from channels contexes.
 *
 * \param[in,out] svd routine context structure.
 */
void
svd_atab_delete ( svd_t * const svd )
{/*{{{*/
	int i;
	int j;
DFS
	assert (svd);

	if ( !(svd->ab) ){
		goto __exit;
	}

	/* ab_chans_magic_destroy */
	j = svd->ab->chans_num;
	for( i = 0; i < j; i++ ){
		svd_chan_t * curr_chan = svd->ab->chans[ i ].ctx;
		if (curr_chan){
			if (curr_chan->dtmf_tmr){
				su_timer_destroy(curr_chan->dtmf_tmr);
			}
			svd_media_unregister(svd, &svd->ab->chans[ i ]);
			free (curr_chan);
			curr_chan = NULL;
		}
	}
__exit:
DFE
	return;
}/*}}}*/

/**
 * \param[in] svd 	routine context structure.
 * \param[in] chan	channel on which we should attach callbacks.
 * \retval 0	if etherything ok.
 * \retval -1	if something nasty happens.
 */
int
svd_media_register (svd_t * const svd, ab_chan_t * const chan)
{/*{{{*/
	su_wait_t wait[1];
	svd_chan_t * chan_ctx;
	int ret;
DFS
	if( (!chan) || (!chan->ctx)){
		SU_DEBUG_1(("ERROR: [%02d] CAN`T REGISTER MEDIA on unallocated channel\n",
				(chan)? chan->abs_idx: -1));
		goto __exit_fail;
	}

	chan_ctx = chan->ctx;

	ret = su_wait_create(wait, chan->rtp_fd, SU_WAIT_IN);
	if (ret){
		SU_DEBUG_0 ((LOG_FNC_A ("su_wait_create() fails" ) ));
		goto __exit_fail;
	}

	ret = su_root_register (svd->root, wait,
			svd_media_tapi_handle_local_data, chan, 0);
	if (ret == -1){
		SU_DEBUG_0 ((LOG_FNC_A ("su_root_register() fails" ) ));
		goto __exit_fail;
	}
	chan_ctx->local_wait_idx = ret;

	ret = svd_media_tapi_open_rtp (chan_ctx);
	if (ret == -1){
		goto __exit_fail;
	}
	chan_ctx->rtp_sfd = ret;

	ret = su_wait_create(wait, chan_ctx->rtp_sfd, SU_WAIT_IN);
	if (ret){
		SU_DEBUG_0 ((LOG_FNC_A ("su_wait_create() fails" ) ));
		goto __exit_fail;
	}

	ret = su_root_register (svd->root, wait,
			svd_media_tapi_handle_remote_data, chan, 0);
	if (ret == -1) {
		SU_DEBUG_0 ((LOG_FNC_A ("su_root_register() fails" ) ));
		goto __exit_fail;
	}
	chan_ctx->remote_wait_idx = ret;
DFE
	return 0;
__exit_fail:
DFE
	return -1;
}/*}}}*/

/**
 * \param[in] svd 	routine context structure.
 * \param[in] chan	channel on which we should attach callbacks.
 * \retval 0	if etherything ok.
 * \retval -1	if something nasty happens.
 */
void
svd_media_unregister (svd_t * const svd, ab_chan_t * const chan)
{/*{{{*/
	svd_chan_t * chan_ctx;
DFS
	assert (chan);
	assert (chan->ctx);

	chan_ctx = chan->ctx;

	if(chan_ctx->local_wait_idx != -1){
		su_root_deregister (svd->root, chan_ctx->local_wait_idx);
		chan_ctx->local_wait_idx = -1;
	}
	if(chan_ctx->remote_wait_idx != -1){
		su_root_deregister (svd->root, chan_ctx->remote_wait_idx);
		chan_ctx->remote_wait_idx = -1;
	}
	if(chan_ctx->rtp_sfd != -1){
		if( close (chan_ctx->rtp_sfd) ){
			su_perror("svd_media_unregister() close()");
		}
		chan_ctx->rtp_sfd = -1;
	}
DFE
}/*}}}*/

/**
 * \param[in] svd 	routine context structure.
 * \param[in,out] chan	channel on which we should attach callbacks.
 * \remark
 *		It uses \ref g_conf to read \c route_table and \c address_book.
 */
void
svd_clear_call (svd_t * const svd, ab_chan_t * const chan)
{/*{{{*/
	svd_chan_t * chan_ctx = chan->ctx;
	int size;
DFS
	
	/* Collected digits */
	chan_ctx->dial_status.num_digits = 0;
	size = sizeof(chan_ctx->dial_status.digits);
	memset (chan_ctx->dial_status.digits, 0, size);

	/* SDP */
	chan_ctx->sdp_payload = -1;
	chan_ctx->te_payload = -1;
	memset(chan_ctx->sdp_cod_name,0,sizeof(chan_ctx->sdp_cod_name));

	memset(&chan_ctx->vcod, 0, sizeof(chan_ctx->vcod));
	memset(&chan_ctx->fcod, 0, sizeof(chan_ctx->fcod));

	chan_ctx->remote_port = 0;
	if(chan_ctx->remote_host){
		su_free (svd->home, chan_ctx->remote_host);
		chan_ctx->remote_host = NULL;
	}

	/* HANDLE */
	if(chan_ctx->op_handle){
		nua_handle_destroy (chan_ctx->op_handle);
		chan_ctx->op_handle = NULL;
	}
	
	/* SIP ACCOUNT */
	chan_ctx->account = NULL;
DFE
}/*}}}*/

/**
 * \param[in] ab 				ata board structure.
 * \param[in] self_chan_idx 	self channel index in ab->chans[].
 * \return
 * 		channel index in ab->chans[].
 * 		\retval -1				if no free FXS channels available.
 * 		\retval <other_value>	if we found one.
 * \sa get_FF_FXO_idx().
 */
int
get_FF_FXS_idx ( ab_t const * const ab, char const self_chan_idx )
{/*{{{*/
	int i;
	int j;
DFS
	j = ab->chans_num;
	for (i = 0; i < j; i++){
		ab_chan_t * curr_chan = &ab->chans[i];
		svd_chan_t * chan_ctx = curr_chan->ctx;
		if (curr_chan->parent->type == ab_dev_type_FXS &&
				chan_ctx->op_handle == NULL &&
				i != self_chan_idx ){
			break;
		}
	}
	if (i == j){
		SU_DEBUG_2(("No free FXS channel available\n"));
		i = -1;
	}
DFE
	return i;
}/*}}}*/

/**
 * \param[in] chan	channel to operate on it.
 * 		\retval -1	if somthing nasty happens.
 * 		\retval 0 	if etherything ok.
 * \sa ab_chan_media_deactivate().
 * \remark
 * 		It also tuning RTP modes and WLEC according to channel parameters.
 */
int
ab_chan_media_activate ( ab_chan_t * const chan )
{/*{{{*/
	int err;
	jb_prms_t * jbp = NULL;
	svd_chan_t * ctx = chan->ctx;

	err = svd_prepare_chan_codecs (chan, &jbp);
	if(err){
		SU_DEBUG_1((LOG_FNC_A(
				"ERROR: Could not prepare channel codecs params.")));
		goto __exit;
	}

	/* RTP */
	err = ab_chan_media_rtp_tune (chan, &ctx->vcod, &ctx->fcod,
			&g_conf.audio_prms[chan->abs_idx], ctx->te_payload);
	if(err){
		SU_DEBUG_1(("Media_tune error : %s",ab_g_err_str));
		goto __exit;
	}

	/* Jitter Buffer */
	err = ab_chan_media_jb_tune (chan, jbp);
	if(err){
		SU_DEBUG_1(("JB_tune error : %s",ab_g_err_str));
		goto __exit;
	}

	/* WLEC */
	err = ab_chan_media_wlec_tune(chan, &g_conf.wlec_prms[chan->abs_idx]);
	if (err) {
		SU_DEBUG_1(("WLEC activate error : %s",ab_g_err_str));
		goto __exit;
	}

	err = ab_chan_media_switch (chan, 1);
	if(err){
		SU_DEBUG_1(("Media activate error : %s",ab_g_err_str));
		goto __exit;
	}

__exit:
	return err;
}/*}}}*/

/**
 * \param[in] chan	channel to operate on it.
 * 		\retval -1	if somthing nasty happens.
 * 		\retval 0 	if etherything ok.
 * \sa ab_chan_media_activate().
 * \remark
 * 		It also tuning RTP modes and WLEC according to channel parameters.
 */
int
ab_chan_media_deactivate ( ab_chan_t * const chan )
{/*{{{*/
	int err;
	wlec_t wc;

	err = ab_chan_media_switch (chan, 0);
	if(err){
		SU_DEBUG_1(("Media deactivate error : %s",ab_g_err_str));
		goto __exit;
	}

	/* WLEC */
	memset (&wc, 0, sizeof(wc));
	wc.mode = wlec_mode_OFF;
	err = ab_chan_media_wlec_tune(chan, &wc);
	if (err) {
		SU_DEBUG_1(("WLEC deactivate error : %s",ab_g_err_str));
		goto __exit;
	}
__exit:
	return err;
}/*}}}*/

/**
 * It prepares chan voice coder and fax coder parameters.
 *
 * \param[in,out] chan channel to operate on it.
 * \param[out]    jbp  jitter buffer parameters.
 * \retval 0 Success.
 * \retval -1 Fail.
 */
int
svd_prepare_chan_codecs( ab_chan_t * const chan, jb_prms_t ** jpb)
{/*{{{*/
	svd_chan_t * ctx = chan->ctx;
	cod_prms_t const * cp = NULL;
	sip_account_t * account;
	
	/* prepare fcod */
	ctx->fcod.type = g_conf.fax.codec_type;
	account = ctx->account;
	ctx->fcod.sdp_selected_payload = g_conf.fax.external_pt;

	/* prepare vcod */
	/* find codec type by sdp_name */
	cp = svd_cod_prms_get(cod_type_NONE ,ctx->sdp_cod_name);
	if( !cp){
		SU_DEBUG_1((LOG_FNC_A("Can`t get codec params by name")));
		goto __exit_fail;
	}
	ctx->vcod.type = cp->type;
	ctx->vcod.sdp_selected_payload = ctx->sdp_payload;
	ctx->vcod.pkt_size = g_conf.codecs[cp->type].pkt_size;
	*jpb = &g_conf.codecs[cp->type].jb;

	return 0;
__exit_fail:
	return -1;
}/*}}}*/

/**
 * Sets channel id standard.
 *
 * \param[in,out] chan channel to operate on it.
 * \param[in]     cid  name of standard.
 * \retval 0 Success.
 * \retval -1 Fail.
 * \ remark 
 *   Valid values: "TELCORDIA", "ETSI_FSK", "ETSI_DTMF", "SIN", "NTT", "KPN_DTMF", "KPN_DTMF_FSK"
 */
int svd_set_cid( ab_chan_t * const chan, const char *cid)
{
	cid_std_t standard;
	
	if (!strcasecmp(cid, "OFF")) {
		standard = cid_OFF;
	}
	else if (!strcasecmp(cid, "TELCORDIA")) {
		standard = cid_TELCORDIA;
	}
	else if (!strcasecmp(cid, "ETSI_FSK")) {
		standard = cid_ETSI_FSK;
	}
	else if (!strcasecmp(cid, "ETSI_DTMF")) {
		standard = cid_ETSI_DTMF;
	}
	else if (!strcasecmp(cid, "SIN")) {
		standard = cid_SIN;
	}
	else if (!strcasecmp(cid, "NTT")) {
		standard = cid_NTT;
	}
	else if (!strcasecmp(cid, "KPN_DTMF")) {
		standard = cid_KPN_DTMF;
	}
	else if (!strcasecmp(cid, "KPN_DTMF_FSK")) {
		standard = cid_KPN_DTMF_FSK;
	} else {
	        SU_DEBUG_9(("Caller id %s not mapped\n",cid));
		return -1;
	}
	SU_DEBUG_9(("Channel %d, mapped caller id standard: %s to: %d\n",chan->abs_idx, cid, standard));
	int err=ab_chan_cid_standard(chan, standard);
	if (err)
	  SU_DEBUG_9(("%s\n",ab_g_err_str));
	return err;
}

/**
 * \param[in] ct codec type.
 * \param[in] cn codec sdp name.
 * \return cod_prms_t pointer.
 * \retval NULL if fail.
 * \retval valid_address if success.
 * \remark
 *	if  \code  ct == cod_type_NONE \endcode  or
 *	\code  cp == NULL \endcode, then you get parameters by
 *	other valid given value. If both are unset, then you get the next
 *	codec`s parameters from the all, until you got the empty set. After
 *	that all rounds again.
 */
cod_prms_t const *
svd_cod_prms_get(enum cod_type_e ct ,char const * const cn)
{/*{{{*/
	static int get_next = 0;
	int i;
	int ret_idx = -1;
	int pass_type = 0;
	int pass_name = 0;

	if(ct == cod_type_NONE){
		pass_type = 1;
	}
	if( !cn){
		pass_name = 1;
	}

	/* It shouldn't actually happen, this function is always called with one parameter set */
	if(pass_type && pass_name){
		ret_idx = get_next;
		if(g_conf.cp[get_next].type == cod_type_NONE || g_conf.cp[get_next].type == TELEPHONE_EVENT_CODEC){
			get_next = 0;
		} else {
			get_next++;
		}
		goto __exit_success;
	}

	for (i=0; g_conf.cp[i].type != cod_type_NONE; i++){
		if(		(pass_type || (g_conf.cp[i].type == ct)) &&
				(pass_name || (!strcmp(g_conf.cp[i].sdp_name, cn)))){
			ret_idx = i;
			break;
		}
	}

	if(ret_idx == -1){
		goto __exit_fail;
	}


__exit_success:
	return &g_conf.cp[ret_idx];
__exit_fail:
	return NULL;
}/*}}}*/

/**
 * \param[in] svd 			svd context structure.
 * \param[in] w 			wait object, that emits.
 * \param[in] user_data 	device on witch event occures.
 * 		\retval -1	if somthing nasty happens.
 * 		\retval 0 	if etherything ok.
 * \todo
 * 		In ideal world it should be reenterable or mutexes should be used.
 */
static int
svd_atab_handler (svd_t * svd, su_wait_t * w, su_wakeup_arg_t * user_data)
{/*{{{*/
DFS
	ab_dev_t * ab_dev = (ab_dev_t *) user_data;
	ab_dev_event_t evt;
	svd_chan_t * chan_ctx;
	unsigned char chan_av;
	int chan_idx;
	int dev_idx;
	int err;

do{
	memset(&evt, 0, sizeof(evt));

	err = ab_dev_event_get( ab_dev, &evt, &chan_av );
	if( err ){
		SU_DEBUG_1 ((LOG_FNC_A (ab_g_err_str) ));
		goto __exit_fail;
	}

	dev_idx = ab_dev->idx - 1;
	if (chan_av){
		/* in evt.ch we have proper number of the chan */
		chan_idx = dev_idx * svd->ab->chans_per_dev + evt.ch;
	} else {
		/* in evt.ch we do not have proper number of the chan because
		 * the event is the device event - not the chan event
		 */
		chan_idx = dev_idx * svd->ab->chans_per_dev;
	}
	chan_ctx = svd->ab->chans[chan_idx].ctx;

	if        (evt.id == ab_dev_event_FXS_OFFHOOK){
		SU_DEBUG_8 (("Got fxs offhook event: 0x%X on [%d/%d]\n",
				evt.data, dev_idx,evt.ch ));
		err = svd_handle_event_FXS_OFFHOOK(svd, chan_idx);
	} else if(evt.id == ab_dev_event_FXS_ONHOOK){
		SU_DEBUG_8 (("Got fxs onhook event: 0x%X on [%d/%d]\n",
				evt.data, dev_idx,evt.ch ));
		err = svd_handle_event_FXS_ONHOOK(svd, chan_idx);
	} else if(evt.id == ab_dev_event_FXS_DIGIT_TONE ||
			  evt.id == ab_dev_event_FXS_DIGIT_PULSE){
		err = svd_handle_event_FXS_DIGIT_X(svd, chan_idx, evt.data);
	} else if(evt.id == ab_dev_event_FXO_RINGING){
		/* shouldn't happen */
		SU_DEBUG_8 (("Got fxo ringing event: 0x%X on [%d/%d]\n",
				evt.data, dev_idx,evt.ch ));
		err = 0;
	} else if(evt.id == ab_dev_event_FM_CED){
		SU_DEBUG_8 (("Got CED event: 0x%X on [%d/%d]\n",
				evt.data, dev_idx,evt.ch ));
		err = svd_handle_event_FM_CED (svd, chan_idx, evt.data);
	} else if(evt.id == ab_dev_event_COD){
		SU_DEBUG_8 (("Got coder event: 0x%X on [%d/%d]\n",
				evt.data,dev_idx,evt.ch ));
	} else if(evt.id == ab_dev_event_TONE){
		SU_DEBUG_8 (("Got tone event: 0x%X on [%d/%d]\n",
				evt.data,dev_idx,evt.ch ));
	} else if(evt.id == ab_dev_event_UNCATCHED){
		SU_DEBUG_8 (("Got unknown event : 0x%X on [%d/%d]\n",
				evt.data, dev_idx,evt.ch ));
	}
	if(evt.more){
		SU_DEBUG_8 (("Got more then one event in one time: on [%d/%d]\n",
				evt.data, dev_idx,evt.ch ));
	}
	if (err){
		goto __exit_fail;
	}
} while(evt.more);

DFE
	return 0;
__exit_fail:
DFE
	return -1;
}/*}}}*/

/**
 * \param[in] svd 		svd context structure.
 * \param[in] chan_idx 	channel on which event occures.
 * 		\retval -1	if somthing nasty happens.
 * 		\retval 0 	if etherything ok.
 */
static int
svd_handle_event_FXS_OFFHOOK( svd_t * const svd, int const chan_idx )
{/*{{{*/
	ab_chan_t * ab_chan = &svd->ab->chans[chan_idx];
	svd_chan_t * chan_ctx = ab_chan->ctx;
	int call_answered;
	int err;
	int i;
DFS
	chan_ctx->off_hook = 1;
	/* stop ringing all lines that were ringing for this call*/
	if (chan_ctx->op_handle) {
		for (i=0; i<g_conf.channels; i++) {
			ab_chan_t * chan = &svd->ab->chans[i];
			svd_chan_t * ctx = chan->ctx;
			if (chan_ctx->op_handle == ctx->op_handle)
			    ab_FXS_line_ring(chan, ab_chan_ring_MUTE, NULL, NULL);
			/* remove association with this call if it's not this line answering */
			if (chan_idx != ctx->chan_idx) {
			    ctx->op_handle = NULL;
			    if (g_conf.chan_led[ctx->chan_idx])
				led_off(g_conf.chan_led[ctx->chan_idx]);
			    svd_clear_call(svd, chan);
			}
		    
		}
	}
	
	/* change linefeed mode to ACTIVE */
	err = ab_FXS_line_feed (ab_chan, ab_chan_linefeed_ACTIVE);
	if (err){
		SU_DEBUG_0(("can`t set linefeed to active on [%02d]\n",
				ab_chan->abs_idx));
		svd_answer(svd, ab_chan, SIP_500_INTERNAL_SERVER_ERROR);
		goto __exit_fail;
	}

	/* answer on call if it exists */
	call_answered = svd_answer(svd, ab_chan, SIP_200_OK);
	if (call_answered){
		if (g_conf.chan_led[chan_ctx->chan_idx])
			led_blink(g_conf.chan_led[chan_ctx->chan_idx], LED_SLOW_BLINK);
		goto __exit_success;
	}

	/* no call to answer - play dialtone*/
	if (g_conf.chan_led[chan_ctx->chan_idx])
		led_on(g_conf.chan_led[chan_ctx->chan_idx]);
	err = ab_FXS_line_tone (ab_chan, ab_chan_tone_DIAL);
	if(err){
		SU_DEBUG_2(("can`t play dialtone on [%02d]\n",ab_chan->abs_idx));
	}
	SU_DEBUG_8(("play dialtone on [%02d]\n",ab_chan->abs_idx));
__exit_success:
DFE
	return 0;
__exit_fail:
DFE
	return -1;
}/*}}}*/

/**
 * \param[in] svd 		svd context structure.
 * \param[in] chan_idx 	channel on which event occures.
 * 		\retval -1	if somthing nasty happens.
 * 		\retval 0 	if etherything ok.
 */
static int
svd_handle_event_FXS_ONHOOK( svd_t * const svd, int const chan_idx )
{/*{{{*/
	ab_chan_t * ab_chan = &svd->ab->chans[chan_idx];
	svd_chan_t * chan_ctx = ab_chan->ctx;
	int err;

DFS
	chan_ctx->off_hook = 0;
	
	/* say BYE on existing connection */
	svd_bye(svd, ab_chan);

	/* stop playing any tone on the chan */
	err = ab_FXS_line_tone (ab_chan, ab_chan_tone_MUTE);
	if(err){
		SU_DEBUG_2(("can`t stop playing tone on [%02d]\n",
				ab_chan->abs_idx));
	}
	/* stop playing tone */
	SU_DEBUG_8(("stop playing tone on [%02d]\n",
				ab_chan->abs_idx));

	/* turn off led */
	if (g_conf.chan_led[chan_ctx->chan_idx])
		led_off(g_conf.chan_led[chan_ctx->chan_idx]);
	
	/* change linefeed mode to STANDBY */
	err = ab_FXS_line_feed (ab_chan, ab_chan_linefeed_STANDBY);
	if (err){
		SU_DEBUG_0(("can`t set linefeed to standby on [%02d]\n",
				ab_chan->abs_idx));
		goto __exit_fail;
	}

DFE
	return 0;
__exit_fail:
DFE
	return -1;
}/*}}}*/

/**
 * \param[in] svd 		svd context structure.
 * \param[in] chan_idx 	channel on which event occures.
 * \param[in] data 		digit of the event.
 * 		\retval -1	if somthing nasty happens.
 * 		\retval 0 	if etherything ok.
 */
static int
svd_handle_event_FXS_DIGIT_X ( svd_t * const svd, int const chan_idx,
		long const data )
{/*{{{*/
	ab_chan_t * ab_chan = &svd->ab->chans[chan_idx];
	svd_chan_t * chan_ctx = ab_chan->ctx;
	char digit = data;
	int err;

DFS
	SU_DEBUG_8 (("[%02d] DIGIT \'%c\'(l:%d,n:%d)HN:%p\n",
			ab_chan->abs_idx, digit, (data >> 9),(data >> 8) & 1,
			chan_ctx->op_handle));

	if( chan_ctx->op_handle ){
		/* already connected, send dtmf info (rfc2976) if account configured for that */
		if (chan_ctx->account->dtmf == dtmf_info) {
			int tone = (data >> 8);
			char pd[INFO_STR_LENGTH];

			memset (pd, 0, sizeof(pd[0]));
			snprintf(pd, INFO_STR_LENGTH, INFO_STR, tone, digit);

			/* send INFO */
			nua_info(chan_ctx->op_handle,
				  SIPTAG_PAYLOAD_STR(pd),
				  TAG_NULL());
		}
	} else {
		/* not connected yet - should process digits */
		err = svd_handle_digit (svd, chan_idx, digit);
		if(err){
			/* clear call params */
			svd_clear_call (svd, ab_chan);
			goto __exit_fail;
		}
		/* stop playing any tone while dialing a number */
		err = ab_FXS_line_tone (ab_chan, ab_chan_tone_MUTE);
		if(err){
			SU_DEBUG_2(("can`t stop playing tone on [%02d]\n",ab_chan->abs_idx));
		}
		/* stop playing tone */
		SU_DEBUG_8(("stop playing tone on [%02d]\n",ab_chan->abs_idx));
	}
DFE
	return 0;
__exit_fail:
DFE
	return -1;
}/*}}}*/

/**
 * \param[in] svd 		svd context structure.
 * \param[in] chan_idx 	channel on which event occures.
 * \param[in] data 		event data (0 if cedend and 1 if ced).
 * 		\retval -1	if somthing nasty happens.
 * 		\retval 0 	if etherything ok.
 */
static int
svd_handle_event_FM_CED ( svd_t * const svd, int const chan_idx,
		long const data  )
{/*{{{*/
	ab_chan_t * ab_chan = &svd->ab->chans[chan_idx];
	int err;
DFS
	if( !data){ /* CEDEND */
		err = ab_chan_fax_pass_through_start (ab_chan);
		if( err){
			SU_DEBUG_3(("can`t start fax_pass_through on [%02d]: %s\n",
					ab_chan->abs_idx, ab_g_err_str));
			goto __exit_fail;
		} else {
			SU_DEBUG_3(("fax_pass_through started on [%02d]\n",
					ab_chan->abs_idx));
		}
	} else { /* CED */
		/* empty now
		 * maybe in the future try to start fax there */
	}
DFE
	return 0;
__exit_fail:
DFE
	return -1;
}/*}}}*/

/**
 * \param[out] svd	svd context structure to initialize channels in it`s ab.
 * 		\retval -1	if somthing nasty happens.
 * 		\retval 0 	if etherything ok.
 */
static int
svd_chans_init ( svd_t * const svd )
{/*{{{*/
	int i;
	int chans_num;
DFS

	chans_num = svd->ab->chans_num;
	g_conf.channels = chans_num;
	for (i=0; i<chans_num; i++){
		ab_chan_t * curr_chan;
		svd_chan_t * chan_ctx;
		curr_chan = &svd->ab->chans[ i ];
		curr_chan->ctx = malloc(sizeof(svd_chan_t));
		chan_ctx = curr_chan->ctx;
		if( !chan_ctx ){
			SU_DEBUG_0 ((LOG_FNC_A (LOG_NOMEM_A("svd->ab->chans[i].ctx") ) ));
			goto __exit_fail;
		}
		memset (chan_ctx, 0, sizeof(*chan_ctx));
		chan_ctx->chan_idx = i;

	 	/* SDP */
		chan_ctx->rtp_sfd = -1;
		chan_ctx->remote_host = NULL;

		/* MEDIA REGISTER */
		svd_media_register (svd, curr_chan);

	 	/* HANDLE */
		chan_ctx->op_handle = NULL;

		/* ALL OTHER */
		chan_ctx->dtmf_tmr = su_timer_create(su_root_task(svd->root), 4000);
		if( !chan_ctx->dtmf_tmr){
			SU_DEBUG_1 (( LOG_FNC_A ("su_timer_create() dtmf fails" ) ));
		}
		svd_clear_call (svd, curr_chan);
	}
DFE
	return 0;
__exit_fail:
DFE
	return -1;
}/*}}}*/

/**
 * \param[in] svd	svd context structure to attach callbacks on ab->devs.
 * 		\retval -1	if somthing nasty happens.
 * 		\retval 0 	if etherything ok.
 */
static int
attach_dev_cb ( svd_t * const svd )
{/*{{{*/
	su_wait_t wait[1];
	ab_dev_t * curr_dev;
	int i;
	int j;
	int err;
DFS
	j = svd->ab->devs_num;
	for(i = 0; i < j; i++){
		curr_dev = &svd->ab->devs[ i ];
		err = su_wait_create (wait, curr_dev->cfg_fd, POLLIN);
		if(err){
			SU_DEBUG_0 ((LOG_FNC_A ("su_wait_create() fails" ) ));
			goto __exit_fail;
		}

		err = su_root_register( svd->root, wait, svd_atab_handler, curr_dev, 0);
		if (err == -1){
			SU_DEBUG_0 ((LOG_FNC_A ("su_root_register() fails" ) ));
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
 * Timer to end collection of dtmf digits and place a call.
 * \param[in] magic	svd pointer.
 * \param[in] t		initiator timer.
 * \param[in] arg	ab channel pointer.
 */
void
dtmf_timer_cb (su_root_magic_t *magic, su_timer_t *t, su_timer_arg_t *arg)
{/*{{{*/
	svd_t * svd = magic;
	svd_chan_t * chan_ctx = arg;
	/* led shows that a call is ongoing */
	if (g_conf.chan_led[chan_ctx->chan_idx])
		led_blink(g_conf.chan_led[chan_ctx->chan_idx], LED_SLOW_BLINK);
	/* place a call */
	if (svd_invite_to(svd, chan_ctx->chan_idx, chan_ctx->dial_status.digits))
		ab_FXS_line_tone (&svd->ab->chans[chan_ctx->chan_idx], ab_chan_tone_BUSY);
	  

}/*}}}*/

/**
 * \param[in] svd		svd context structure.
 * \param[in] chan_idx 	channel on which event occures.
 * \param[in] digit 	current digit from parsing sequence.
 * 		\retval -1	if somthing nasty happens.
 * 		\retval 0 	if etherything ok.
 */
static int
svd_handle_digit( svd_t * const svd, int const chan_idx, long const digit )
{/*{{{*/
	ab_t * ab = svd->ab;
	svd_chan_t * chan_ctx = ab->chans[ chan_idx ].ctx;
	int * net_idx = & (chan_ctx->dial_status.num_digits);
	int err = 0;
DFS
	if (digit != PLACE_CALL_MARKER){
		/* put input digits to buffer */
		chan_ctx->dial_status.digits[ *net_idx ] = digit;
		++(*net_idx);
		/* start timer */
		err = su_timer_set(chan_ctx->dtmf_tmr, dtmf_timer_cb, chan_ctx);
		if (err){
			SU_DEBUG_2 (("su_timer_set ERROR on [%02d] : %d (dtmf_tmr)\n",
						chan_idx, err));
			goto __exit_fail;
		}
	} else {
		/* stop the timer */
		su_timer_reset(chan_ctx->dtmf_tmr);
		/* led shows that a call is ongoing */
		if (g_conf.chan_led[chan_ctx->chan_idx])
			led_blink(g_conf.chan_led[chan_ctx->chan_idx], LED_SLOW_BLINK);
		/* place a call */
		if (svd_invite_to(svd, chan_idx, chan_ctx->dial_status.digits))
			ab_FXS_line_tone (&ab->chans[chan_ctx->chan_idx], ab_chan_tone_BUSY);

		goto __exit_success;
	}
	
	/* when buffer is full but we did not find terminant*/
	if(*net_idx == ADDR_PAYLOAD_LEN){
		SU_DEBUG_2 (("Too long digits [%s] it should be "
				"not longer then %d chars\n",
				chan_ctx->dial_status.digits,
				ADDR_PAYLOAD_LEN));
		goto __exit_fail;
	}
	
DFE
__exit_success:
	return 0;
__exit_fail:
	return -1;
}/*}}}*/

/**
 * \param[in] 	root 		root object that contain wait object.
 * \param[in] 	w			wait object that emits.
 * \param[in] 	user_data	channel that gives RTP data.
 * \retval -1	if somthing nasty happens.
 * \retval 0 	if etherything is ok.
 */
static int
svd_media_tapi_handle_local_data (su_root_magic_t * root, su_wait_t * w,
		su_wakeup_arg_t * user_data)
{/*{{{*/
	ab_chan_t * chan = user_data;
	svd_chan_t * chan_ctx = chan->ctx;
	struct sockaddr_in target_sock_addr;
	unsigned char buf [BUFF_PER_RTP_PACK_SIZE];
	int rode;
	int sent;

	if (chan_ctx->remote_port == 0 ||
			chan_ctx->remote_host == NULL){
		rode = read(chan->rtp_fd, buf, sizeof(buf));
		SU_DEBUG_2(("HLD:%d|",rode));
		goto __exit_success;
	}

	memset(&target_sock_addr, 0, sizeof(target_sock_addr));

	target_sock_addr.sin_family = AF_INET;
	target_sock_addr.sin_port = htons(chan_ctx->remote_port);
	inet_aton (chan_ctx->remote_host, &target_sock_addr.sin_addr);
	rode = read(chan->rtp_fd, buf, sizeof(buf));

	if (rode == 0){
		SU_DEBUG_2 ((LOG_FNC_A("wrong event")));
		goto __exit_fail;
	} else if(rode > 0){
		// should not block
		sent = sendto(chan_ctx->rtp_sfd, buf, rode, 0,
				&target_sock_addr, sizeof(target_sock_addr));
		if (sent == -1){
			SU_DEBUG_2 (("HLD() ERROR : sent() : %d(%s)\n",
					errno, strerror(errno)));
			goto __exit_fail;
		} else if (sent != rode){
			SU_DEBUG_2(("HLD() ERROR :RODE FROM rtp_stream : %d, but "
					"SENT TO socket : %d\n",
					rode, sent));
			goto __exit_fail;
		}
	} else {
		SU_DEBUG_2 (("HLD() ERROR : read() : %d(%s)\n",
				errno, strerror(errno)));
		goto __exit_fail;
	}
__exit_success:
	return 0;
__exit_fail:
	return -1;
}/*}}}*/

/**
 * \param[in] 		root 		root object that contain wait object.
 * \param[in] 		w			wait object that emits.
 * \param[in,out]	user_data	channel that receives RTP data from socket.
 * \retval -1	if somthing nasty happens.
 * \retval 0 	if etherything is ok.
 */
static int
svd_media_tapi_handle_remote_data (su_root_magic_t * root, su_wait_t * w,
		su_wakeup_arg_t * user_data)
{/*{{{*/
	ab_chan_t * chan = user_data;
	svd_chan_t * chan_ctx = chan->ctx;
	unsigned char buf [BUFF_PER_RTP_PACK_SIZE];
	int received;
	int writed;

	assert( chan_ctx->rtp_sfd != -1 );

	received = recv(chan_ctx->rtp_sfd, buf, sizeof(buf), 0);

	if (received == 0){
		SU_DEBUG_2 ((LOG_FNC_A("wrong event")));
		goto __exit_fail;
	} else if (received > 0){
		/* should not block */
		writed = write(chan->rtp_fd, buf, received);
		if (writed == -1){
			SU_DEBUG_2 (("HRD() ERROR: write() : %d(%s)\n",
					errno, strerror(errno)));
			goto __exit_fail;
		} else if (writed != received){
			SU_DEBUG_2(("HRD() ERROR: RECEIVED FROM socket : %d, but "
					"WRITED TO rtp-stream : %d\n",
					received, writed));
			goto __exit_fail;
		}
	} else {
		SU_DEBUG_2 (("HRD() ERROR : recv() : %d(%s)\n",
				errno, strerror(errno)));
		goto __exit_fail;
	}
	return 0;
__exit_fail:
	return -1;
}/*}}}*/

/**
 * \param[in,out] chan_ctx 	channel context on which open socket, and set
 * 		socket parameters.
 * \retval -1	if somthing nasty happens.
 * \retval 0 	if etherything is ok.
 * \remark
 * 		It uses \ref g_conf to read \ref svd_conf_s::rtp_port_first and
 * 		\ref svd_conf_s::rtp_port_last.
 */
static int
svd_media_tapi_open_rtp (svd_chan_t * const chan_ctx)
{/*{{{*/
	int i;
	long ports_count;
	struct sockaddr_in my_addr;
#ifndef DONT_BIND_TO_DEVICE
	struct ifreq ifr;
#endif
	int sock_fd;
	int rtp_binded = 0;
	int err;
	int tos = 0;
DFS
	sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock_fd == -1) {
		SU_DEBUG_1 (("OPEN_RTP() ERROR : socket() : %d(%s)\n",
				errno, strerror(errno)));
		goto __exit_fail;
	}

	//tos = g_conf.rtp_tos & IPTOS_TOS_MASK;
	tos = g_conf.rtp_tos & 0xFF;
	err = setsockopt(sock_fd, IPPROTO_IP, IP_TOS, &tos, sizeof(tos));
	if( err){
		SU_DEBUG_2(("Can`t set TOS :%s",strerror(errno)));
	}

#ifndef DONT_BIND_TO_DEVICE
	/* Set SO_BINDTODEVICE for right ip using */
	if (chan_ctx->account) {
		strcpy(ifr.ifr_name, chan_ctx->account->rtp_interface);
	} else {
		strcpy(ifr.ifr_name, "lo");
	}
	if(setsockopt (sock_fd, SOL_SOCKET, SO_BINDTODEVICE, &ifr,
			sizeof (ifr)) < 0 ){
		goto __sock_opened;
	}
#endif

	/* Bind socket to appropriate address */
	ports_count = g_conf.rtp_port_last - g_conf.rtp_port_first + 1;
	for (i = 0; i < ports_count; i++) {
		memset(&my_addr, 0, sizeof(my_addr));
		my_addr.sin_family = AF_INET;
		my_addr.sin_port = htons(g_conf.rtp_port_first + i);
		my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		if ((bind(sock_fd, &my_addr, sizeof(my_addr))) != -1) {
			chan_ctx->rtp_port = g_conf.rtp_port_first + i;
			rtp_binded = 1;
			break;
		}
	}
	if( !rtp_binded ){
		SU_DEBUG_1(("svd_media_tapi_open_rtp(): could not find free "
				"port for RTP in range [%d,%d]\n",
				g_conf.rtp_port_first, g_conf.rtp_port_last));
		goto __sock_opened;
	}

DFE
	return sock_fd;

__sock_opened:
	if(close (sock_fd)){
		SU_DEBUG_2 (("OPEN_RTP() ERROR : close() : %d(%s)\n",
				errno, strerror(errno)));
	}
__exit_fail:
DFE
	return -1;
}/*}}}*/

/**
 * \param[in,out] ctx 	channel context on which we set socket parameters.
 * \retval -1	if somthing nasty happens.
 * \retval 0 	if etherything is ok.
 */
int
svd_media_tapi_rtp_sock_rebinddev (svd_chan_t * const ctx)
{/*{{{*/
#ifndef DONT_BIND_TO_DEVICE
	struct ifreq ifr;
#endif
DFS
#ifndef DONT_BIND_TO_DEVICE
	/* use account interface */
	strcpy(ifr.ifr_name, ctx->account->rtp_interface);
	SU_DEBUG_0 (("THE NAME OF THE DEVICE : %s\n",ifr.ifr_name));
	if(setsockopt (ctx->rtp_sfd, SOL_SOCKET, SO_BINDTODEVICE, &ifr,
			sizeof (ifr)) < 0 ){
		SU_DEBUG_1 ((LOG_FNC_A(strerror(errno))));
		goto __exit_fail;
	}
#endif	
DFE
	return 0;
#ifndef DONT_BIND_TO_DEVICE
__exit_fail:
DFE
	return -1;
#endif
}/*}}}*/
