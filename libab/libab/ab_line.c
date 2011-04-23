#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>

#include "ab_internal_v22.h"

#define AB_INTER_DIGIT_TIME_DF 100
#define AB_DIGIT_PLAY_TIME_DF  100

#define CHAN_VOLUME_MIN_GAIN (-24)
#define CHAN_VOLUME_MAX_GAIN 24

/**
	Run the appropriate ioctl command and set the error if necessary. 
\param [in] chan - channel to write error message and run ioctl
\param [in] request - ioctl macro
\param [in] data - ioctl data
\param [in] err_msg - error message if ioctl fails
\return 
	ioctl result
\remark
	ioctl makes on chan->rtp_fd file descriptor
*/
static int 
err_set_ioctl (ab_chan_t * const chan, int const request, int const data,
		char const * const err_msg )
{/*{{{*/
	int err = 0;
	err = ioctl(chan->rtp_fd, request, data);
	if (err){
		ab_err_set(AB_ERR_UNKNOWN, (char const *)err_msg); 
	}
	return err;
}/*}}}*/

/**
	Plays the given tone 
\param [in] chan - channel to play tone
\param [in] tone - tone to play 
\return 
	ioctl result
\remark
	it just play the tone without any test of actual state.
*/
static int 
ab_FXS_line_just_play_it (ab_chan_t * const chan, enum ab_chan_tone_e const tone)
{/*{{{*/
	int err = 0;
	switch(tone){
		case ab_chan_tone_MUTE: {
			err = err_set_ioctl( chan, IFX_TAPI_TONE_LOCAL_PLAY, 0,
					"stop playing tone (ioctl)");
			if( !err){
				chan->status.tone = ab_chan_tone_MUTE;
			}
			break;
		}
		case ab_chan_tone_DIAL: {
			err = err_set_ioctl(chan, IFX_TAPI_TONE_DIALTONE_PLAY, 0,
					"playing dialtone (ioctl)");
			if( !err){
				chan->status.tone = ab_chan_tone_DIAL;
			}
			break;
		}
		case ab_chan_tone_BUSY: {
			err = err_set_ioctl(chan, IFX_TAPI_TONE_BUSY_PLAY, 0,
					"playing busy (ioctl)");
			if( !err){
				chan->status.tone = ab_chan_tone_BUSY;
			}
			break;
		}
		case ab_chan_tone_RINGBACK: {
			err = err_set_ioctl(chan, IFX_TAPI_TONE_RINGBACK_PLAY, 0,
					"playing ringback (ioctl)");
			if( !err){
				chan->status.tone = ab_chan_tone_RINGBACK;
			}
			break;
		}
	}
	return err;
}/*}}}*/

/**
	Ring or mute on given channel
\param chan - channel to ring
\param ring - can be RINGING or MUTE
\return 
	ioctl result
\remark
	If the given ring state is actual - there is nothing happens
*/
int 
ab_FXS_line_ring (ab_chan_t * const chan, enum ab_chan_ring_e ring, char * number, char * name)
{/*{{{*/
	int err = 0;
	if (chan->status.ring != ring){
		if( ring == ab_chan_ring_RINGING ) {
			IFX_TAPI_CID_MSG_t cidType1;
			IFX_TAPI_CID_MSG_ELEMENT_t message[2];
			memset(&cidType1, 0, sizeof(cidType1));
			memset(&message, 0, sizeof(message));
			int i=0;
			if (number!=NULL && number[0]!=0) {
				message[i].string.elementType = IFX_TAPI_CID_ST_CLI;
				message[i].string.len = strlen(number);
				strncpy(message[i].string.element, number, sizeof(message[0].string.element));
				i++;
			}
			if (name!=NULL && name[0]!=0) {
				message[i].string.elementType = IFX_TAPI_CID_ST_NAME;
				message[i].string.len = strlen(name);
				strncpy(message[i].string.element, name, sizeof(message[0].string.element));
				i++;
			}
			if (i==0) {
				/* neither caller id or name, normal ring */
				err = err_set_ioctl(
					chan, IFX_TAPI_RING_START, 0,
						  "start ringing (ioctl)");
			} else {
				cidType1.txMode = IFX_TAPI_CID_HM_ONHOOK;
				cidType1.messageType = IFX_TAPI_CID_MT_CSUP;
				cidType1.nMsgElements = i;
				cidType1.message = message;
				err = ioctl(chan->rtp_fd, IFX_TAPI_CID_TX_SEQ_START, &cidType1);
				if (err){
					ab_err_set(AB_ERR_UNKNOWN, "caller id and start ringing (ioctl)"); 
				}
			}
			if (!err){
				chan->status.ring = ab_chan_ring_RINGING;
			}
		} else {
			err = err_set_ioctl(
				chan, IFX_TAPI_RING_STOP, 0,
						"stop ringing (ioctl)");
			if (!err){
				chan->status.ring = ab_chan_ring_MUTE;
			}
		}
	}
	return err;
}/*}}}*/

