#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>

#include "ab_internal_v22.h"

#define AB_DRIVER_DEV_NODE "/dev/sgatab"

int ab_g_err_idx;
char ab_g_err_str[ERR_STR_LENGTH];
int ab_g_err_extra_value;

static void ab_chan_status_init( ab_chan_t * const chan );
static int get_devs_params (unsigned int * const devs_num, 
		ab_dev_params_t ** const dprms);

extern int ab_dev_event_clean(ab_dev_t * const dev);

/**
	Create the ab_t object. 
\return
	Pointer to created object or NULL if something nasty happens.
\remark
	This function:
	- allocates memory
	- make nessesary initializations
*/
ab_t* 
ab_create( void )
{/*{{{*/
	ab_t *ab = NULL;
	ab_dev_params_t * dprms = NULL;
	unsigned int devs_num;
	unsigned int chans_num;
	int i;
	int err;

	err = get_devs_params (&devs_num, &dprms);
	if(err){
		goto __exit_fail;
	}

	ab = malloc(sizeof(*ab));
	if( !ab){
		ab_err_set(AB_ERR_NO_MEM, "Not enough memory for ab");
		goto __free_and_exit_fail;
	}
	memset(ab, 0, sizeof(*ab));

	ab->devs_num = devs_num;
	chans_num = devs_num * CHANS_PER_DEV;
	ab->chans_num = chans_num;
	ab->chans_per_dev = CHANS_PER_DEV;
	if((! ab->devs_num) || (! ab->chans_num)) {
		ab_err_set(AB_ERR_BAD_PARAM, "devices or channels number is zero" );
		goto __free_and_exit_fail;
	}

	ab->chans = malloc(sizeof(*(ab->chans)) * ab->chans_num);
	ab->devs = malloc(sizeof(*(ab->devs)) * ab->devs_num);
	if( (! ab->chans) || (! ab->devs) ){
		ab_err_set(AB_ERR_NO_MEM, "no memory for chans or devs structures");
		goto __free_and_exit_fail;
	}
	memset(ab->chans, 0, sizeof(*(ab->chans)) * ab->chans_num);
	memset(ab->devs, 0, sizeof(*(ab->devs)) * ab->devs_num);

	/* Devices init */ 
	for (i=0; i<devs_num; i++){
		ab_dev_t * curr_dev = &ab->devs[ i ];
		int fd_chip;
		char dev_node[ 50 ];

		curr_dev->idx = i + 1;
		curr_dev->parent = ab;

		sprintf(dev_node,"/dev/vin%d0", curr_dev->idx );

		fd_chip = open(dev_node, O_RDWR);
		if(fd_chip==-1){
			ab_err_set(AB_ERR_NO_FILE, "opening vinetic device node");
			goto __free_and_exit_fail;
		}
		curr_dev->cfg_fd = fd_chip;
		if(dprms[i].type == dev_type_FXS){
			curr_dev->type = ab_dev_type_FXS;
		} else if(dprms[i].type == dev_type_FXO){
			curr_dev->type = ab_dev_type_FXO;
		} else if(dprms[i].type == dev_type_VF){
			curr_dev->type = ab_dev_type_VF;
		}
	}

	/* Channels init */ 
	for(i=0; i<chans_num; i++) {
		ab_chan_t * curr_chan = &ab->chans[ i ];
		int fd_chan;
		char dev_node[ 50 ];
		/* it should be 0 if i is odd(!/2) and 1 if i is even(/2) */
		int chan_idx_in_dev = CHANS_PER_DEV - (1 + i%CHANS_PER_DEV);
		int pdev_idx = i / CHANS_PER_DEV;

		curr_chan->idx = chan_idx_in_dev + 1;
		curr_chan->parent = &ab->devs[pdev_idx];

		/* Initialize channel */
		sprintf(dev_node, "/dev/vin%d%d", 
				curr_chan->parent->idx, curr_chan->idx);

		fd_chan = open(dev_node, O_RDWR);
		if (fd_chan==-1){
			ab_err_set(AB_ERR_NO_FILE, "opening vinetic channel node");
			goto __free_and_exit_fail;
		}
		curr_chan->rtp_fd = fd_chan;
		curr_chan->abs_idx = dprms[pdev_idx].chans_idx[chan_idx_in_dev];

		if(curr_chan->abs_idx >= CHANS_MAX){
			ab_err_set(AB_ERR_BAD_PARAM, "too many channels on boards");
			goto __free_and_exit_fail;
		}
		ab->pchans[curr_chan->abs_idx] = curr_chan;

		/* set channel status to initial proper values */
		ab_chan_status_init (curr_chan);
	}

	if(dprms){
		free (dprms);
	}
	return ab;

__free_and_exit_fail:
	ab_destroy(&ab);
	if(dprms){
		free (dprms);
	}
__exit_fail:
	return NULL;
} /*}}}*/

