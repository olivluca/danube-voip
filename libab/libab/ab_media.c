#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>

#include "ab_internal_v22.h"

/**
 * \param[in,out] chan channel to operate on.
 * \retval	0 in success.
 * \retval	-1 if fail.
 * \remark
 *	Set coder type fo ALAW
 *	Set Jitter Buffer fixed
 *	Set WLEC type NE with NLP off
 */ 
int 
ab_chan_fax_pass_through_start( ab_chan_t * const chan ) 
{/*{{{*/
	IFX_TAPI_JB_CFG_t jbCfgData;
	IFX_TAPI_WLEC_CFG_t lecConf;

	int cfd = chan->rtp_fd;
	int err;
	int err_sum = 0;

	memset(&jbCfgData, 0, sizeof(jbCfgData));
	memset(&lecConf, 0, sizeof(lecConf));

	/* Configure coder for fax/modem communications */
//FIXME	err = ioctl(cfd, IFX_TAPI_ENC_TYPE_SET, IFX_TAPI_COD_TYPE_ALAW);
	if(err != IFX_SUCCESS){
//FIXME		ioctl (cfd, FIO_VINETIC_LASTERR, &ab_g_err_extra_value);
		ab_err_set(AB_ERR_UNKNOWN, "IFX_TAPI_ENC_TYPE_SET");
		err_sum++;
	}
	/* Reconfigure JB for fax/modem communications */
	jbCfgData.nJbType = IFX_TAPI_JB_TYPE_FIXED;
	jbCfgData.nPckAdpt = IFX_TAPI_JB_PKT_ADAPT_DATA;
	jbCfgData.nInitialSize = 120*8; /* 120 ms - optimum buffer size */
	err = ioctl(cfd, IFX_TAPI_JB_CFG_SET, &jbCfgData);
	if(err != IFX_SUCCESS){
//FIXME		ioctl (cfd, FIO_VINETIC_LASTERR, &ab_g_err_extra_value);
		ab_err_set(AB_ERR_UNKNOWN, "IFX_TAPI_JB_CFG_SET");
		err_sum++;
	}
	/* Reconfigure LEC for fax/modem communications */
	lecConf.nType = IFX_TAPI_WLEC_TYPE_NE;
	lecConf.bNlp = IFX_TAPI_LEC_NLP_OFF;
	err =ioctl(cfd,IFX_TAPI_WLEC_PHONE_CFG_SET,&lecConf);
	if(err != IFX_SUCCESS){
//FIXME		ioctl (cfd, FIO_VINETIC_LASTERR, &ab_g_err_extra_value);
		ab_err_set(AB_ERR_UNKNOWN, "IFX_TAPI_WLEC_PHONE_CFG_SET");
		err_sum++;
	}

	err = err_sum ? -1 : 0;
	return err;
}/*}}}*/

/**
 *	Tune RTP parameters on given channel
 *
 * \param[in,out] chan channel to operate on.
 * \param[in] cod codec type and parameters.
 * \param[in] fcod fax codec type and parameters.
 * \param[in] rtpp rtp parameters to set on given channel.
 * \retval	0 in success.
 * \retval	-1 if fail.
 * \remark
 * 	from \c fcod using just \c type and \c sdp_selected_payload_type.
 */ 
