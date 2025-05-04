#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>

#include "ab_internal_v22.h"

/**
	Get event from device
\param dev - device to operate on it
\param evt - occured event
\param chan_available - is event occured on chan or on whole device
\return 
	0 in success case and other value otherwise
\remark
	returns the ioctl error value and writes error message
*/
int 
ab_dev_event_get(ab_dev_t * const dev, ab_dev_event_t * const evt, 
		unsigned char * const chan_available )
{/*{{{*/
	IFX_TAPI_EVENT_t ioctl_evt;
	unsigned long int evt_type;
	unsigned long int evt_subtype;
	int err = 0;

	memset(&ioctl_evt, 0, sizeof(ioctl_evt));
	memset(evt, 0, sizeof(*evt));

	*chan_available = 0;

	ioctl_evt.ch = IFX_TAPI_EVENT_ALL_CHANNELS;

	err = ioctl(dev->cfg_fd, IFX_TAPI_EVENT_GET, &ioctl_evt);
	if( err ){
		ab_err_set(AB_ERR_UNKNOWN, "Getting event (ioctl)"); 
		goto ab_dev_event_get_exit;
	}

	evt->id = ab_dev_event_NONE;

	if(!ioctl_evt.id){
		goto ab_dev_event_get_exit;
	}

	/* ioctl_evt.ch = [0;1] ?- swap channels */
	evt->ch = ioctl_evt.ch;
	evt->more = ioctl_evt.more;

	evt_type = (ioctl_evt.id & 0xFF000000);
	evt_subtype = (ioctl_evt.id & 0x000000FF);

	if       (ioctl_evt.id == IFX_TAPI_EVENT_FXS_ONHOOK){
		evt->id = ab_dev_event_FXS_ONHOOK;
	} else if(ioctl_evt.id == IFX_TAPI_EVENT_FXS_OFFHOOK){
		evt->id = ab_dev_event_FXS_OFFHOOK;
	} else if(ioctl_evt.id == IFX_TAPI_EVENT_DTMF_DIGIT){
		evt->id = ab_dev_event_FXS_DIGIT_TONE;
		evt->data = ioctl_evt.data.dtmf.ascii;
		evt->data += 
			(long )(ioctl_evt.data.dtmf.local) << 9;
		evt->data += 
			(long )(ioctl_evt.data.dtmf.network) << 8;
	} else if(ioctl_evt.id == IFX_TAPI_EVENT_PULSE_DIGIT){
		evt->id = ab_dev_event_FXS_DIGIT_PULSE;
		if(ioctl_evt.data.pulse.digit == 0xB){
			evt->data = '0';
		} else {
			evt->data = '0' + ioctl_evt.data.pulse.digit;
		}
	} else if(ioctl_evt.id == IFX_TAPI_EVENT_FXO_RING_START){
		evt->id = ab_dev_event_FXO_RINGING;
		evt->data = 1;
	} else if(ioctl_evt.id == IFX_TAPI_EVENT_FXO_RING_STOP){
		evt->id = ab_dev_event_FXO_RINGING;
		evt->data = 0;
	} else if(ioctl_evt.id == IFX_TAPI_EVENT_FAXMODEM_CED){
		evt->id = ab_dev_event_FM_CED;
		evt->data = 1;
	} else if(ioctl_evt.id == IFX_TAPI_EVENT_FAXMODEM_CEDEND){
		evt->id = ab_dev_event_FM_CED;
		evt->data = 0;
	} else if(evt_type == IFX_TAPI_EVENT_TYPE_COD){
		evt->id = ab_dev_event_COD;
		evt->data = evt_subtype;
	} else if(evt_type == IFX_TAPI_EVENT_TYPE_TONE_GEN){
		evt->id = ab_dev_event_TONE;
		evt->data = evt_subtype;
	} else {
		evt->id = ab_dev_event_UNCATCHED;
		evt->data = ioctl_evt.id;
	}

	if (ioctl_evt.ch == IFX_TAPI_EVENT_ALL_CHANNELS){
		*chan_available = 0;
		evt->ch = 0;
	} else {
		*chan_available = 1;
	}

ab_dev_event_get_exit:
	return err;
}/*}}}*/

/**
	Cleaning the given device from events on it 
\param
	dev - device to operate on it
\return 
	0 in success case and other value otherwise
\remark
	returns the ioctl error value and writes error message
*/
int ab_dev_event_clean(ab_dev_t * const dev)
{/*{{{*/
	ab_dev_event_t evt;
	int err = 0;

	memset(&evt, 0, sizeof(evt));

	do {
		unsigned char ch_av;
		err = ab_dev_event_get(dev, &evt, &ch_av);
		if(err){
			goto __exit_fail;
		}
	} while (evt.more);

	return 0;
__exit_fail:
	return -1;
}/*}}}*/

