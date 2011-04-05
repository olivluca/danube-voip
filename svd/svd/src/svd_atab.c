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
/** Process FXO Ring event.*/
static int svd_handle_event_FXO_RINGING
		( svd_t * const svd, int const chan_idx );
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
/** START state in dialed sequence.*/
static int svd_handle_START
		( ab_t * const ab, int const chan_idx, long const digit );
/** ROUTE_ID state in dialed sequence.*/
static int svd_handle_ROUTE_ID
		( ab_t * const ab, int const chan_idx, long const digit );
/** CHAN_ID state in dialed sequence.*/
static int svd_handle_CHAN_ID
		( svd_t * const svd, int const chan_idx, long const digit );
/** NET_ADDR state in dialed sequence.*/
static int svd_handle_NET_ADDR
		( svd_t * const svd, int const chan_idx, long const digit );
/** ADDR_BOOK state in dialed sequence.*/
static int svd_handle_ADDR_BOOK
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
static int svd_media_vinetic_handle_local_data (su_root_magic_t * root,
		su_wait_t * w, su_wakeup_arg_t * user_data );
/** Move RTP data from RTP socket to channel.*/
static int svd_media_vinetic_handle_remote_data (su_root_magic_t * root,
		su_wait_t * w, su_wakeup_arg_t * user_data );
/** Open RTP socket.*/
static int svd_media_vinetic_open_rtp (svd_chan_t * const chan_ctx);
/** @}*/



/** @defgroup ATA_OTHER_I Other internals.
 *  @ingroup ATA_B
 *  Other internal helper functions.
 *  @{*/
/** Init chans context from \ref g_conf.*/
static int svd_chans_init ( svd_t * const svd );
/** Attach callback for ATA devices.*/
static int attach_dev_cb ( svd_t * const svd );
/** Find address book value for given channel. */
static int svd_ab_value_set_by_id (ab_chan_t * const ab_chan);
/** Parse given dial sequence.*/
static int svd_process_addr (svd_t * const svd, int const chan_idx,
		char const * const value);
/** Set router ip from \ref g_conf using channel dial context info.*/
static int set_route_ip (svd_chan_t * const chan_ctx);
/** Choose direction for call in local network.*/
static int local_connection_selector
		( svd_t * const svd, int const use_ff_FXO, ab_chan_t * const chan);
/** @}*/

/** @defgroup ATA_VF_I Voice Frequency CRAM internals.
 *  @ingroup ATA_B
 *  Other internal VF definitions.
 *  @{*/
#define AB_FW_CRAM_VFN2_NAME "/lib/firmware/cramfw_vfn2.bin"
#define AB_FW_CRAM_VFN4_NAME "/lib/firmware/cramfw_vfn4.bin"
#define AB_FW_CRAM_VFT2_NAME "/lib/firmware/cramfw_vft2.bin"
#define AB_FW_CRAM_VFT4_NAME "/lib/firmware/cramfw_vft4.bin"
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
	int i;
	int ch_num;
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

	/* REINIT tf-channels with CRAM */
	ch_num = svd->ab->chans_num;
	for (i=0; i<CHANS_MAX; i++){
		char * cram_file_path;
		/* reinit cram coefficients */
		if(g_conf.voice_freq[i].is_set){
			if       (g_conf.voice_freq[i].type == vf_type_N4){
				cram_file_path = AB_FW_CRAM_VFN4_NAME;
			} else if(g_conf.voice_freq[i].type == vf_type_N2){
				cram_file_path = AB_FW_CRAM_VFN2_NAME;
			} else if(g_conf.voice_freq[i].type == vf_type_T4){
				cram_file_path = AB_FW_CRAM_VFT4_NAME;
			} else if(g_conf.voice_freq[i].type == vf_type_T2){
				cram_file_path = AB_FW_CRAM_VFT2_NAME;
			}
			if(ab_chan_cram_init(svd->ab->pchans[i], cram_file_path)){
				SU_DEBUG_0 ((LOG_FNC_A("can`t init channel with given CRAM")));
				goto __exit_fail;
			}
		}
	}

	/* set gpio on VF-devices to proper values */
	if (ab_devs_vf_gpio_reset (svd->ab)){
		SU_DEBUG_0 ((LOG_FNC_A("can`t reinitilize VF GPIO")));
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
			if (curr_chan->dial_status.route_id){
				free (curr_chan->dial_status.route_id);
			}
			if (curr_chan->dial_status.addrbk_id){
				free (curr_chan->dial_status.addrbk_id);
			}
			if (curr_chan->ring_tmr){
				su_timer_destroy(curr_chan->ring_tmr);
			}
			if (curr_chan->vf_tmr){
				su_timer_destroy(curr_chan->vf_tmr);
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
			svd_media_vinetic_handle_local_data, chan, 0);
	if (ret == -1){
		SU_DEBUG_0 ((LOG_FNC_A ("su_root_register() fails" ) ));
		goto __exit_fail;
	}
	chan_ctx->local_wait_idx = ret;

	ret = svd_media_vinetic_open_rtp (chan_ctx);
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
			svd_media_vinetic_handle_remote_data, chan, 0);
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
	int len;
	int size;