/**
	This one returns the parameters of all devices on the all boards
\param[out] devs_num - number of the found devices will be returned
\param[out] dprms - devices parameters
\return 
	ioctl result
\remark
	it allocates memory for *dprms, that should be freed outside
*/
static int 
get_devs_params (unsigned int * const devs_num, ab_dev_params_t ** const dprms)
{/*{{{*/
	ab_board_params_t bp;
	ab_dev_params_t tmp_prms [BOARDS_MAX*DEVS_PER_BOARD_MAX];
	int cp;
	int i;
	int j;
	int ab_fd;
	int err;

	/* AB read config from drv_sgatab.ko */
	memset (&cp, 0, sizeof(cp));
	memset (tmp_prms, 0, sizeof(tmp_prms));

	/* open sgatab dev node */
	ab_fd = open(AB_DRIVER_DEV_NODE, O_RDWR);
	if (ab_fd==-1) {
		ab_err_set(AB_ERR_NO_FILE, "opening board device node");
		goto __exit_fail;
	}

	/* Get boards count info */
	err = ioctl(ab_fd, SGAB_GET_BOARDS_COUNT, &cp);
	if(err) {
		ab_err_set(AB_ERR_UNKNOWN, "getting ata boards count info (ioctl)");
		goto __close_and_exit_fail;
	}

	for (*devs_num=0,i=0; i<cp; i++){
		memset(&bp, 0, sizeof(bp));
		bp.board_idx = i;
		err = ioctl (ab_fd, SGAB_GET_BOARD_PARAMS, &bp);
		if(err){
			ab_err_set(AB_ERR_UNKNOWN,"SGAB_GET_BOARD_PARAMS");
			goto __close_and_exit_fail;
		} else if( !bp.is_present){
			ab_err_set(AB_ERR_UNKNOWN, "internal error bp is not present");
			goto __close_and_exit_fail;
		}
		for(j=0;j<DEVS_PER_BOARD_MAX; j++){
			if(bp.devices[j].type != dev_type_ABSENT){
				tmp_prms[*devs_num] = bp.devices[j];
				(*devs_num)++;
			}
		}
	}
	
	*dprms = malloc(sizeof(**dprms)*(*devs_num));
	if( !(*dprms)){
		ab_err_set(AB_ERR_NO_MEM, "no memory for devparams");
		goto __close_and_exit_fail;
	}

	for(i=0; i<(*devs_num); i++){
		(*dprms)[i] = tmp_prms[i];
	}

	close(ab_fd);
	return 0;
__close_and_exit_fail:
	close(ab_fd);
__exit_fail:
	return -1;
}/*}}}*/

/**
	Destroy the ab_t object. 
\param [in]
	ab - pointer to pointer to destroying object.
		pointer to object will set to NULL
		after destroying
\remark
	After all ab = NULL.
*/
void 
ab_destroy( ab_t ** ab )
{/*{{{*/
	ab_t * ab_tmp = *ab;
	if(ab_tmp) {
		if(ab_tmp->chans) {
			int i;
			int j;
			j = ab_tmp->chans_num;
			for(i = 0; i < j; i++) {
				ab_chan_t * curr_chan = &ab_tmp->chans[ i ];
				if(curr_chan->rtp_fd > 0) {
					close(curr_chan->rtp_fd);
				}
			}
			free (ab_tmp->chans);
		}
		if(ab_tmp->devs) {
			int i;
			int j;
			j = ab_tmp->devs_num;
			for(i = 0; i < j; i++) {
				ab_dev_t * curr_dev = &ab_tmp->devs[ i ];
				if(curr_dev->cfg_fd > 0) {
					close(curr_dev->cfg_fd);
				}
			}
			free (ab_tmp->devs);
		}
		free (ab_tmp);
		ab_tmp = NULL;
	}
}/*}}}*/

/**
 * \param[in] ab - ata board 
 * \param[in] abs_idx - absolute channel index
 * \param[in] path - path to CRAM file
 *
 * \retval -1 if something nasty happens
 * \retval 0 and greater - the channel number
 */ 
