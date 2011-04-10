#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "ab_internal_v22.h"

#define AB_DRIVER_DEV_NODE "/dev/sgatab"
#define TAPI_AUDIO_DEV_NUM 2
#define TAPI_LL_DEV_FIRMWARE_NAME   "/lib/firmware/danube_firmware.bin" 
#define TAPI_LL_BBD_NAME   "/lib/firmware/danube_bbd_fxs.bin" 

int ab_g_err_idx;
char ab_g_err_str[ERR_STR_LENGTH];
int ab_g_err_extra_value;

static void ab_chan_status_init( ab_chan_t * const chan );
#if 0
static int get_devs_params (unsigned int * const devs_num, 
		ab_dev_params_t ** const dprms);
#endif		

extern int ab_dev_event_clean(ab_dev_t * const dev);

static int tapi_dev_binary_buffer_create(
                     const char *pPath,
                     unsigned char **ppBuf,
                     unsigned int *pBufSz)
{
   int status = AB_ERR_NO_ERR;
   FILE *fd;
   struct stat file_stat;

   /* Open binary file for reading*/
   fd = fopen(pPath, "rb");
   if (fd == NULL) {
      sprintf(ab_g_err_str, "ERROR -  binary file %s open failed!", pPath);
      return AB_ERR_NO_FILE;
   }

   /* Get file statistics*/
   if (stat(pPath, &file_stat) != 0) {
      sprintf(ab_g_err_str, "ERROR -  file %s statistics get failed!", pPath);
      return AB_ERR_NO_FILE;
   }

   *ppBuf = malloc(file_stat.st_size);
   if (*ppBuf == NULL) {
      sprintf(ab_g_err_str, "ERROR -  binary file %s memory allocation failed!", pPath);
      status = AB_ERR_NO_FILE;

      goto on_exit;
   }

   if (fread (*ppBuf, sizeof(unsigned char), file_stat.st_size, fd) <= 0) {
      sprintf(ab_g_err_str, "ERROR - file %s read failed!", pPath);
      status = AB_ERR_NO_FILE;

      goto on_exit;
   }

   *pBufSz = file_stat.st_size; 

on_exit:
   if (fd != NULL) {
      fclose(fd);
   }

   if (*ppBuf != NULL && status != AB_ERR_NO_ERR) {
      free(*ppBuf);
   }

   return status;
}

static void tapi_dev_binary_buffer_delete(unsigned char *pBuf)
{
   if (pBuf != NULL)
      free(pBuf);
}

static int tapi_dev_firmware_download(
                     int fd,
                     const char *pPath)
{
   int status = AB_ERR_NO_ERR;
   unsigned char *pFirmware = NULL;
   unsigned int binSz = 0;
   VMMC_IO_INIT vmmc_io_init;

   /* Create binary buffer*/
   status = tapi_dev_binary_buffer_create(pPath, &pFirmware, &binSz);
   if (status != AB_ERR_NO_ERR) {
      return status;
   }

   /* Download Voice Firmware*/
   memset(&vmmc_io_init, 0, sizeof(VMMC_IO_INIT));
   vmmc_io_init.pPRAMfw   = pFirmware;
   vmmc_io_init.pram_size = binSz;

   status = ioctl(fd, FIO_FW_DOWNLOAD, &vmmc_io_init);
   if (status != AB_ERR_NO_ERR) {
      sprintf(ab_g_err_str, "ERROR -  FIO_FW_DOWNLOAD ioctl failed!");
   }

   /* Delete binary buffer*/
   tapi_dev_binary_buffer_delete(pFirmware);

   return status;
}

static int tapi_dev_bbd_download(
                     int fd,
                     const char *pPath)
{
   int status = AB_ERR_NO_ERR;
   unsigned char *pFirmware = NULL;
   unsigned int binSz = 0;
   VMMC_DWLD_t bbd_data;


   /* Create binary buffer*/
   status = tapi_dev_binary_buffer_create(pPath, &pFirmware, &binSz);
   if (status != AB_ERR_NO_ERR) {
      return status;
   }

   /* Download Voice Firmware*/
   memset(&bbd_data, 0, sizeof(VMMC_DWLD_t));
   bbd_data.buf = pFirmware;
   bbd_data.size = binSz;

   status = ioctl(fd, FIO_BBD_DOWNLOAD, &bbd_data);
   if (status != AB_ERR_NO_ERR) {
      sprintf(ab_g_err_str, "ERROR -  FIO_BBD_DOWNLOAD ioctl failed!");
   }

   /* Delete binary buffer*/
   tapi_dev_binary_buffer_delete(pFirmware);

   return status;
}