DFS
	/* STATES */
	chan_ctx->dial_status.state = dial_state_START;

	/* TAG */
	chan_ctx->dial_status.tag = 0;

	/* DEST_IS_SELF */
	chan_ctx->dial_status.dest_is_self = self_UNDEFINED;

	/* ROUTER */
	len = g_conf.route_table.id_len;
	if( len ){
		size = sizeof(*(chan_ctx->dial_status.route_id));
		memset(chan_ctx->dial_status.route_id, 0, size * (len+1));
	}
	/* route_ip  */
	chan_ctx->dial_status.route_ip = NULL;

	/* CHAN */
	size = sizeof(chan_ctx->dial_status.chan_id);
	memset (chan_ctx->dial_status.chan_id, 0, size);

	/* ADDRBOOK */
	len = g_conf.address_book.id_len;
	if( len ){
		size = sizeof(*(chan_ctx->dial_status.addrbk_id));
		memset (chan_ctx->dial_status.addrbk_id, 0, size * (len+1));
	}

	/* ADDR_PAYLOAD */
	size = sizeof(chan_ctx->dial_status.addr_payload);
	memset (chan_ctx->dial_status.addr_payload, 0, size);

	/* Caller remote or not */
	chan_ctx->call_type = calltype_UNDEFINED;
	/* Caller router is self or not */
	chan_ctx->caller_router_is_self = 0;

	/* SDP */
	chan_ctx->sdp_payload = -1;
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
DFE
}/*}}}*/

/**
 * \param[in] ab 				ata board structure.
 * \param[in] self_chan_idx 	self channel index in ab->chans[].
 * \return
 * 		channel index in ab->chans[].
 * 		\retval -1				if no free FXO channels available.
 * 		\retval <other_value>	if we found one.
 * \sa get_FF_FXS_idx().
 */
int
get_FF_FXO_idx ( ab_t const * const ab, char const self_chan_idx )
{/*{{{*/
	int i;
	int j;
DFS
	j = ab->chans_num;
	for (i = 0; i < j; i++){
		ab_chan_t * curr_chan = &ab->chans[i];
		svd_chan_t * chan_ctx = curr_chan->ctx;
		if (curr_chan->parent->type == ab_dev_type_FXO &&
				chan_ctx->op_handle == NULL &&
				i != self_chan_idx ){
			break;
		}
	}
	if (i == j){
		SU_DEBUG_2(("No free FXO channel available\n"));
		i = -1;
	}
DFE
	return i;
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
			&g_conf.audio_prms[chan->abs_idx]);
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
	codec_t * ct = NULL;
	cod_prms_t const * cp = NULL;
	int i;
	int is_vf = 0;

	/* prepare fcod */
	ctx->fcod.type = g_conf.fax.codec_type;
	if       (ctx->call_type == calltype_LOCAL){
		ct = g_conf.int_codecs;
		ctx->fcod.sdp_selected_payload = g_conf.fax.internal_pt;
	} else if(ctx->call_type == calltype_REMOTE){
		ct = g_conf.sip_set.ext_codecs;
		ctx->fcod.sdp_selected_payload = g_conf.fax.external_pt;
	} else if(ctx->call_type == calltype_UNDEFINED){
		SU_DEBUG_1((LOG_FNC_A("Calltype still undefined, can`t start media")));
		goto __exit_fail;
	}

	if(g_conf.voice_freq[chan->abs_idx].is_set){
		is_vf = 1;
		ct = &g_conf.voice_freq[chan->abs_idx].vf_codec;
	}

	/* prepare vcod */
	/* find codec type by sdp_name */
	cp = svd_cod_prms_get(cod_type_NONE ,ctx->sdp_cod_name);
	if( !cp){
		SU_DEBUG_1((LOG_FNC_A("Can`t get codec params by name")));
		goto __exit_fail;
	}
	ctx->vcod.type = cp->type;
	ctx->vcod.sdp_selected_payload = ctx->sdp_payload;

	for (i=0; ct[i].type!=cod_type_NONE; i++){
		if(ct[i].type == cp->type){
			ctx->vcod.pkt_size = ct[i].pkt_size;
			*jpb = &ct[i].jb;
			break;
		}
		if(is_vf){
			break;
		}
	}

	return 0;
__exit_fail:
	return -1;
}/*}}}*/

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
	static int cp_inited = 0;
	static int get_next = 0;
	static cod_prms_t cp[COD_MAS_SIZE] = {0};
	int i;
	int ret_idx = -1;
	int err;
	int pass_type = 0;
	int pass_name = 0;

	if(ct == cod_type_NONE){
		pass_type = 1;
	}
	if( !cn){
		pass_name = 1;
	}
	if( !cp_inited){
		err = svd_init_cod_params(cp);
		if(err){
			goto __exit_fail;
		}
		cp_inited = 1;
	}

	if(pass_type && pass_name){
		ret_idx = get_next;
		if(cp[get_next].type == cod_type_NONE){
			get_next = 0;
		} else {
			get_next++;
		}
		goto __exit_success;
	}

	for (i=0; cp[i].type != cod_type_NONE; i++){
		if(		(pass_type || (cp[i].type == ct)) &&
				(pass_name || (!strcmp(cp[i].sdp_name, cn)))){
			ret_idx = i;
			break;
		}
	}

	if(ret_idx == -1){
		goto __exit_fail;
	}