int 
ab_chan_cram_init (ab_chan_t const * const chan, char const * const path)
{/*{{{*/
	struct bbd_format_s {
		unsigned char * buf;
		unsigned long size;
	} bbd_download;
	int fd;
	int err;

	if(! chan){
		ab_g_err_idx = AB_ERR_BAD_PARAM;
		strcpy(ab_g_err_str, "Channel don`t exist");
		goto __exit_fail;
	}

	memset(&bbd_download, 0, sizeof (bbd_download));

	fd = open(path, O_RDONLY);
	if( fd <= 0 ) {
		ab_g_err_idx = AB_ERR_NO_FILE;
		strcpy(ab_g_err_str, "Opening firmware file");
		goto __exit_fail;
	}
	bbd_download.size = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);

	bbd_download.buf = malloc(bbd_download.size);
	if( ! bbd_download.buf ){
		ab_g_err_idx = AB_ERR_NO_MEM;
		goto __exit_fail_close;
	}

	if(read(fd, bbd_download.buf, bbd_download.size) != bbd_download.size){
		ab_g_err_idx = AB_ERR_NO_FILE;
		strcpy(ab_g_err_str, "Reading firmware file");
		goto __exit_fail_free;
	}

	err = ioctl(chan->rtp_fd, FIO_VINETIC_BBD_DOWNLOAD, &bbd_download);
	if( err ){
		ab_g_err_idx = AB_ERR_UNKNOWN;
		strcpy(ab_g_err_str, "Initing chan with CRAM (ioctl)");
		goto __exit_fail_free;
	}

	free (bbd_download.buf);
	close (fd);
	return 0;

__exit_fail_free:
	free (bbd_download.buf);
__exit_fail_close:
	close (fd);
__exit_fail:
	return -1;
}/*}}}*/

/**
 * \param[in] ab - ata board 
 * \param[in] abs_idx - absolute channel index
 *
 * \retval -1 if something nasty happens
 * \retval 0 and greater - the channel number
 */ 
int 
ab_get_chan_idx_by_abs(ab_t const * const ab, int const abs_idx)
{/*{{{*/
	int ret_idx;
	int chans_num; 
	int i;

	ret_idx = -1;
	chans_num = ab->chans_num;
	for(i=0; i<chans_num; i++){
		if (abs_idx == ab->chans[ i ].abs_idx){
			ret_idx = i;
			break;
		}
	}
	return ret_idx;
}/*}}}*/

/**
	Sets the proper state and status of the channel structure 
\param chan[in,out] - channel struture 
\remark
	it mutes all rings and tones on the FXS channel and do onhook on FXO
*/
static void 
ab_chan_status_init( ab_chan_t * const chan )
{/*{{{*/
	if(chan->parent->type == ab_dev_type_FXS){
		/* linefeed to standby */
		ioctl(chan->rtp_fd, IFX_TAPI_LINE_FEED_SET, IFX_TAPI_LINE_FEED_STANDBY);
		chan->status.linefeed = ab_chan_linefeed_STANDBY;
		/* ring to mute */
		ioctl (chan->rtp_fd, IFX_TAPI_RING_STOP, 0);
		chan->status.ring = ab_chan_ring_MUTE;
		/* tone to mute */
		ioctl (chan->rtp_fd, IFX_TAPI_TONE_LOCAL_PLAY, 0);
		chan->status.tone = ab_chan_tone_MUTE;
	} else if (chan->parent->type == ab_dev_type_FXO){
		/* hook to onhook */
		ioctl (chan->rtp_fd, IFX_TAPI_FXO_HOOK_SET, IFX_TAPI_FXO_HOOK_ONHOOK);
		chan->status.hook = ab_chan_hook_ONHOOK;
	} else if (chan->parent->type == ab_dev_type_VF){
		/* linefeed to standby */
		ioctl(chan->rtp_fd, IFX_TAPI_LINE_FEED_SET, IFX_TAPI_LINE_FEED_STANDBY);
		chan->status.linefeed = ab_chan_linefeed_STANDBY;
		/* linefeed to active */
		ioctl(chan->rtp_fd, IFX_TAPI_LINE_FEED_SET, IFX_TAPI_LINE_FEED_ACTIVE);
		chan->status.linefeed = ab_chan_linefeed_ACTIVE;
		/* ring to mute */
		ioctl (chan->rtp_fd, IFX_TAPI_RING_STOP, 0);
		chan->status.ring = ab_chan_ring_MUTE;
		/* tone to mute */
		ioctl (chan->rtp_fd, IFX_TAPI_TONE_LOCAL_PLAY, 0);
		chan->status.tone = ab_chan_tone_MUTE;
	}
	/* initial onhook detected (from channel) */
	ab_dev_event_clean(chan->parent);
}/*}}}*/