/**
	Play the given tone
\param chan - channel to play tone
\param tone - tone to play 
\return 
	ioctl result
\remark
	it test the state and do not do the unnecessary actions
*/
int 
ab_FXS_line_tone (ab_chan_t * const chan, enum ab_chan_tone_e tone)
{/*{{{*/
	int err = 0;
	if (chan->status.tone != tone){
		/* Status is not actual - should do smth */
		if(chan->status.tone == ab_chan_tone_MUTE){
			/* we should play smth */
			err = ab_FXS_line_just_play_it (chan, tone);
			if (err){
				goto __exit;
			} 
		} else {
			/* Something playing, but not that what we need */
			err = ab_FXS_line_just_play_it (chan, ab_chan_tone_MUTE);
			if (err){
				goto __exit;
			} else if (tone != ab_chan_tone_MUTE){
				/* we are don`t need MUTE */
				err = ab_FXS_line_just_play_it (chan, tone);
				if (err){
					goto __exit;
				} 
			}
		}
	}
__exit:
	return err;
}/*}}}*/

/**
	Change current linefeed to given
\param chan - channel to operate on it
\param feed - linefeed to set
\return 
	ioctl result
\remark
	it test the state and do not do the unnecessary actions
	if linefeed is disabled, and we want to set it to active, it will set
			it to standby first
*/
int 
ab_FXS_line_feed (ab_chan_t * const chan, enum ab_chan_linefeed_e feed) 
{/*{{{*/
	int err = 0;

	if (chan->status.linefeed != feed){
		char const * err_msg;
		int lf_to_set;
		switch (feed){
			case ab_chan_linefeed_DISABLED: {
				/* From any state */
				err_msg= "Setting linefeed to disabled (ioctl)";
				lf_to_set = IFX_TAPI_LINE_FEED_DISABLED;
				break;	
			}
			case ab_chan_linefeed_STANDBY: {
				/* From any state */
				err_msg = "Setting linefeed to standby (ioctl)";
				lf_to_set = IFX_TAPI_LINE_FEED_STANDBY;
				break;	
			}
			case ab_chan_linefeed_ACTIVE: {
				/* linefeed_STANDBY should be set before ACTIVE */
				if( chan->status.linefeed == ab_chan_linefeed_DISABLED){
					err = err_set_ioctl (chan, IFX_TAPI_LINE_FEED_SET, 
						IFX_TAPI_LINE_FEED_STANDBY, 
						"Setting linefeed to standby before set "
						"it to active (ioctl)"); 
					if( err ){
						goto __exit;
					} else {
						chan->status.linefeed = ab_chan_linefeed_STANDBY;
					}
				}
				err_msg = "Setting linefeed to active (ioctl)";
				lf_to_set = IFX_TAPI_LINE_FEED_ACTIVE;
				break;	
			}
		}
		err = err_set_ioctl (chan, IFX_TAPI_LINE_FEED_SET, lf_to_set, err_msg);
		if ( !err){
			chan->status.linefeed = feed;
		} 
	}
__exit:
	return err;
}/*}}}*/

/**
	Do onhook or offhook
\param chan - channel to operate on it
\param hook - desired hookstate
\return 
	ioctl result
\remark
	it test the state and do not do the unnecessary actions
\todo
	we can also test hook by ioctl there
*/
int 
ab_FXO_line_hook (ab_chan_t * const chan, enum ab_chan_hook_e hook)
{/*{{{*/
	int err = 0;

	if (chan->status.hook != hook){
		char const * err_msg;
		int h_to_set;
		switch (hook) {
			case ab_chan_hook_OFFHOOK : {
				h_to_set = IFX_TAPI_FXO_HOOK_OFFHOOK;
				err_msg = "Try to offhook (ioctl)";
				break;
			}
			case ab_chan_hook_ONHOOK: {
				h_to_set = IFX_TAPI_FXO_HOOK_ONHOOK;
				err_msg = "Try to onhook (ioctl)";
				break;
			}
		}
		err = err_set_ioctl (chan, IFX_TAPI_FXO_HOOK_SET, h_to_set, err_msg);
		if( !err){
			chan->status.hook = hook;
		} 
	}
	return err;
}/*}}}*/