static int tapi_dev_start(ab_t *ab)
{
   int status = AB_ERR_NO_ERR;
   unsigned char c;
   IFX_TAPI_DEV_START_CFG_t tapistart;
   IFX_TAPI_MAP_DATA_t datamap;
#if 0   
   IFX_TAPI_ENC_CFG_t enc_cfg;
   IFX_TAPI_LINE_VOLUME_t vol;
#endif   
   IFX_TAPI_SIG_DETECTION_t dtmfDetection;
   IFX_TAPI_SIG_DETECTION_t faxSig;

#if 0   
   /* Open device*/
   f->dev_ctx.dev_fd = tapi_dev_open(TAPI_LL_DEV_BASE_PATH, 0);

   if (f->dev_ctx.dev_fd < 0) {
      TRACE_((THIS_FILE, "ERROR - TAPI device open failed!"));
      return PJ_EUNKNOWN;
   }

   for (c = 0; c < TAPI_AUDIO_DEV_NUM; c++) {
      f->dev_ctx.ch_fd[c] = tapi_dev_open(TAPI_LL_DEV_BASE_PATH, TAPI_AUDIO_MAX_DEV_NUM - c);

      if (f->dev_ctx.dev_fd < 0) {
         TRACE_((THIS_FILE, "ERROR - TAPI channel%d open failed!", c));
         return PJ_EUNKNOWN;
      }
      
      f->dev_ctx.data2phone_map[c] = c & 0x1 ? 0 : 1;
   }
#endif
   /* Stop TAPI*/
   status = ioctl(ab->devs[0].cfg_fd, IFX_TAPI_DEV_STOP, 0);
   if (status != AB_ERR_NO_ERR) {
      sprintf(ab_g_err_str, "ERROR - IFX_TAPI_DEV_STOP ioctl failed");
      return status;
   }

   status = tapi_dev_firmware_download(ab->devs[0].cfg_fd, TAPI_LL_DEV_FIRMWARE_NAME);
   if (status != AB_ERR_NO_ERR) {
      return status;
   }

   memset(&tapistart, 0x0, sizeof(IFX_TAPI_DEV_START_CFG_t));
   tapistart.nMode = IFX_TAPI_INIT_MODE_VOICE_CODER;

   /* Start TAPI*/
   status = ioctl(ab->devs[0].cfg_fd, IFX_TAPI_DEV_START, &tapistart);
   if (status != AB_ERR_NO_ERR) {
      sprintf(ab_g_err_str, "ERROR - IFX_TAPI_DEV_START ioctl failed");
      return status;
   }

   /* Download coefficients */
   status = tapi_dev_bbd_download(ab->devs[0].cfg_fd, TAPI_LL_BBD_NAME);
   if (status != AB_ERR_NO_ERR) {
      sprintf(ab_g_err_str, "ERROR - Voice Coefficients Download failed!");
      return status;
   }

   for (c = 0; c < TAPI_AUDIO_DEV_NUM; c++) {
      /* Perform mapping*/
      memset(&datamap, 0x0, sizeof(IFX_TAPI_MAP_DATA_t));
      datamap.nDstCh  = c & 0x1 ? 0 : 1; /*f->dev_ctx.data2phone_map[c];*/
      datamap.nChType = IFX_TAPI_MAP_TYPE_PHONE;

      status = ioctl(ab->chans[c].rtp_fd, IFX_TAPI_MAP_DATA_ADD, &datamap);

      if (status != AB_ERR_NO_ERR) {
         sprintf(ab_g_err_str, "ERROR - IFX_TAPI_MAP_DATA_ADD ioctl failed");
         return status;
      }

      char data[10] = {0xFF,0xFF,0xF0,0,0,0,0,0,0,0};
      IFX_TAPI_RING_CADENCE_t ringCadence;

      /* ENABLE detection of DTMF tones 
       * from local interface (ALM X) */
      memset(&dtmfDetection, 0, sizeof (dtmfDetection));
      dtmfDetection.sig = IFX_TAPI_SIG_DTMFTX;
      status = ioctl (ab->chans[c].rtp_fd,IFX_TAPI_SIG_DETECT_ENABLE,&dtmfDetection);
      if (status != AB_ERR_NO_ERR ){
         sprintf(ab_g_err_str, "ERROR - IFX_TAPI_SIG_DTMFTX ioctl failed");
         return status;
      }
      
      /* configure ring candence */
      memset(&ringCadence, 0, sizeof(ringCadence));
      memcpy(&ringCadence.data, data, sizeof(data));
      ringCadence.nr = sizeof(data) * 8;
      status = ioctl(ab->chans[c].rtp_fd, IFX_TAPI_RING_CADENCE_HR_SET, &ringCadence);
      if (status != AB_ERR_NO_ERR ){
         sprintf(ab_g_err_str, "ERROR - IFX_TAPI_RING_CADENCE_HR_SET ioctl failed");
         return status;
      }
      
      /* ENABLE detection of FAX signals */
      memset (&faxSig, 0, sizeof(faxSig));
      faxSig.sig = IFX_TAPI_SIG_CEDRX | IFX_TAPI_SIG_CEDTX |
                   IFX_TAPI_SIG_CEDENDRX | IFX_TAPI_SIG_CEDENDTX;
      status = ioctl (ab->chans[c].rtp_fd,IFX_TAPI_SIG_DETECT_ENABLE,&faxSig);
      if (status != AB_ERR_NO_ERR ){
         sprintf(ab_g_err_str, "ERROR - IFX_TAPI_SIG_DETECT_ENABLE ioctl failed");
         return status;
      }

      /* Set Line feed*/
      status = ioctl(ab->chans[c].rtp_fd, IFX_TAPI_LINE_FEED_SET, IFX_TAPI_LINE_FEED_STANDBY);

      if (status != AB_ERR_NO_ERR) {
         sprintf(ab_g_err_str, "ERROR - IFX_TAPI_LINE_FEED_SET ioctl failed");
         return status;
      }
#if 0
      /* Config encoder for linear stream*/
      memset(&enc_cfg, 0x0, sizeof(IFX_TAPI_ENC_CFG_t));

      enc_cfg.nFrameLen = IFX_TAPI_COD_LENGTH_20;
      enc_cfg.nEncType  = IFX_TAPI_COD_TYPE_LIN16_8;

      status = ioctl(f->dev_ctx.ch_fd[c], IFX_TAPI_ENC_CFG_SET, &enc_cfg);
      if (status != PJ_SUCCESS) {
         TRACE_((THIS_FILE, "ERROR - IFX_TAPI_ENC_CFG_SET ioctl failed"));
         return PJ_EUNKNOWN;
      }


      /* Suppress TAPI volume, otherwise PJSIP starts autogeneration!!!*/
      vol.nGainRx = -8;
      vol.nGainTx = -8;

      status = ioctl(f->dev_ctx.ch_fd[c], IFX_TAPI_PHONE_VOLUME_SET, &vol);
      if (status != PJ_SUCCESS) {
         TRACE_((THIS_FILE, "ERROR - IFX_TAPI_PHONE_VOLUME_SET ioctl failed"));
         return PJ_EUNKNOWN;
      }
#endif      
   }
   return status;
}