__exit_success:
	return &cp[ret_idx];
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
#if 0
		/* tag__ stress-testing */
		int abs_idx = svd->ab->chans[chan_idx].abs_idx;
		if((abs_idx == 0) ||
			(abs_idx == 1) ||
			(abs_idx == 2) ||
			(abs_idx == 3)){
			/* do not do anything */
		} else {
#endif
		SU_DEBUG_8 (("Got fxo ringing event: 0x%X on [%d/%d]\n",
				evt.data, dev_idx,evt.ch ));
		err = svd_handle_event_FXO_RINGING (svd, chan_idx);
	//	}
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
	int call_answered;
	int err;
DFS
	/* stop ringing */
	err = ab_FXS_line_ring( ab_chan, ab_chan_ring_MUTE );
	if (err){
		SU_DEBUG_0(("can`t stop ring on [%02d]\n", ab_chan->abs_idx));
		svd_answer(svd, ab_chan, SIP_500_INTERNAL_SERVER_ERROR);
		goto __exit_fail;
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
		goto __exit_success;
	}

	/* no call to answer */

	/* hotline test */
	if(((svd_chan_t *)(ab_chan->ctx))->is_hotlined){
		/* process the hotline sequence */
		err = svd_process_addr (svd, chan_idx,
				((svd_chan_t *)(ab_chan->ctx))->hotline_addr);
		if(err){
			goto __exit_fail;
		}
	} else {
		/* no hotline - play dialtone */
		err = ab_FXS_line_tone (ab_chan, ab_chan_tone_DIAL);
		if(err){
			SU_DEBUG_2(("can`t play dialtone on [%02d]\n",ab_chan->abs_idx));
		}
		SU_DEBUG_8(("play dialtone on [%02d]\n",ab_chan->abs_idx));
	}
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
	int err;

DFS
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
		/* allready connected - we can send info
		 * see rfc_2976
		 */
		int tone = (data >> 8);
		char pd[INFO_STR_LENGTH];

		memset (pd, 0, sizeof(pd[0]));
		snprintf(pd, INFO_STR_LENGTH, INFO_STR, tone, digit);

		/* send INFO */
		nua_info(chan_ctx->op_handle,
				SIPTAG_PAYLOAD_STR(pd),
				TAG_NULL());
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
 * It is callback on the rings to FXO channels.
 * \param[in] magic	svd pointer.
 * \param[in] t		initiator timer.
 * \param[in] arg	ab channel pointer.
 * \remark
 *	Used to measure time delays between two ring detection. This function
 *	should be cancelled on the every incoming ring. If it is not cancelled
 *	it will send SIP CANCEL to cancel the invite request, because nobody
 *	rings on FXO anymore.
 */
void
ring_timer_cb (su_root_magic_t *magic, su_timer_t *t, su_timer_arg_t *arg)
{/*{{{*/
	ab_chan_t * chan = arg;
	svd_chan_t * ctx = chan->ctx;

	/* if FXS-side answers - just out */
	if( ctx->call_state == nua_callstate_ready ||
		ctx->call_state == nua_callstate_completing ){
		ctx->ring_state = ring_state_NO_TIMER_INVITE_SENT;
		SU_DEBUG_4(("%s():%d RING_STATE [%02d] IS %d\n",
				__func__, __LINE__, chan->abs_idx, ctx->ring_state));
		return;
	}

	/* nobody answers, and no ring coming in RING_WAIT_DROP seconds */
	SU_DEBUG_3 (("no RING on [%02d], in %d sec\n -> send SIP CANCEL\n",
			chan->abs_idx, RING_WAIT_DROP));
	nua_cancel (ctx->op_handle, TAG_NULL());
	ctx->ring_state = ring_state_CANCEL_IN_QUEUE;
	SU_DEBUG_4(("%s():%d RING_STATE [%02d] IS %d\n",
			__func__, __LINE__, chan->abs_idx, ctx->ring_state));
}/*}}}*/

/**
 * \param[in] svd 		svd context structure.
 * \param[in] chan_idx 	channel on which event occures.
 * 		\retval -1	if somthing nasty happens.
 * 		\retval 0 	if etherything ok.
 */
static int
svd_handle_event_FXO_RINGING ( svd_t * const svd, int const chan_idx )
{/*{{{*/
	ab_chan_t * ab_chan = &svd->ab->chans[chan_idx];
	svd_chan_t * chan_ctx = ab_chan->ctx;
	int err;
DFS
	/* not hotlined FXO - error case */
	if( !chan_ctx->is_hotlined){
		SU_DEBUG_3 (("Got ringing event on channel [%02d], it is not "
				"hotlined, but should be\n", ab_chan->abs_idx));
		goto __exit_fail;
	}

	/* if connection is already prepared -
	 * we will (or already) initiate connection to FXS on it`s INVITE  */
	if( chan_ctx->call_state == nua_callstate_received   ||
		chan_ctx->call_state == nua_callstate_early      ||
		chan_ctx->call_state == nua_callstate_completed  ||
		chan_ctx->call_state == nua_callstate_completing ||
		chan_ctx->call_state == nua_callstate_ready ){
		SU_DEBUG_8 (("Connection already initiated on [%02d]: ignoring ring\n",
				ab_chan->abs_idx));
		goto __exit_success;
	}

	if       ( chan_ctx->ring_state == ring_state_NO_RING_BEFORE){
		/* process the hotline sequence */
		err = svd_process_addr (svd, chan_idx, chan_ctx->hotline_addr);
		if(err){
			goto __exit_fail;
		}
		chan_ctx->ring_state = ring_state_INVITE_IN_QUEUE;
		SU_DEBUG_4(("%s():%d RING_STATE [%02d] IS %d\n",
				__func__, __LINE__, ab_chan->abs_idx, chan_ctx->ring_state));
	}
	else if( chan_ctx->ring_state == ring_state_INVITE_IN_QUEUE) {
		/* don`t do nothing before invite */
		SU_DEBUG_4(("%s():%d RING_STATE [%02d] IS %d\n",
				__func__, __LINE__, ab_chan->abs_idx, chan_ctx->ring_state));
	} else if( chan_ctx->ring_state == ring_state_NO_TIMER_INVITE_SENT) {
		/* timer up error occured - try to start it again */
		err = su_timer_set_interval(chan_ctx->ring_tmr, ring_timer_cb, ab_chan,
				RING_WAIT_DROP*1000);
		if (err){
			SU_DEBUG_2 (("su_timer_set_interval ERROR on [%02d] : %d\n",
						ab_chan->abs_idx, err));
			chan_ctx->ring_state = ring_state_NO_TIMER_INVITE_SENT;
			SU_DEBUG_4(("%s():%d RING_STATE [%02d] IS %d\n",
					__func__, __LINE__, ab_chan->abs_idx, chan_ctx->ring_state));
			goto __exit_fail;
		}
		chan_ctx->ring_state = ring_state_TIMER_UP_INVITE_SENT;
		SU_DEBUG_4(("%s():%d RING_STATE [%02d] IS %d\n",
				__func__, __LINE__, ab_chan->abs_idx, chan_ctx->ring_state));
	} else if( chan_ctx->ring_state == ring_state_TIMER_UP_INVITE_SENT) {
		/* ring already in process - should restart timer */
		SU_DEBUG_3 (("Got ringing event on channel [%02d], it is already "
				"in process\n", ab_chan->abs_idx));
		err = su_timer_reset(chan_ctx->ring_tmr);
		if (err){
			SU_DEBUG_2 (("su_timer_reset ERROR on [%02d] : %d\n",
						ab_chan->abs_idx, err));
			goto __exit_fail;
		}
		err = su_timer_set_interval(chan_ctx->ring_tmr, ring_timer_cb, ab_chan,
				RING_WAIT_DROP*1000);
		if (err){
			SU_DEBUG_2 (("su_timer_set_interval ERROR on [%02d] : %d\n",
						ab_chan->abs_idx, err));
			chan_ctx->ring_state = ring_state_NO_TIMER_INVITE_SENT;
			SU_DEBUG_4(("%s():%d RING_STATE [%02d] IS %d\n",
					__func__, __LINE__, ab_chan->abs_idx, chan_ctx->ring_state));
			goto __exit_fail;
		}
	} else if( chan_ctx->ring_state == ring_state_CANCEL_IN_QUEUE) {
		SU_DEBUG_4(("%s():%d RING_STATE [%02d] IS %d\n",
				__func__, __LINE__, ab_chan->abs_idx, chan_ctx->ring_state));
		/* don`t do nothing - after cancel if we still have incoming
		 * ring connection will up again */
	}
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
	unsigned char route_id_len;
	unsigned char addrbk_id_len;
	int chans_num;
DFS
	route_id_len = g_conf.route_table.id_len;
	addrbk_id_len = g_conf.address_book.id_len;

	chans_num = svd->ab->chans_num;
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

		/* ROUTER */
		/* route_id channel ctx sets */
		int route_id_sz = sizeof(*(chan_ctx->dial_status.route_id));

		chan_ctx->dial_status.route_id = malloc((route_id_len+1)*route_id_sz);
		if( !chan_ctx->dial_status.route_id ){
			SU_DEBUG_0 ((LOG_FNC_A (LOG_NOMEM_A
					("chans[i].ctx->dial_status.route_id") ) ));
			goto __exit_fail;
		}
		memset (chan_ctx->dial_status.route_id, 0,
				route_id_sz * (route_id_len + 1));

		/* ADDRBOOK */
		/* address_book id channel data sets */
		int adbk_sz = sizeof(*(chan_ctx->dial_status.addrbk_id));

		chan_ctx->dial_status.addrbk_id = malloc( (addrbk_id_len+1) * adbk_sz);
		if( !chan_ctx->dial_status.addrbk_id ){
			SU_DEBUG_0 ((LOG_FNC_A (LOG_NOMEM_A
					("chans[i].ctx->dial_status.addrbk_id") ) ));
			goto __exit_fail;
		}
		memset (chan_ctx->dial_status.addrbk_id, 0,(addrbk_id_len+1) * adbk_sz);

	 	/* SDP */
		chan_ctx->rtp_sfd = -1;
		chan_ctx->remote_host = NULL;

		/* MEDIA REGISTER */
		chan_ctx->dial_status.dest_is_self = self_YES; /* for binding socket */
		svd_media_register (svd, curr_chan);

	 	/* HANDLE */
		chan_ctx->op_handle = NULL;

		/* RING */
		chan_ctx->ring_state = ring_state_NO_RING_BEFORE;
		chan_ctx->ring_tmr = su_timer_create(su_root_task(svd->root), 0);
		if( !chan_ctx->ring_tmr){
			SU_DEBUG_1 (( LOG_FNC_A ("su_timer_create() ring fails" ) ));
		}

		/* VF */
		chan_ctx->vf_tmr = su_timer_create(su_root_task(svd->root), 0);
		if( !chan_ctx->vf_tmr){
			SU_DEBUG_1 (( LOG_FNC_A ("su_timer_create() vf fails" ) ));
		}

		/* ALL OTHER */
		svd_clear_call (svd, curr_chan);
	}

	/* HOTLINE */
	for (i=0; i<CHANS_MAX; i++){
		struct hot_line_s * curr_rec = &g_conf.hot_line[ i ];
		if (curr_rec->is_set){
			svd_chan_t * ctx = svd->ab->pchans[i]->ctx;
			ctx->is_hotlined = 1;
			ctx->hotline_addr = curr_rec->value;
		}
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
	int err = 0;
DFS
	switch( chan_ctx->dial_status.state ){
		case dial_state_START:
			err = svd_handle_START (ab, chan_idx, digit);
			break;
		case dial_state_ADDR_BOOK:
			err = svd_handle_ADDR_BOOK(svd, chan_idx, digit);
			break;
		case dial_state_ROUTE_ID:
			err = svd_handle_ROUTE_ID(ab, chan_idx, digit);
			break;
		case dial_state_CHAN_ID:
			err = svd_handle_CHAN_ID(svd, chan_idx, digit);
			break;
		case dial_state_NET_ADDR:
			err = svd_handle_NET_ADDR(svd, chan_idx, digit);
			break;
	}
DFE
	return err;
}/*}}}*/

/**
 * \param[in] ab		ata board structre to get chan from it.
 * \param[in] chan_idx 	channel on which event occures.
 * \param[in] digit 	current digit from parsing sequence.
 * 		\retval -1	if somthing nasty happens.
 * 		\retval 0 	if etherything ok.
 * \remark
 * 		Using \ref g_conf to read \c route_table.
 */
static int
svd_handle_START( ab_t * const ab, int const chan_idx, long const digit )
{/*{{{*/
	svd_chan_t * chan_ctx = ab->chans[chan_idx].ctx;

	switch(digit){
		case ADBK_MARKER:
			chan_ctx->dial_status.state = dial_state_ADDR_BOOK;
			break;
		case SELF_MARKER:
		case SELF_MARKER2:
			chan_ctx->dial_status.dest_is_self = self_YES;
			chan_ctx->dial_status.state = dial_state_CHAN_ID;
			break;
		default :
			if( !isdigit (digit) ){
				goto __exit_success;
			}
			chan_ctx->dial_status.route_id[ 0 ] = digit;
			if( g_conf.route_table.id_len != 1 ){
				chan_ctx->dial_status.state = dial_state_ROUTE_ID;
				chan_ctx->dial_status.tag = 1;
			} else {
				/* set route ip */
				int err;
				err = set_route_ip (chan_ctx);
				if (err){
					goto __exit_fail;
				}
				chan_ctx->dial_status.state = dial_state_CHAN_ID;
			}
			break;
	}

__exit_success:
	return 0;
__exit_fail:
	return -1;
}/*}}}*/

/**
 * \param[in] ab		ata board structre to get chan from it.
 * \param[in] chan_idx 	channel on which event occures.
 * \param[in] digit 	current digit from parsing sequence.
 * 		\retval -1	if somthing nasty happens.
 * 		\retval 0 	if etherything ok.
 * \remark
 * 		Using \ref g_conf to read \c route_table.
 */
static int
svd_handle_ROUTE_ID( ab_t * const ab, int const chan_idx, long const digit )
{/*{{{*/
	svd_chan_t * chan_ctx = ab->chans[chan_idx].ctx;
	int * route_idx = & (chan_ctx->dial_status.tag);
	int route_id_len = g_conf.route_table.id_len;

	if( !isdigit (digit) ){
		goto __exit_success;
	}
	chan_ctx->dial_status.route_id [*route_idx] = digit;
	++(*route_idx);
	if(*route_idx == route_id_len){
		/* we got all digits of route_id */
		/* set route ip */
		int err;
		err = set_route_ip (chan_ctx);
		if( err ){
			goto __exit_fail;
		}
		SU_DEBUG_3 (("Choosed router [%s]\n",chan_ctx->dial_status.route_id ));
		chan_ctx->dial_status.state = dial_state_CHAN_ID;
		chan_ctx->dial_status.tag = 0;
	}

__exit_success:
	return 0;
__exit_fail:
	return -1;
}/*}}}*/

/**
 * \param[in] svd		svd context structure.
 * \param[in] chan_idx 	channel on which event occures.
 * \param[in] digit 	current digit from parsing sequence.
 * 		\retval -1	if somthing nasty happens.
 * 		\retval 0 	if etherything ok.
 * \remark
 * 		Using \ref g_conf to read \c sip_set.
 */
static int
svd_handle_CHAN_ID( svd_t * const svd, int const chan_idx, long const digit )
{/*{{{*/
	svd_chan_t * chan_ctx = svd->ab->chans[chan_idx].ctx;
	int * chan_mas_idx = & (chan_ctx->dial_status.tag);
	int err = 0;

	if( *chan_mas_idx == 0 ){
		if( digit == FXO_MARKER ){
			err = local_connection_selector(svd, 1, &svd->ab->chans[chan_idx]);
			goto __exit;
		} else if( digit == NET_MARKER ){
			if (g_conf.sip_set.all_set){
				chan_ctx->dial_status.state = dial_state_NET_ADDR;
			} else {
				SU_DEBUG_2 (("No registration available\n"));
				goto __exit_fail;
			}
			goto __exit_success;
		}
	}

	if( !isdigit (digit) ){
		goto __exit_success;
	}
	chan_ctx->dial_status.chan_id [*chan_mas_idx] = digit;
	++(*chan_mas_idx);
	if (*chan_mas_idx == CHAN_ID_LEN-1){
		/* we got all digits of chan_id */
		SU_DEBUG_3 (("Choosed chan [%s]\n",chan_ctx->dial_status.chan_id ));
		err = local_connection_selector (svd, 0, &svd->ab->chans[chan_idx]);
	}

__exit:
	return err;
__exit_success:
	return 0;
__exit_fail:
	return -1;
}/*}}}*/

/**
 * \param[in] svd		svd context structure.
 * \param[in] chan_idx 	channel on which event occures.
 * \param[in] digit 	current digit from parsing sequence.
 * 		\retval -1	if somthing nasty happens.
 * 		\retval 0 	if etherything ok.
 */
static int
svd_handle_NET_ADDR( svd_t * const svd, int const chan_idx, long const digit )
{/*{{{*/
	ab_chan_t * ab_chan = &svd->ab->chans[chan_idx];
	svd_chan_t * chan_ctx = ab_chan->ctx;
	int * net_idx = & (chan_ctx->dial_status.tag);

	if (digit != NET_MARKER){
		/* put input digits to buffer */
		chan_ctx->dial_status.addr_payload[ *net_idx ] = digit;
		++(*net_idx);
	} else {
		/* place a call */
		svd_invite_to(svd, chan_idx, chan_ctx->dial_status.addr_payload);
		goto __exit_success;
	}

	/* when buffer is full but we did not find terminant*/
	if(*net_idx == ADDR_PAYLOAD_LEN){
		SU_DEBUG_2 (("Too long addr_payload [%s] it should be "
				"not longer then %d chars\n",
				chan_ctx->dial_status.addr_payload,
				ADDR_PAYLOAD_LEN));
		goto __exit_fail;
	}

__exit_success:
	return 0;
__exit_fail:
	return -1;
}/*}}}*/

/**
 * \param[in] svd		svd context structure.
 * \param[in] chan_idx 	channel on which event occures.
 * \param[in] digit 	current digit from parsing sequence.
 * 		\retval -1	if somthing nasty happens.
 * 		\retval 0 	if etherything ok.
 * \remark
 * 		Using \ref g_conf to read \c address_book.
 */
static int
svd_handle_ADDR_BOOK( svd_t * const svd, int const chan_idx, long const digit )
{/*{{{*/
	ab_chan_t * ab_chan = &svd->ab->chans[chan_idx];
	svd_chan_t * chan_ctx = ab_chan->ctx;
	int * ab_id_idx = & (chan_ctx->dial_status.tag);
	int err = 0;

	/* put input digits to buffer */
	chan_ctx->dial_status.addrbk_id[ *ab_id_idx ] = digit;
	++(*ab_id_idx);

	/* when buffer is full */
	if(*ab_id_idx == g_conf.address_book.id_len){
		*ab_id_idx = 0;
		/* find appropriate addrbook value */
		err = svd_ab_value_set_by_id(ab_chan);
		if(err){
			goto __exit_fail;
		}
		/* process it */
		err = svd_process_addr (svd, chan_idx,
				chan_ctx->dial_status.addrbk_value);
		if(err){
			goto __exit_fail;
		}
	}

	return 0;
__exit_fail:
	return -1;
}/*}}}*/

/**
 * \param[in,out] ab_chan 	channel for which we try to set address book value.
 * 		\retval -1	if somthing nasty happens.
 * 		\retval 0 	if etherything ok.
 * \remark
 * 		Using \ref g_conf to read \c address_book.
 */
static int
svd_ab_value_set_by_id (ab_chan_t * const ab_chan)
{/*{{{*/
	svd_chan_t * chan_ctx = ab_chan->ctx;
	int i;
	int j;
	struct adbk_record_s * cur_rec;

	j = g_conf.address_book.records_num;
	for (i=0; i<j; i++){
		cur_rec = &g_conf.address_book.records[ i ];
		if( !strcmp (cur_rec->id, chan_ctx->dial_status.addrbk_id) ){
			chan_ctx->dial_status.addrbk_value = cur_rec->value;
			goto __exit_success;
		}
	}
	SU_DEBUG_2(("Wrong address book id : [%s]\n",
			chan_ctx->dial_status.addrbk_id));
	return -1;
__exit_success:
	return 0;
}/*}}}*/

/**
 * \param[in] svd		svd context structure.
 * \param[in] chan_idx 	channel on which we should dial a sequence.
 * \param[in] value 	sequence to parse.
 * 		\retval -1	if somthing nasty happens.
 * 		\retval 0 	if etherything ok.
 * \todo
 * 		Calling number on hotline is not implemented.
 * 		In hotline we can just choose the router and the chan.
 */
static int
svd_process_addr (svd_t * const svd, int const chan_idx,
		char const * const value)
{/*{{{*/
	ab_chan_t * ab_chan = &svd->ab->chans[chan_idx];
	svd_chan_t * chan_ctx = ab_chan->ctx;
	int i;
	int err;
DFS
	chan_ctx->dial_status.state = dial_state_START;

	for(i=0; value[i]; i++){
		err = svd_handle_digit ( svd, chan_idx, value[ i ] );
		if(err){
			/* clear call params */
			svd_clear_call (svd, ab_chan);
			goto __exit_fail;
		}
		if(i!=0 && chan_ctx->dial_status.state==dial_state_START){
			/* after that - just ',' and dialtones
			 * not processed now if want to process -
			 * should store to buffer and play it later */
			i++;
			break;
		}
	}
DFE
	return 0;
__exit_fail:
DFE
	return -1;
}/*}}}*/

/**
 * Cleans old route_ip and sets new ip if router is not self
 * 		set self flag to proper value.
 *
 * \param[in] chan_ctx 	channel context from which we got route id and
 * 		set it`s ip.
 * 	\retval -1	if somthing nasty happens (Wrong router id).
 * 	\retval 0 	self flag has been set to proper value and
 * 		router ip has been set if router is not self.
 * \remark
 * 		It uses \ref g_conf to read the \c self_number and \c route_table.
 */
static int
set_route_ip (svd_chan_t * const chan_ctx)
{/*{{{*/
	char * g_conf_id;
	char * route_id = chan_ctx->dial_status.route_id;
	int rec_idx;
	int routers_num;
	int ip_find = 0;

	if(chan_ctx->dial_status.dest_is_self == self_YES){
		SU_DEBUG_3 (("Choosed router is self\n"));
		goto __exit_success;
	}
	if( !strcmp(g_conf.self_number, route_id) ){
		/* it is self */
		SU_DEBUG_3 (("Choosed router is self\n"));
		chan_ctx->dial_status.dest_is_self = self_YES;
		goto __exit_success;
	}
	chan_ctx->dial_status.dest_is_self = self_NO;

	routers_num = g_conf.route_table.records_num;

	for( rec_idx=0; rec_idx<routers_num; rec_idx++ ){
		g_conf_id = g_conf.route_table.records [rec_idx].id;
		if( !strcmp( g_conf_id, route_id ) ){
			ip_find = 1;
			break;
		}
	}
	if (ip_find){
		chan_ctx->dial_status.route_ip = g_conf.route_table.records[rec_idx].value;
	} else {
		SU_DEBUG_2(("Wrong router id [%s]\n", route_id ));
		goto __exit_fail;
	}

__exit_success:
	return 0;
__exit_fail:
	return -1;
}/*}}}*/

/**
 * \param[in] svd			svd context structure.
 * \param[in] use_ff_FXO	should we dial to first free FXO channel.
 * \param[in] chan  		channel that initiate connection.
 * \retval -1	if somthing nasty happens.
 * \retval 0 	if etherything is ok.
 * \remark
 * 		It is make a choice - is it a self_call (call on self router) or it
 * 		is call on local network.
 */
static int
local_connection_selector
		( svd_t * const svd, int const use_ff_FXO, ab_chan_t * const chan)
{/*{{{*/
	svd_chan_t * chan_ctx = chan->ctx;
	int err;
DFS
	chan_ctx->dial_status.state = dial_state_START;

	if( chan_ctx->dial_status.dest_is_self == self_YES ){
		/* Destination router is self */
		chan_ctx->dial_status.route_ip = g_conf.self_ip;
	}
	err = svd_invite( svd, use_ff_FXO, chan);
DFE
	return err;
}/*}}}*/

/**
 * \param[in] 	root 		root object that contain wait object.
 * \param[in] 	w			wait object that emits.
 * \param[in] 	user_data	channel that gives RTP data.
 * \retval -1	if somthing nasty happens.
 * \retval 0 	if etherything is ok.
 */
static int
svd_media_vinetic_handle_local_data (su_root_magic_t * root, su_wait_t * w,
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
svd_media_vinetic_handle_remote_data (su_root_magic_t * root, su_wait_t * w,
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
svd_media_vinetic_open_rtp (svd_chan_t * const chan_ctx)
{/*{{{*/
	int i;
	long ports_count;
	struct sockaddr_in my_addr;
	struct ifreq ifr;
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
	if(chan_ctx->dial_status.dest_is_self == self_YES ||
			chan_ctx->caller_router_is_self){
		/* use local interface */
		strcpy(ifr.ifr_name, "lo");
		if(setsockopt (sock_fd, SOL_SOCKET, SO_BINDTODEVICE, &ifr,
				sizeof (ifr)) < 0 ){
			goto __sock_opened;
		}
	} else if(chan_ctx->dial_status.dest_is_self == self_NO ||
			(!chan_ctx->caller_router_is_self)){
		/* use <self> set interface */
		/* get device name by self_ip */
		int stmp;
		int device_finded = 0;

		stmp = socket (PF_INET, SOCK_STREAM, 0);
		if(stmp == -1){
			SU_DEBUG_1 ((LOG_FNC_A(strerror(errno))));
			goto __exit_fail;
		}
		for(i=1;;i++){
			struct sockaddr_in *sin = (struct sockaddr_in *) &ifr.ifr_addr;
			char *ip;

			ifr.ifr_ifindex = i;
			if (ioctl (stmp, SIOCGIFNAME, &ifr) < 0){
				break;
			}
			if (ioctl (stmp, SIOCGIFADDR, &ifr) < 0){
				continue;
			}
			ip = inet_ntoa (sin->sin_addr);
			if( !strcmp(ip, g_conf.self_ip)){
				SU_DEBUG_8 (("THE NAME OF THE DEVICE : %s\n",ifr.ifr_name));
				if(setsockopt (sock_fd, SOL_SOCKET, SO_BINDTODEVICE, &ifr,
						sizeof (ifr)) < 0 ){
					goto __sock_opened;
				}
				device_finded = 1;
				break;
			}
		}
		close (stmp);
		if( !device_finded){
			SU_DEBUG_1 (("ERROR: Network interface with self_ip %s not found",
					g_conf.self_ip));
			goto __sock_opened;
		}
	} else {
		SU_DEBUG_0((LOG_FNC_A("SHOULDN`T BE THERE!!")));
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
		SU_DEBUG_1(("svd_media_vinetic_open_rtp(): could not find free "
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
svd_media_vinetic_rtp_sock_rebinddev (svd_chan_t * const ctx)
{/*{{{*/
	int i;
	struct ifreq ifr;
DFS
#ifndef DONT_BIND_TO_DEVICE
	/* Set SO_BINDTODEVICE for right ip using */
	if(ctx->dial_status.dest_is_self == self_YES ||
			ctx->caller_router_is_self){
		/* use local interface */
		strcpy(ifr.ifr_name, "lo");
		SU_DEBUG_0 (("THE NAME OF THE DEVICE : %s\n",ifr.ifr_name));
		if(setsockopt (ctx->rtp_sfd, SOL_SOCKET, SO_BINDTODEVICE, &ifr,
				sizeof (ifr)) < 0 ){
			SU_DEBUG_1 ((LOG_FNC_A(strerror(errno))));
			goto __exit_fail;
		}
	} else if(ctx->dial_status.dest_is_self == self_NO ||
			(!ctx->caller_router_is_self)){
		/* use <self> set interface */
		/* get device name by self_ip */
		int stmp;
		int device_finded = 0;

		stmp = socket (PF_INET, SOCK_STREAM, 0);
		if(stmp == -1){
			SU_DEBUG_1 ((LOG_FNC_A(strerror(errno))));
			goto __exit_fail;
		}
		for(i=1;;i++){
			struct sockaddr_in *sin = (struct sockaddr_in *) &ifr.ifr_addr;
			char *ip;

			ifr.ifr_ifindex = i;
			if (ioctl (stmp, SIOCGIFNAME, &ifr) < 0){
				break;
			}
			if (ioctl (stmp, SIOCGIFADDR, &ifr) < 0){
				continue;
			}
			ip = inet_ntoa (sin->sin_addr);
			if( !strcmp(ip, g_conf.self_ip)){
				SU_DEBUG_0 (("THE NAME OF THE DEVICE : %s\n",ifr.ifr_name));
				if(setsockopt (ctx->rtp_sfd, SOL_SOCKET, SO_BINDTODEVICE, &ifr,
						sizeof (ifr)) < 0 ){
					SU_DEBUG_1 ((LOG_FNC_A(strerror(errno))));
					goto __exit_fail;
				}
				device_finded = 1;
				break;
			}
		}
		close (stmp);
		if( !device_finded){
			SU_DEBUG_1 (("ERROR: Network interface with self_ip %s not found",
					g_conf.self_ip));
			goto __exit_fail;
		}
	} else {
		SU_DEBUG_0((LOG_FNC_A("SHOULDN`T BE THERE!!")));
		goto __exit_fail;
	}
#endif
DFE
	return 0;
__exit_fail:
DFE
	return -1;
}/*}}}*/