int 
ab_chan_media_rtp_tune( ab_chan_t * const chan, codec_t const * const cod,
		codec_t const * const fcod, rtp_session_prms_t const * const rtpp, int te_payload)
{/*{{{*/
	IFX_TAPI_PKT_RTP_PT_CFG_t rtpPTConf;
	IFX_TAPI_PKT_RTP_CFG_t rtpConf;
	IFX_TAPI_PKT_VOLUME_t codVolume;
	IFX_TAPI_ENC_CFG_t encCfg;
	IFX_TAPI_DEC_CFG_t decCfg;
	int vad_param;
	int hpf_param;
	int fcodt;
	int check_bitpack = 0;
	int err_summary = 0;
	int err;

	memset(&rtpPTConf, 0, sizeof(rtpPTConf));
	memset(&rtpConf, 0, sizeof(rtpConf));
	memset(&codVolume, 0, sizeof(codVolume));
	memset(&encCfg, 0, sizeof(encCfg));
	memset(&decCfg, 0, sizeof(decCfg));

	/* frame len {{{*/
	if       (cod->pkt_size == cod_pkt_size_2_5){
		encCfg.nFrameLen = IFX_TAPI_COD_LENGTH_2_5;
	} else if(cod->pkt_size == cod_pkt_size_5){
		encCfg.nFrameLen = IFX_TAPI_COD_LENGTH_5;
	} else if(cod->pkt_size == cod_pkt_size_5_5){
		encCfg.nFrameLen = IFX_TAPI_COD_LENGTH_5_5;
	} else if(cod->pkt_size == cod_pkt_size_10){
		encCfg.nFrameLen = IFX_TAPI_COD_LENGTH_10;
	} else if(cod->pkt_size == cod_pkt_size_11){
		encCfg.nFrameLen = IFX_TAPI_COD_LENGTH_11;
	} else if(cod->pkt_size == cod_pkt_size_20){
		encCfg.nFrameLen = IFX_TAPI_COD_LENGTH_20;
	} else if(cod->pkt_size == cod_pkt_size_30){
		encCfg.nFrameLen = IFX_TAPI_COD_LENGTH_30;
	} else if(cod->pkt_size == cod_pkt_size_40){
		encCfg.nFrameLen = IFX_TAPI_COD_LENGTH_40;
	} else if(cod->pkt_size == cod_pkt_size_50){
		encCfg.nFrameLen = IFX_TAPI_COD_LENGTH_50;
	} else if(cod->pkt_size == cod_pkt_size_60){
		encCfg.nFrameLen = IFX_TAPI_COD_LENGTH_60;
	}
	/*}}}*/
	/* PTypes table and [enc,dec]Cfg, correct frame len and set bitpack{{{*/
	if(cod->type == cod_type_ALAW){
		encCfg.nEncType = IFX_TAPI_COD_TYPE_ALAW;
		rtpPTConf.nPTup   [encCfg.nEncType] = 
		rtpPTConf.nPTdown [encCfg.nEncType] = 
				cod->sdp_selected_payload;
	} else if(cod->type == cod_type_G729){
		encCfg.nEncType = IFX_TAPI_COD_TYPE_G729;
		rtpPTConf.nPTup   [encCfg.nEncType] = 
		rtpPTConf.nPTdown [encCfg.nEncType] = 
				cod->sdp_selected_payload;
	} else if(cod->type == cod_type_G729E){
		encCfg.nEncType = IFX_TAPI_COD_TYPE_G729_E;
		rtpPTConf.nPTup   [encCfg.nEncType] = 
		rtpPTConf.nPTdown [encCfg.nEncType] = 
				cod->sdp_selected_payload;
	} else if(cod->type == cod_type_ILBC_133){
		encCfg.nFrameLen = IFX_TAPI_COD_LENGTH_30;
		encCfg.nEncType = IFX_TAPI_COD_TYPE_ILBC_133;
		rtpPTConf.nPTup   [IFX_TAPI_COD_TYPE_ILBC_152] = 
		rtpPTConf.nPTdown [IFX_TAPI_COD_TYPE_ILBC_152] = 
		rtpPTConf.nPTup   [IFX_TAPI_COD_TYPE_ILBC_133] = 
		rtpPTConf.nPTdown [IFX_TAPI_COD_TYPE_ILBC_133] = 
				cod->sdp_selected_payload;
	} else if(cod->type == cod_type_G723){
		if((encCfg.nFrameLen != IFX_TAPI_COD_LENGTH_30) &&
			(encCfg.nFrameLen != IFX_TAPI_COD_LENGTH_60)){
			/* can be set 30 or 60 */
			encCfg.nFrameLen = IFX_TAPI_COD_LENGTH_30;
		}
		encCfg.nEncType = IFX_TAPI_COD_TYPE_G723_53;
		rtpPTConf.nPTup   [IFX_TAPI_COD_TYPE_G723_63] = 
		rtpPTConf.nPTdown [IFX_TAPI_COD_TYPE_G723_63] = 
		rtpPTConf.nPTup   [IFX_TAPI_COD_TYPE_G723_53] = 
		rtpPTConf.nPTdown [IFX_TAPI_COD_TYPE_G723_53] = 
				cod->sdp_selected_payload;
	} else if(cod->type == cod_type_G726_16){
		encCfg.nEncType = IFX_TAPI_COD_TYPE_G726_16;
		rtpPTConf.nPTup   [IFX_TAPI_COD_TYPE_G726_16] = 
		rtpPTConf.nPTdown [IFX_TAPI_COD_TYPE_G726_16] = 
				cod->sdp_selected_payload;
		check_bitpack = 1;
	} else if(cod->type == cod_type_G726_24){
		encCfg.nEncType = IFX_TAPI_COD_TYPE_G726_24;
		rtpPTConf.nPTup   [IFX_TAPI_COD_TYPE_G726_24] = 
		rtpPTConf.nPTdown [IFX_TAPI_COD_TYPE_G726_24] = 
				cod->sdp_selected_payload;
		check_bitpack = 1;
	} else if(cod->type == cod_type_G726_32){
		encCfg.nEncType = IFX_TAPI_COD_TYPE_G726_32;
		rtpPTConf.nPTup   [IFX_TAPI_COD_TYPE_G726_32] = 
		rtpPTConf.nPTdown [IFX_TAPI_COD_TYPE_G726_32] = 
				cod->sdp_selected_payload;
		check_bitpack = 1;
	} else if(cod->type == cod_type_G726_40){
		encCfg.nEncType = IFX_TAPI_COD_TYPE_G726_40;
		rtpPTConf.nPTup   [IFX_TAPI_COD_TYPE_G726_40] = 
		rtpPTConf.nPTdown [IFX_TAPI_COD_TYPE_G726_40] = 
				cod->sdp_selected_payload;
		check_bitpack = 1;
	} else if(cod->type == cod_type_NONE){
		err_summary++;
		ab_err_set(AB_ERR_BAD_PARAM, "codec type not set");
		goto __exit;
	}

	if(check_bitpack){
		if(cod->bpack == bitpack_RTP){
			encCfg.AAL2BitPack = 
			decCfg.AAL2BitPack = IFX_TAPI_COD_RTP_BITPACK;
		} else {
			encCfg.AAL2BitPack = 
			decCfg.AAL2BitPack = IFX_TAPI_COD_AAL2_BITPACK;
		}
	} else {
		encCfg.AAL2BitPack = 
		decCfg.AAL2BitPack = IFX_TAPI_COD_RTP_BITPACK;
	}
	/*}}}*/
	/* FAX {{{*/
	fcodt = IFX_TAPI_COD_TYPE_ALAW;
	/* tuning fax transmission codec */
	rtpPTConf.nPTup [fcodt] = rtpPTConf.nPTdown [fcodt] = 
			fcod->sdp_selected_payload;
	/*}}}*/
	rtpConf.nSeqNr = 0;
	rtpConf.nSsrc = 0;
	/* set out-of-band (RFC 2833 packet) configuratoin {{{*/
	if (te_payload<0) {
		rtpConf.nEvents = IFX_TAPI_PKT_EV_OOB_NO;
		rtpConf.nEventPT = 0x62;
		rtpConf.nEventPlayPT = 0x62;
		rtpConf.nPlayEvents = IFX_TAPI_PKT_EV_OOBPLAY_MUTE;
	} else {
		rtpConf.nEvents = IFX_TAPI_PKT_EV_OOB_ONLY;
		rtpConf.nEventPT = te_payload;
		rtpConf.nEventPlayPT = te_payload;
		rtpConf.nPlayEvents = IFX_TAPI_PKT_EV_OOBPLAY_PLAY;
	}
	/*}}}*/
	/* Configure encoder and decoder gains {{{*/
	codVolume.nEnc = rtpp->enc_dB;
	codVolume.nDec = rtpp->dec_dB;
	/*}}}*/
	/* Set the VAD configuration {{{*/
	switch(rtpp->VAD_cfg){
	case vad_cfg_ON:
		vad_param = IFX_TAPI_ENC_VAD_ON;
		break;
	case vad_cfg_OFF:
		vad_param = IFX_TAPI_ENC_VAD_NOVAD;
		break;
	case vad_cfg_G711:
		vad_param = IFX_TAPI_ENC_VAD_G711;
		break;
	case vad_cfg_CNG_only:
		vad_param = IFX_TAPI_ENC_VAD_CNG_ONLY;
		break;
	case vad_cfg_SC_only:
		vad_param = IFX_TAPI_ENC_VAD_SC_ONLY;
		break;
	}
	/*}}}*/
	/* Configure high-pass filter {{{ */
	if(rtpp->HPF_is_ON){
		hpf_param = IFX_TRUE;
	} else {
		hpf_param = IFX_FALSE;
	}
	/*}}}*/

#if 0/*{{{*/
fprintf(stderr,"[e%d/d%d] ",codVolume.nEnc,codVolume.nDec);
switch(rtpp->VAD_cfg){
	case vad_cfg_ON:
fprintf(stderr,"vad on");
		break;
	case vad_cfg_OFF:
fprintf(stderr,"vad off");
		break;
	case vad_cfg_G711:
fprintf(stderr,"vad g711");
		break;
	case vad_cfg_CNG_only:
fprintf(stderr,"vad cng_only");
		break;
	case vad_cfg_SC_only:
fprintf(stderr,"vad sc_only");
		break;
	}
if(rtpp->HPF_is_ON){
fprintf(stderr," hpf on\n");
	} else {
fprintf(stderr," hpf off\n");
	}
#endif/*}}}*/
	/*********** ioctl seq {{{*/

	/* Set the coder payload table */ 
	err = 0;
	err = ioctl(chan->rtp_fd, IFX_TAPI_PKT_RTP_PT_CFG_SET, &rtpPTConf);
	if(err){
		err_summary++;
		ab_err_set(AB_ERR_UNKNOWN, "media rtp PT tune ioctl error");
	}

	/* Set the rtp configuration (OOB, pkt params, etc.) */ 
	err = 0;
	err = ioctl(chan->rtp_fd, IFX_TAPI_PKT_RTP_CFG_SET, &rtpConf);
	if(err){
		err_summary++;
		ab_err_set(AB_ERR_UNKNOWN, "media rtp tune ioctl error");
	}

	/* Set the encoder */ 
	err = 0;
	err = ioctl(chan->rtp_fd, IFX_TAPI_ENC_CFG_SET, &encCfg);
	if(err){
		ab_err_set(AB_ERR_UNKNOWN, "encoder set ioctl error");
		err_summary++;
	}
	/* Set the decoder */ 
	err = 0;
	err = ioctl(chan->rtp_fd, IFX_TAPI_DEC_CFG_SET, &decCfg);
	if(err){
		ab_err_set(AB_ERR_UNKNOWN, "decoder set ioctl error");
		err_summary++;
	}
	
	/* Set the VAD configuration */
	err = 0;
	err = ioctl(chan->rtp_fd, IFX_TAPI_ENC_VAD_CFG_SET, vad_param);
	if(err){
		ab_err_set(AB_ERR_UNKNOWN, "vad set ioctl error");
		err_summary++;
	}

	/* Configure encoder and decoder gains */
	err = 0;
	err = ioctl(chan->rtp_fd, IFX_TAPI_COD_VOLUME_SET, &codVolume);
	if(err){
		ab_err_set(AB_ERR_UNKNOWN, "volume set ioctl error");
		err_summary++;
	}

	/* Configure high-pass filter */
	err = 0;
	err = ioctl(chan->rtp_fd, IFX_TAPI_COD_DEC_HP_SET, hpf_param);
	if(err){
		ab_err_set(AB_ERR_UNKNOWN, "high pass set ioctl error");
		err_summary++;
	}
	/*}}}*/
__exit:
	if(err_summary){
		err = -1;
	}
	return err;
}/*}}}*/