static int tapi_dev_stop(ab_t *ab)
{
   int status = AB_ERR_NO_ERR;
#if 0   
   unsigned char c;
#endif
   
   
   /* Stop TAPI device*/
   if (ioctl(ab->devs[0].cfg_fd, IFX_TAPI_DEV_STOP, 0) != AB_ERR_NO_ERR) {
      sprintf(ab_g_err_str, "ERROR - IFX_TAPI_DEV_STOP ioctl failed");
      status = AB_ERR_UNKNOWN;
   }
#if 0
   /* Close device FD*/
   close(f->dev_ctx.dev_fd);

   /* Close channel FD*/
   for (c = TAPI_AUDIO_DEV_NUM; c > 0; c--) {
      close(f->dev_ctx.ch_fd[TAPI_AUDIO_DEV_NUM-c]);
   }

#endif
   return status;
}

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
#if 0
	int err;
	err = get_devs_params (&devs_num, &dprms);
	if(err){
		goto __exit_fail;
	}
#else
	devs_num = 1;
	dprms = malloc(sizeof(ab_dev_params_t));
	if( !(dprms)){
		ab_err_set(AB_ERR_NO_MEM, "no memory for devparams");
		goto __exit_fail;
	}
	dprms->type = dev_type_FXS;
	dprms->nBaseAddress = 0;
	dprms->AccessMode = 0;
	dprms->chans_idx[0] = 0;
	dprms->chans_idx[1] = 1;
#endif	
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

		sprintf(dev_node,"/dev/vmmc%d0", curr_dev->idx );

		fd_chip = open(dev_node, O_RDWR);
		if(fd_chip==-1){
			ab_err_set(AB_ERR_NO_FILE, "opening vmmc device node");
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
		sprintf(dev_node, "/dev/vmmc%d%d", 
				curr_chan->parent->idx, curr_chan->idx);

		fd_chan = open(dev_node, O_RDWR);
		if (fd_chan==-1){
			ab_err_set(AB_ERR_NO_FILE, "opening vmmc channel node");
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
	
	if (tapi_dev_start(ab) != AB_ERR_NO_ERR) {
		/* tapi_dev_start sets the error message */
		ab_g_err_idx = AB_ERR_UNKNOWN;
		goto __free_and_exit_fail;
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
#if 0
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
#endif

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
		tapi_dev_stop(ab_tmp);
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

#if 0
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
#endif

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

