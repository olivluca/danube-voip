#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <time.h>

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
	char const *err_msg;
	int index;

	switch(tone){
		case ab_chan_tone_MUTE: {
			index = 0;
			err_msg = "stop playing tone (ioctl)";
			break;
		}
		case ab_chan_tone_DIAL: {
			index = TAPI_TONE_LOCALE_DIAL_CODE;
			err_msg = "playing dialtone (ioctl)";
			break;
		}
		case ab_chan_tone_BUSY: {
			index = TAPI_TONE_LOCALE_BUSY_CODE;
			err_msg = "playing busy (ioctl)";
			break;
		}
		case ab_chan_tone_RINGBACK: {
			index = TAPI_TONE_LOCALE_RINGING_CODE;
			err_msg = "playing ringback (ioctl)";
			break;
		}
		default:
			index = -1;
	}
	if (index>=0) {
		err = err_set_ioctl(chan, IFX_TAPI_TONE_LOCAL_PLAY, index, err_msg);
		if( !err){
			chan->status.tone = tone;
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
			IFX_TAPI_CID_MSG_ELEMENT_t message[3];
			memset(&cidType1, 0, sizeof(cidType1));
			memset(&message, 0, sizeof(message));
			int i=0;
			if (chan->cid_std!=cid_OFF && number!=NULL && number[0]!=0) {
				message[i].string.elementType = IFX_TAPI_CID_ST_CLI;
				message[i].string.len = strlen(number);
				strncpy(message[i].string.element, number, sizeof(message[0].string.element));
				i++;
			}
			if (chan->cid_std!=cid_OFF && name!=NULL && name[0]!=0) {
				message[i].string.elementType = IFX_TAPI_CID_ST_NAME;
				message[i].string.len = strlen(name);
				strncpy(message[i].string.element, name, sizeof(message[0].string.element));
				i++;
			}
			time_t timestamp;
			struct tm *tm;
			if ((time(&timestamp) != -1) && ((tm=localtime(&timestamp)) != NULL)) {
                        	message[i].date.elementType = IFX_TAPI_CID_ST_DATE;
                        	message[i].date.day = tm->tm_mday;
                        	message[i].date.month = tm->tm_mon;
                        	message[i].date.hour = tm->tm_hour;
                        	message[i].date.mn = tm->tm_min;
                        	i++;
			}
			if (i==0) {
				/* neither caller id, name or date, normal ring */
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

/******************************************************************
 *   Asterisk routines
 */

struct ast_tone_zone_part {
	unsigned int freq1;
	unsigned int freq2;
	unsigned int time;
	unsigned int modulate:1;
	unsigned int midinote:1;
};

int ast_tone_zone_part_parse(const char *s, struct ast_tone_zone_part *tone_data)
{
	if (sscanf(s, "%30u+%30u/%30u", &tone_data->freq1, &tone_data->freq2,
			&tone_data->time) == 3) {
		/* f1+f2/time format */
	} else if (sscanf(s, "%30u+%30u", &tone_data->freq1, &tone_data->freq2) == 2) {
		/* f1+f2 format */
		tone_data->time = 0;
	} else if (sscanf(s, "%30u*%30u/%30u", &tone_data->freq1, &tone_data->freq2,
			&tone_data->time) == 3) {
		/* f1*f2/time format */
		tone_data->modulate = 1;
	} else if (sscanf(s, "%30u*%30u", &tone_data->freq1, &tone_data->freq2) == 2) {
		/* f1*f2 format */
		tone_data->time = 0;
		tone_data->modulate = 1;
	} else if (sscanf(s, "%30u/%30u", &tone_data->freq1, &tone_data->time) == 2) {
		/* f1/time format */
		tone_data->freq2 = 0;
	} else if (sscanf(s, "%30u", &tone_data->freq1) == 1) {
		/* f1 format */
		tone_data->freq2 = 0;
		tone_data->time = 0;
	} else if (sscanf(s, "M%30u+M%30u/%30u", &tone_data->freq1, &tone_data->freq2,
			&tone_data->time) == 3) {
		/* Mf1+Mf2/time format */
		tone_data->midinote = 1;
	} else if (sscanf(s, "M%30u+M%30u", &tone_data->freq1, &tone_data->freq2) == 2) {
		/* Mf1+Mf2 format */
		tone_data->time = 0;
		tone_data->midinote = 1;
	} else if (sscanf(s, "M%30u*M%30u/%30u", &tone_data->freq1, &tone_data->freq2,
			&tone_data->time) == 3) {
		/* Mf1*Mf2/time format */
		tone_data->modulate = 1;
		tone_data->midinote = 1;
	} else if (sscanf(s, "M%30u*M%30u", &tone_data->freq1, &tone_data->freq2) == 2) {
		/* Mf1*Mf2 format */
		tone_data->time = 0;
		tone_data->modulate = 1;
		tone_data->midinote = 1;
	} else if (sscanf(s, "M%30u/%30u", &tone_data->freq1, &tone_data->time) == 2) {
		/* Mf1/time format */
		tone_data->freq2 = -1;
		tone_data->midinote = 1;
	} else if (sscanf(s, "M%30u", &tone_data->freq1) == 1) {
		/* Mf1 format */
		tone_data->freq2 = -1;
		tone_data->time = 0;
		tone_data->midinote = 1;
	} else {
		return -1;
	}

	return 0;
}


static int ast_strlen_zero(const char *s)
{
	return (!s || (*s == '\0'));
}

static char * ast_skip_blanks(const char *str)
{
	if (str) {
		while (*str && ((unsigned char) *str) < 33) {
			str++;
		}
	}

	return (char *) str;
}
static char *ast_trim_blanks(char *str)
{
	char *work = str;

	if (work) {
		work += strlen(work) - 1;
		/* It's tempting to only want to erase after we exit this loop,
		   but since ast_trim_blanks *could* receive a constant string
		   (which we presumably wouldn't have to touch), we shouldn't
		   actually set anything unless we must, and it's easier just
		   to set each position to \0 than to keep track of a variable
		   for it */
		while ((work >= str) && ((unsigned char) *work) < 33)
			*(work--) = '\0';
	}
	return str;
}

static char *ast_strip(char *s)
{
	if ((s = ast_skip_blanks(s))) {
		ast_trim_blanks(s);
	}
	return s;
}


/******************************************************************
 *   chan_lantiq  routines
 */

/* add a frequency to TAPE tone structure */
/* returns the TAPI frequency ID */

static int tapitone_add_freq (IFX_TAPI_TONE_t *tone, IFX_uint32_t freq) {
	const int n=4; /* TAPI tone structure supports up to 4 frequencies */
	int error=0;
	int ret;
	int i;

	/* pointer array for freq's A, B, C, D */
	IFX_uint32_t *freqarr[] = { &(tone->simple.freqA), &(tone->simple.freqB), &(tone->simple.freqC), &(tone->simple.freqD) };

	/* pointer array for level's A, B, C, D */
	IFX_int32_t *lvlarr[] = { &(tone->simple.levelA), &(tone->simple.levelB), &(tone->simple.levelC), &(tone->simple.levelD) };

	/* array for freq IDs */
	IFX_uint32_t retarr[] = { IFX_TAPI_TONE_FREQA, IFX_TAPI_TONE_FREQB, IFX_TAPI_TONE_FREQC, IFX_TAPI_TONE_FREQD };

	/* determine if freq already set */
	for (i = 0; i < n; i++) {
		if(*freqarr[i] == freq) /* freq found */
			break;
		else if (i == n-1) /* last iteration */
			error=1; /* not found */
	}

	/* write frequency if not already set */
	if(error) {
		error=0; /* reset error flag */
		/* since freq is not set, write it into first free place */
		for (i = 0; i < n; i++) {
			if(!*freqarr[i]) { /* free place */
				*freqarr[i] = freq; /* set freq */
				*lvlarr[i] = -150; /* set volume level */
				break;
			} else if (i == n-1) /* last iteration */
				error=1; /* no free place becaus maximum count of freq's is set */
		}
	}

	/* set freq ID return value */
	if (!freq || error)
		ret = IFX_TAPI_TONE_FREQNONE;
	else
		ret = retarr[i];

	return ret; /* freq ID */
}

/* convert asterisk playlist string to tapi tone structure */
/* based on ast_playtones_start() from indications.c of asterisk 13 */
static void playlist_to_tapitone (const char *playlst, IFX_uint32_t index, IFX_TAPI_TONE_t *tone)
{
	char *s, *data = strdup(playlst);
	char *stringp;
	char *separator;
	int i;

	/* initialize tapi tone structure */
	memset(tone, 0, sizeof(IFX_TAPI_TONE_t));
	tone->simple.format = IFX_TAPI_TONE_TYPE_SIMPLE;
	tone->simple.index = index;

	stringp = data;

	/* check if the data is separated with '|' or with ',' by default */
	if (strchr(stringp,'|')) {
		separator = "|";
	} else {
		separator = ",";
	}

	for ( i = 0; (s = strsep(&stringp, separator)) && !ast_strlen_zero(s) && i < IFX_TAPI_TONE_STEPS_MAX; i++) {
		struct ast_tone_zone_part tone_data = {
			.time = 0,
		};

		s = ast_strip(s);
		if (s[0]=='!') {
			s++;
		}

		if (ast_tone_zone_part_parse(s, &tone_data)) {
			//FIXME ast_log(LOG_ERROR, "Failed to parse tone part '%s'\n", s);
			continue;
		}

		/* first tone must hava a cadence */
		if (i==0 && !tone_data.time)
			tone->simple.cadence[i] = 1000;
		else
			tone->simple.cadence[i] = tone_data.time;

		/* check for modulation */
		if (tone_data.modulate) {
			tone->simple.modulation[i] = IFX_TAPI_TONE_MODULATION_ON;
			tone->simple.modulation_factor = IFX_TAPI_TONE_MODULATION_FACTOR_90;
		}

		/* copy freq's to tapi tone structure */
		/* a freq will implicitly skipped if it is zero  */
		tone->simple.frequencies[i] |= tapitone_add_freq(tone, tone_data.freq1);
		tone->simple.frequencies[i] |= tapitone_add_freq(tone, tone_data.freq2);
	}
	free(data);
}
/**
	Setup the given tone using an asterisk playlist
\param chan - channel to setup tone
\param tone - tone to setup
\param playlst - asterisk playlist defining the tone
\return
	ioctl result
*/
int
ab_FXS_set_tone(ab_chan_t *const chan, enum ab_chan_tone_e tone, const char* playlst)
{
	IFX_TAPI_TONE_t tapi_tone;
	IFX_uint32_t index;
	char const * err_msg;

	switch(tone){
		case ab_chan_tone_MUTE: {
			return AB_ERR_NO_ERR;
			break;
		}
		case ab_chan_tone_DIAL: {
			index = TAPI_TONE_LOCALE_DIAL_CODE;
			err_msg = "dial";
			break;
		}
		case ab_chan_tone_BUSY: {
			index = TAPI_TONE_LOCALE_BUSY_CODE;
			err_msg = "busy";
			break;
		}
		case ab_chan_tone_RINGBACK: {
			index = TAPI_TONE_LOCALE_RINGING_CODE;
			err_msg = "ringing";
			break;
		}
		default: { //it shouldn't happen
			return AB_ERR_NO_ERR;
		}
	}
	memset(&tapi_tone, 0, sizeof(IFX_TAPI_TONE_t));
	tapi_tone.simple.format = IFX_TAPI_TONE_TYPE_SIMPLE;
	playlist_to_tapitone(playlst, index, &tapi_tone);
	if (ioctl(chan->rtp_fd, IFX_TAPI_TONE_TABLE_CFG_SET, &tapi_tone)) {
		sprintf(ab_g_err_str, "IFX_TAPI_TONE_CFG_SET %s tone failed", err_msg);
		return AB_ERR_UNKNOWN;
	}
	return AB_ERR_NO_ERR;
}

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

	chan->cid_std = std;
	if (std == cid_OFF)
	  goto __exit_success;
	
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