/**
 *	Tune JB parameters on given channel
 *
 * \param[in,out] chan channel to operate on.
 * \param[in] jbp jitter buffer parameters to set on given channel.
 * \retval	0 in success.
 * \retval	-1 if fail.
 */ 
int 
ab_chan_media_jb_tune( ab_chan_t * const chan, jb_prms_t const * const jbp)
{/*{{{*/
	IFX_TAPI_JB_CFG_t jbCfg;
	int err;

	memset(&jbCfg, 0, sizeof(jbCfg));

	if       (jbp->jb_type == jb_type_FIXED){
		jbCfg.nJbType = IFX_TAPI_JB_TYPE_FIXED;
	} else if(jbp->jb_type == jb_type_ADAPTIVE){
		jbCfg.nJbType = IFX_TAPI_JB_TYPE_ADAPTIVE;
	}

	jbCfg.nPckAdpt = IFX_TAPI_JB_PKT_ADAPT_VOICE;

	if       (jbp->jb_loc_adpt == jb_loc_adpt_OFF){
		jbCfg.nLocalAdpt = IFX_TAPI_JB_LOCAL_ADAPT_OFF;
	} else if(jbp->jb_loc_adpt == jb_loc_adpt_ON){
		jbCfg.nLocalAdpt = IFX_TAPI_JB_LOCAL_ADAPT_ON;
//	} else if(jbp->jb_loc_adpt == jb_loc_adpt_SI){
//		jbCfg.nLocalAdpt = IFX_TAPI_JB_LOCAL_ADAPT_SI_ON;
	}
	jbCfg.nScaling = jbp->jb_scaling;
	jbCfg.nInitialSize = jbp->jb_init_sz;
	jbCfg.nMinSize = jbp->jb_min_sz;
	jbCfg.nMaxSize = jbp->jb_max_sz;

	/* Configure jitter buffer */
	err = ioctl(chan->rtp_fd, IFX_TAPI_JB_CFG_SET, &jbCfg);
	if(err){
		ab_err_set(AB_ERR_UNKNOWN, "jb cfg set ioctl error");
	}

	return err;
}/*}}}*/