/**
	Dial the given sequence of numbers
\param chan - channel dial numbers in it
\param data_length - sequence length
\param data - sequence of numbers
\param nInterDigitTime - interval between dialed digits
\param nDigitPlayTime - interval to play digits
\param pulseMode - if set - dial in pulse mode - not tone
\return 
	ioctl result
\remark
	the digits can be: '0' - '9', '*','#','A','B','C' and 'D'
	if nInterDigitTime or nDigitPlayTime set to 0, it will be set to 
			default value (100 ms). Maximum value is 127 ms.
*/
int 
ab_FXO_line_digit (ab_chan_t * const chan, char const data_length, 
		char const * const data, char const nInterDigitTime,
		char const nDigitPlayTime, char const pulseMode)
{/*{{{*/
	IFX_TAPI_FXO_DIAL_CFG_t dialCfg;
	IFX_TAPI_FXO_DIAL_t 	dialDigits;
	int err = 0;

	memset(&dialCfg, 0, sizeof(dialCfg));
	memset(&dialDigits, 0, sizeof(dialDigits));

	/* Configure dial timing */
	dialCfg.nInterDigitTime = AB_INTER_DIGIT_TIME_DF;
	dialCfg.nDigitPlayTime = AB_DIGIT_PLAY_TIME_DF;
	if ( nInterDigitTime ) {
		dialCfg.nInterDigitTime = nInterDigitTime;
	}
	if( nInterDigitTime ) {
		dialCfg.nDigitPlayTime = nDigitPlayTime;
	}

//FIXME	dialCfg.pulseMode = pulseMode;  No tenemos FXO

	err = err_set_ioctl( chan, IFX_TAPI_FXO_DIAL_CFG_SET, (int)&dialCfg, 
			"Try to configure dial params (ioctl)");
	if( err ){
		goto __exit;
	} 

	/* Dial sequence 
	 * it needs for some time but returns immediately
	 * */
	dialDigits.nDigits = data_length;
	memcpy(dialDigits.data, data, dialDigits.nDigits);

	err = err_set_ioctl( chan, IFX_TAPI_FXO_DIAL_START, (int)(&dialDigits), 
			"Try to dial sequence (ioctl)");
__exit:
	return err;
}/*}}}*/

/**
 * Play to rtp flow some event in-band or out-of-band.
 *
 * \param[in] chan - channel to play in.
 * \param[in] tone - tone to play play.
 *
 * \retval 0 - success
 * \retval -1 - fail
 *
 * \remark
 *	tones can be '0' - '9', '*', '#', 'A' - 'D', or 'b'/'r'/'d' for
 *	busy ringing and dialtones. Or 'f' / 'F' for CNG and CED fax signals and
 *	'm' for muting previously playing tone.
 */
int 
ab_FXS_netlo_play (ab_chan_t * const chan, char tone, char local)
{/*{{{*/
	int err;
	int idx;
	if(tone >= '1' && tone <= '9'){
		idx = tone - '0';
	} else if(tone == '0'){
		idx = 11;
	} else if(tone == '*'){
		idx = 10;
	} else if(tone == '#'){
		idx = 12;
	} else if(tone == 'A'){
		idx = 28;
	} else if(tone == 'B'){
		idx = 29;
	} else if(tone == 'C'){
		idx = 30;
	} else if(tone == 'D'){
		idx = 31;
	} else if(tone == 'd'){
		idx = 25;
	} else if(tone == 'r'){
		idx = 26;
	} else if(tone == 'b'){
		idx = 27;
	} else if(tone == 'f'){
		idx = 17;
	} else if(tone == 'F'){
		idx = 22;
	} else if(tone == 'm'){
		idx = 0;
	} 
	if(local){
		err = err_set_ioctl (chan, IFX_TAPI_TONE_LOCAL_PLAY, idx, 
				"Try to play network tone (ioctl)");
	} else {
		err = err_set_ioctl (chan, IFX_TAPI_TONE_NET_PLAY, idx, 
				"Try to play network tone (ioctl)");
	}
	if( err){
		goto __exit_fail;
	} 
	return 0;
__exit_fail:
	return -1;
}/*}}}*/

/**
	Set caller id standard
\param[in,out] chan - channel to operate on it
\param[in]     std - caller id standard	
\return 
	0 in success case and other value otherwise
*/
int 
ab_chan_cid_standard( ab_chan_t * const chan, const cid_std_t std )
{/*{{{*/
	int err;
	IFX_TAPI_CID_CFG_t cidConf;

	memset(&cidConf, 0, sizeof(cidConf));
	switch (std) {
	  case cid_TELCORDIA:
	    cidConf.nStandard = IFX_TAPI_CID_STD_TELCORDIA;
	    break;
	  case cid_ETSI_FSK:
	    cidConf.nStandard = IFX_TAPI_CID_STD_ETSI_FSK;
	    break;
	  case cid_ETSI_DTMF:
	    cidConf.nStandard = IFX_TAPI_CID_STD_ETSI_DTMF;
	    break;
	  case cid_SIN:
	    cidConf.nStandard = IFX_TAPI_CID_STD_SIN;
	    break;
	  case cid_NTT:
	    cidConf.nStandard = IFX_TAPI_CID_STD_NTT;
	    break;
	  case cid_KPN_DTMF:
	    cidConf.nStandard = IFX_TAPI_CID_STD_KPN_DTMF;
	    break;
	  case cid_KPN_DTMF_FSK:
	    cidConf.nStandard = IFX_TAPI_CID_STD_KPN_DTMF_FSK;
	    break;
	  default:
	    goto __exit_fail;
	}

	err = ioctl (chan->rtp_fd, IFX_TAPI_CID_CFG_SET, &cidConf);
	if(err == IFX_ERROR){
		ab_err_set (AB_ERR_UNKNOWN, "Set Caller Id ioctl error");
		goto __exit_fail;
	}

__exit_success:
	return 0;
__exit_fail:
	return -1;
}/*}}}*/