/**
 *	Tune WLEC (Window-based Line Echo Canceller) parameters on given channel
 *
 * \param[in,out] chan channel to operate on.
 * \param[in] wp wlec parameters.
 * \retval	0 in success.
 * \retval	-1 if fail.
 */ 
int 
ab_chan_media_wlec_tune( ab_chan_t * const chan, wlec_t const * const wp )
{/*{{{*/
	IFX_TAPI_WLEC_CFG_t lecConf;
	int err;

	memset(&lecConf, 0, sizeof(lecConf));

	/* WLEC mode */
	if        (wp->mode == wlec_mode_OFF ||
			   wp->mode == wlec_mode_UNDEF){
		lecConf.nType = IFX_TAPI_WLEC_TYPE_OFF;
	} else if (wp->mode == wlec_mode_NE){
		lecConf.nType = IFX_TAPI_WLEC_TYPE_NE;
	} else if (wp->mode == wlec_mode_NFE){
		lecConf.nType = IFX_TAPI_WLEC_TYPE_NFE;
	}

	/* NLP */
	if        (wp->nlp == wlec_nlp_ON){
		lecConf.bNlp = IFX_TAPI_WLEC_NLP_ON;
	} else if (wp->nlp == wlec_nlp_OFF){
		lecConf.bNlp = IFX_TAPI_WLEC_NLP_OFF;
	}

	/* Windows */
	if        (wp->ne_nb == wlec_window_size_4){
		lecConf.nNBNEwindow = IFX_TAPI_WLEN_WSIZE_4;
	} else if (wp->ne_nb == wlec_window_size_6){
		lecConf.nNBNEwindow = IFX_TAPI_WLEN_WSIZE_6;
	} else if (wp->ne_nb == wlec_window_size_8){
		lecConf.nNBNEwindow = IFX_TAPI_WLEN_WSIZE_8;
	} else if (wp->ne_nb == wlec_window_size_16){
		lecConf.nNBNEwindow = IFX_TAPI_WLEN_WSIZE_16;
	}

	if        (wp->fe_nb == wlec_window_size_4){
		lecConf.nNBFEwindow = IFX_TAPI_WLEN_WSIZE_4;
	} else if (wp->fe_nb == wlec_window_size_6){
		lecConf.nNBFEwindow = IFX_TAPI_WLEN_WSIZE_6;
	} else if (wp->fe_nb == wlec_window_size_8){
		lecConf.nNBFEwindow = IFX_TAPI_WLEN_WSIZE_8;
	} else if (wp->fe_nb == wlec_window_size_16){
		lecConf.nNBFEwindow = IFX_TAPI_WLEN_WSIZE_16;
	}

	if        (wp->ne_wb == wlec_window_size_4){
		lecConf.nWBNEwindow = IFX_TAPI_WLEN_WSIZE_4;
	} else if (wp->ne_wb == wlec_window_size_6){
		lecConf.nWBNEwindow = IFX_TAPI_WLEN_WSIZE_6;
	} else if (wp->ne_wb == wlec_window_size_8){
		lecConf.nWBNEwindow = IFX_TAPI_WLEN_WSIZE_8;
	} else if (wp->ne_wb == wlec_window_size_16){
		lecConf.nWBNEwindow = IFX_TAPI_WLEN_WSIZE_16;
	}

	/* Set configuration */
	err = ioctl(chan->rtp_fd, IFX_TAPI_WLEC_PHONE_CFG_SET, &lecConf);

	if (err){
		ab_err_set(AB_ERR_UNKNOWN, "wlec_phone_cfg_set ioctl error");
		err = -1;
	}
	return err;
}/*}}}*/

void
jb_stat_avg_count_and_wirte(ab_chan_t * const chan, 
		IFX_TAPI_JB_STATISTICS_t * const jb_stat)
{/*{{{*/
	/* np1 :: n+1 */
	double n = chan->statistics.con_cnt;
	double pks_avg_n = chan->statistics.pcks_avg;
	double pks_np1 = jb_stat->nPackets;
	double denom;
	double numer[4];

	chan->statistics.pcks_avg = n/(n+1)*pks_avg_n + pks_np1/(n+1);

	numer[0] = chan->statistics.jb_stat.nInvalid;
	numer[1] = chan->statistics.jb_stat.nLate;
	numer[2] = chan->statistics.jb_stat.nEarly;
	numer[3] = chan->statistics.jb_stat.nResync;
	if(n){
		double coef = pks_avg_n/100;
		numer[0] = coef * chan->statistics.invalid_pc + (double)jb_stat->nInvalid/n;
		numer[1] = coef * chan->statistics.late_pc + (double)jb_stat->nLate/n;
		numer[2] = coef * chan->statistics.early_pc + (double)jb_stat->nEarly/n;
		numer[3] = coef * chan->statistics.resync_pc + (double)jb_stat->nResync/n;
	} else {
		denom = pks_np1;
	}
	chan->statistics.invalid_pc = numer[0]/denom * 100;
	chan->statistics.late_pc =    numer[1]/denom * 100;
	chan->statistics.early_pc =   numer[2]/denom * 100;
	chan->statistics.resync_pc =  numer[3]/denom * 100;
}/*}}}*/

static int
jb_stat_get(ab_chan_t * const chan, IFX_TAPI_JB_STATISTICS_t * const jb_stat)
{/*{{{*/
	int err;

	err = ioctl (chan->rtp_fd, IFX_TAPI_JB_STATISTICS_GET, jb_stat);
	if(err == IFX_ERROR){
		ab_err_set (AB_ERR_UNKNOWN, "Jitter Buffer statistics get ioctl error");
		goto __exit_fail;
	}
	return 0;
__exit_fail:
	return -1;
}/*}}}*/

void
jb_stat_write(ab_chan_t * const chan, IFX_TAPI_JB_STATISTICS_t * const jb_stat)
{/*{{{*/
	/* rewrite previous JB statistics to the current */
	if(jb_stat->nType == 1){
		chan->statistics.jb_stat.nType = jb_type_FIXED;
	} else if(jb_stat->nType == 2){
		chan->statistics.jb_stat.nType = jb_type_ADAPTIVE;
	}
	chan->statistics.jb_stat.nBufSize = jb_stat->nBufSize;
	chan->statistics.jb_stat.nMaxBufSize = jb_stat->nMaxBufSize;
	chan->statistics.jb_stat.nMinBufSize = jb_stat->nMinBufSize;
	chan->statistics.jb_stat.nPODelay = jb_stat->nPODelay;
	chan->statistics.jb_stat.nMaxPODelay = jb_stat->nMaxPODelay;
	chan->statistics.jb_stat.nMinPODelay = jb_stat->nMinPODelay;
	chan->statistics.jb_stat.nPackets = jb_stat->nPackets;
	chan->statistics.jb_stat.nInvalid = jb_stat->nInvalid;
	chan->statistics.jb_stat.nLate = jb_stat->nLate;
	chan->statistics.jb_stat.nEarly = jb_stat->nEarly;
	chan->statistics.jb_stat.nResync = jb_stat->nResync;
	chan->statistics.jb_stat.nIsUnderflow = jb_stat->nIsUnderflow;
	chan->statistics.jb_stat.nIsNoUnderflow = jb_stat->nIsNoUnderflow;
	chan->statistics.jb_stat.nIsIncrement = jb_stat->nIsIncrement;
	chan->statistics.jb_stat.nSkDecrement = jb_stat->nSkDecrement;
	chan->statistics.jb_stat.nDsDecrement = jb_stat->nDsDecrement;
	chan->statistics.jb_stat.nDsOverflow = jb_stat->nDsOverflow;
	chan->statistics.jb_stat.nSid = jb_stat->nSid;
	chan->statistics.jb_stat.nRecBytesH = jb_stat->nRecBytesH;
	chan->statistics.jb_stat.nRecBytesL = jb_stat->nRecBytesL;
}/*}}}*/

/**
	Switch media on / off on the given channel
\param chan - channel to operate on it
\param switch_up - on (1) or off (0) encoding and decoding
\return 
	0 in success case and other value otherwise
\remark
	returns the ioctl error value and writes error message
*/
int 
ab_chan_media_switch( ab_chan_t * const chan, unsigned char const switch_up )
{/*{{{*/
	int err1= 0;
	int err2= 0;

	if( chan->statistics.is_up == switch_up ){
		/* nothing to do */
		goto __exit_success;
	}

	if (switch_up){
		err1 = ioctl(chan->rtp_fd, IFX_TAPI_ENC_START, 0);
		err2 = ioctl(chan->rtp_fd, IFX_TAPI_DEC_START, 0);
		if(!(err1|err2)){
			chan->statistics.is_up = 1;
		}
	} else {
		int err;
		IFX_TAPI_JB_STATISTICS_t jb_stat;

		/* before down - should read jb/rtcp statistics and
		 * count average jb statistics */
		memset(&jb_stat, 0, sizeof(jb_stat));
		err = jb_stat_get(chan, &jb_stat);
		err1 = ioctl(chan->rtp_fd, IFX_TAPI_ENC_STOP, 0);
		err2 = ioctl(chan->rtp_fd, IFX_TAPI_DEC_STOP, 0);
		if(!(err1|err2)){
			if( !err){
				jb_stat_avg_count_and_wirte(chan, &jb_stat);
				jb_stat_write(chan, &jb_stat);
				ab_chan_media_rtcp_refresh (chan);
			}
			chan->statistics.is_up = 0;
			chan->statistics.con_cnt++;
		}
	}
	if(err1|err2){
		ab_err_set(AB_ERR_UNKNOWN, "enc/dec start/stop ioctl error");
	}

	return (err1|err2);
__exit_success:
	return 0;
}/*}}}*/

/**
	Hold on / off encoding on the given channel
\param[in] chan - channel to operate on it
\param[in] hold - hold (1) or unhold (0) encoding
\return 
	0 in success case and other value otherwise
\remark
	returns the ioctl error value and writes error message
*/
int 
ab_chan_media_enc_hold( ab_chan_t * const chan, unsigned char const hold )
{/*{{{*/
	int err = 0;
	IFX_operation_t op;

	op = (hold) ? IFX_ENABLE : IFX_DISABLE;

	err = ioctl(chan->rtp_fd, IFX_TAPI_ENC_HOLD, op);
	if(err){
		ab_err_set(AB_ERR_UNKNOWN, "encoder hold/unhold ioctl error");
	}
	return err;
}/*}}}*/

/**
	Set encoding and decoding coder volume on the given channel
\param[in] chan - channel to operate on it
\param[in] enc_gain - encoding gain = [-24;24]
\param[in] dec_gain - decoding gain = [-24;24]
\return 
	0 in success case and other value otherwise
\remark
	returns the ioctl error value and writes error message
*/
int 
ab_chan_media_volume( ab_chan_t * const chan, 
		int const enc_gain, int const dec_gain )
{/*{{{*/
	int err = 0;
	IFX_TAPI_PKT_VOLUME_t codVolume;

	codVolume.nDec = dec_gain;
	codVolume.nEnc = enc_gain;

	err = ioctl(chan->rtp_fd, IFX_TAPI_COD_VOLUME_SET, &codVolume);
	if(err){
		ab_err_set(AB_ERR_UNKNOWN, "encoder mute/unmute ioctl error");
	}
	return err;
}/*}}}*/

/**
	Refresh Jitter Buffer statistics in chan->statistics if it is necessary
\param[in,out] chan - channel to operate on it
\return 
	0 in success case and other value otherwise
*/
int 
ab_chan_media_jb_refresh( ab_chan_t * const chan )
{/*{{{*/
	int err;
	IFX_TAPI_JB_STATISTICS_t jb_stat;

	if( !chan->statistics.is_up){
		/* nothing to do */
		goto __exit_success;
	}

	memset(&jb_stat, 0, sizeof(jb_stat));
	err = jb_stat_get(chan, &jb_stat);
	if(err){
		goto __exit_fail;
	}
	jb_stat_write(chan, &jb_stat);

__exit_success:
	return 0;
__exit_fail:
	return -1;
}/*}}}*/

/**
	Refresh RTCP statistics in chan->statistics if it is necessary
\param[in,out] chan - channel to operate on it
\return 
	0 in success case and other value otherwise
*/
int 
ab_chan_media_rtcp_refresh( ab_chan_t * const chan )
{/*{{{*/
	int err;
	IFX_TAPI_PKT_RTCP_STATISTICS_t rtcp_stat;

	if( !chan->statistics.is_up){
		/* nothing to do */
		goto __exit_success;
	}

	memset(&rtcp_stat, 0, sizeof(rtcp_stat));
	err = ioctl (chan->rtp_fd, IFX_TAPI_PKT_RTCP_STATISTICS_GET, &rtcp_stat);
	if(err == IFX_ERROR){
		ab_err_set (AB_ERR_UNKNOWN, "RTCP statistics get ioctl error");
		goto __exit_fail;
	}

	/* rewrite previous RTCP statistics to the current */
	chan->statistics.rtcp_stat.ssrc = rtcp_stat.ssrc;
	chan->statistics.rtcp_stat.rtp_ts = rtcp_stat.rtp_ts;
	chan->statistics.rtcp_stat.psent = rtcp_stat.psent;
	chan->statistics.rtcp_stat.osent = rtcp_stat.osent;
	chan->statistics.rtcp_stat.rssrc = rtcp_stat.rssrc;
	chan->statistics.rtcp_stat.fraction = rtcp_stat.fraction;
	chan->statistics.rtcp_stat.lost = ((unsigned long)rtcp_stat.lost) & 0x00FFFFFF;
	chan->statistics.rtcp_stat.last_seq = rtcp_stat.last_seq;
	chan->statistics.rtcp_stat.jitter = rtcp_stat.jitter;
	
__exit_success:
	return 0;
__exit_fail:
	return -1;
}/*}}}*/
