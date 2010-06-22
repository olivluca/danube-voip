/* INCLUDE {{{*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/kdev_t.h>
#include <errno.h>

#include "ab_api.h"
#include "drv_tapi_io.h"	/* from TAPI_HL_driver */
#include "vinetic_io.h" 	/* from Vinetic_LL_driver */
#include "ab_ioctl.h"		/* from ATA_Board_driver */
/*}}}*/
/* DEFINE {{{*/
#define AB_SGATAB_DEV_NODE "/dev/sgatab"
#define AB_SGATAB_MAJOR_FILE "/proc/driver/sgatab/major"
#define VIN_DEV_NODE_PREFIX "/dev/vin"
#define VINETIC_MAJOR 121
#define FXO_OSI_MAX 600
/*}}}*/
/*{{{ Global VARS */
static unsigned char * fw_pram = NULL;
static unsigned char * fw_dram = NULL;
static unsigned char * fw_cram_fxs = NULL;
static unsigned char * fw_cram_fxo = NULL;
static unsigned char * fw_cram_vfn2 = NULL;
static unsigned char * fw_cram_vfn4 = NULL;
static unsigned char * fw_cram_vft2 = NULL;
static unsigned char * fw_cram_vft4 = NULL;
static unsigned long fw_pram_size = 0;
static unsigned long fw_dram_size = 0;
static unsigned long fw_cram_fxs_size = 0;
static unsigned long fw_cram_fxo_size = 0;
static unsigned long fw_cram_vfn2_size = 0;
static unsigned long fw_cram_vfn4_size = 0;
static unsigned long fw_cram_vft2_size = 0;
static unsigned long fw_cram_vft4_size = 0;
static int g_flags = 0;
static enum vf_type_e * g_types = NULL;
/*}}}*/
/*{{{ Global FUNCTIONS */
static int voip_in_slots( void );
static int load_modules( void );
static int init_voip( void );

static int board_iterator (int (*func)(ab_board_params_t const * const bp));
static int create_vinetic_nodes (void);
static int create_vin_board (ab_board_params_t const * const bp);
static int board_init (ab_board_params_t const * const bp);
static int dev_init(int const dev_idx, ab_dev_params_t const * const dp, 
		long const nIrqNum);
static int basicdev_init( int const dev_idx, ab_dev_params_t const * const dp, 
		long const nIrqNum);
static int chan_init (int const dev_idx, int const chan_idx, 
		dev_type_t const dt, int const abs_chan_idx);
static int chan_init_tune (int const rtp_fd, int const chan_idx, 
		int const dev_idx, dev_type_t const dtype);
static int pd_ram_load( void );
static int cram_fxs_load( void );
static int cram_fxo_load( void );
static int cram_vf_load( enum vf_type_e const type );
static int fw_masses_init_from_path (unsigned char ** const fw_buff, 
		unsigned long * const buff_size, char const * const path );
static void fw_masses_free( void );

static void get_last_err(int fd);
/*}}}*/

int 
ab_hardware_init( enum vf_type_e * const types, int const flags )
{/*{{{*/
	int err = 0;

	g_flags = flags;
	/* tag__ types should be a pointer to mas[CHANS_MAX] how make it better? */
	g_types = types;

	/* test the voip modules presence */
	err = voip_in_slots();
	if(err){
		goto __exit_fail;
	}

	/* load drivers stack and create node files */
	if( !(g_flags & AB_HWI_SKIP_MODULES)){
		err = load_modules();
		if(err){
			goto __exit_fail;
		}
	}

	/* init devices and channels */
	err = init_voip();
	if(err){
		goto __exit_fail;
	}

	return 0;
__exit_fail:
	return -1;
}/*}}}*/

static int 
voip_in_slots( void )
{/*{{{*/
	int err;
	err = system("cat /proc/pci | grep 0055:009c > /dev/null || exit 1");
	if(err){
		ab_g_err_idx = AB_ERR_NO_HARDWARE;
		sprintf(ab_g_err_str, "%s() ERROR : can`t find modules",__func__);
	}
	return err;
}/*}}}*/

static int 
load_modules( void )
{/*{{{*/
	char sgatab_major_str[20];
	int sgatab_major;
	int fd;
	mode_t uma;
	int err;

	uma = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;

	/* remove all modules and they dev nodes */
	system("modprobe -r drv_sgatab");
	system("modprobe -r drv_daa");
	system("modprobe -r drv_vinetic");
	system("modprobe -r drv_tapi");
	system("rm -f "VIN_DEV_NODE_PREFIX"*");
	remove(AB_SGATAB_DEV_NODE);

	/* load sgatab module */
	err = system("modprobe drv_sgatab");
	if(err){
		ab_g_err_idx = AB_ERR_UNKNOWN;
		sprintf(ab_g_err_str, "%s() ERROR : drv_sgatab load",__func__);
		goto __exit_fail;
	}

	/* create sgatab dev node */
	fd = open(AB_SGATAB_MAJOR_FILE, O_RDONLY);
	if(fd == -1){
		ab_g_err_idx = AB_ERR_NO_FILE;
		sprintf(ab_g_err_str, "%s() ERROR : drv_sgatab major file open",
				__func__);
		goto __exit_fail;
	}
	err = read(fd, sgatab_major_str, sizeof(sgatab_major_str));
	close(fd);
	if(!err){
		ab_g_err_idx = AB_ERR_UNKNOWN;
		sprintf(ab_g_err_str, "%s() ERROR : drv_sgatab major file empty",
				__func__);
		goto __exit_fail;
	} else if(err == -1){
		ab_g_err_idx = AB_ERR_UNKNOWN;
		sprintf(ab_g_err_str, "%s() ERROR : reading drv_sgatab major "
				"file : \n\t%s", __func__, strerror(errno));
		goto __exit_fail;
	}

	sgatab_major = strtol(sgatab_major_str, NULL, 10);
	err = mknod(AB_SGATAB_DEV_NODE, S_IFCHR | uma, MKDEV(sgatab_major,0));
	if(err){
		ab_g_err_idx = AB_ERR_UNKNOWN;
		sprintf(ab_g_err_str, "%s() ERROR : drv_sgatab dev file create :"
				"%s", __func__, strerror(errno));
		goto __exit_fail;
	}

	/* load infineon modules stack */
	err = system("modprobe drv_tapi");
	if(err){
		ab_g_err_idx = AB_ERR_UNKNOWN;
		sprintf(ab_g_err_str, "%s() ERROR : drv_tapi load",__func__ );
		goto __exit_fail;
	}
	err = system("modprobe drv_vinetic");
	if(err){
		ab_g_err_idx = AB_ERR_UNKNOWN;
		sprintf(ab_g_err_str, "%s() ERROR : drv_vinetic load",__func__ );
		goto __exit_fail;
	}
	err = system("modprobe drv_daa");
	if(err){
		ab_g_err_idx = AB_ERR_UNKNOWN;
		sprintf(ab_g_err_str, "%s() ERROR : drv_daa load",__func__ );
		goto __exit_fail;
	}

	/* create vinetic nodefiles */
	err = create_vinetic_nodes();
	if(err){
		goto __exit_fail;
	}

	return 0;
__exit_fail:
	return -1;
}/*}}}*/

static int
create_vinetic_nodes( void )
{/*{{{*/
	int err;
	err = board_iterator(create_vin_board);
	if(err){
		goto __exit_fail;
	}
	return 0;
__exit_fail:
	return -1;
}/*}}}*/

static int 
init_voip( void )
{/*{{{*/
	int err;
	err = board_iterator(board_init);
	if(err){
		goto __exit_fail;
	}
	fw_masses_free();
	return 0;
__exit_fail:
	fw_masses_free();
	return -1;
}/*}}}*/

static int
board_iterator (int (*func)(ab_board_params_t const * const bp))
{/*{{{*/
	ab_board_params_t bp;
	int bc;
	int fd;
	int i;
	int err;

	static int boards_count_changed_tag = 0;
	/*  0 - first run
	 *  >0 && !=bc - count of boards changes 
	 * */

	fd = open(AB_SGATAB_DEV_NODE, O_RDWR);
	if(fd==-1){
		ab_g_err_idx = AB_ERR_NO_FILE;
		sprintf(ab_g_err_str, "%s() ERROR : drv_sgatab dev file open",
				__func__ );
		goto __exit_fail;
	}

	err = ioctl(fd, SGAB_GET_BOARDS_COUNT, &bc);
	if(err){
		ab_g_err_idx = AB_ERR_UNKNOWN;
		sprintf(ab_g_err_str, "%s() ERROR : SGAB_GET_BOARDS_COUNT ioctl",
				__func__ );
		goto __exit_fail_close;
	}

	if( boards_count_changed_tag == 0){
		/*first run of board_iterator()*/
		boards_count_changed_tag = bc;
	} else if(boards_count_changed_tag != bc){
		/* not first run & count of boards changed */
		ab_g_err_idx = AB_ERR_UNKNOWN;
		sprintf(ab_g_err_str, "%s() ERROR : count of boards changed",
				__func__ );
		goto __exit_fail_close;
	}

	/* do it for all boards */
	for (i=0; i<bc; i++){
		/* get board params */
		memset(&bp, 0, sizeof(bp));
		bp.board_idx = i;
		err = ioctl(fd, SGAB_GET_BOARD_PARAMS, &bp);
		if(err){
			ab_g_err_idx = AB_ERR_UNKNOWN;
			sprintf(ab_g_err_str, "%s() ERROR : SGAB_GET_BOARD_PARAMS (ioctl)",
					__func__ );
			goto __exit_fail_close;
		} else if((!bp.is_present) || bp.is_count_changed){
			ab_g_err_idx = AB_ERR_UNKNOWN;
			sprintf(ab_g_err_str, "%s() ERROR : SGAB_GET_BOARD_PARAMS"
					" boards count changed", __func__ );
			goto __exit_fail_close;
		}

		/* execute func on current board */
		err = func (&bp);
		if(err){
			goto __exit_fail_close;
		}
	}

	close (fd);
	return 0;
__exit_fail_close:
	close (fd);
__exit_fail:
	return -1;
}/*}}}*/

static int 
board_init (ab_board_params_t const * const bp)
{/*{{{*/
	int i;
	int err;
	static int dev_idx = 0;

	for(i=0; i<DEVS_PER_BOARD_MAX; i++){
		if(bp->devices[i].type != dev_type_ABSENT){
			/* init device and it`s channels */
			err = dev_init (dev_idx, &bp->devices[i], bp->nIrqNum);
			if(err){
				goto __exit_fail;
			}
			dev_idx++;
		}
	}
	return 0;
__exit_fail:
	return -1;
}/*}}}*/

static int 
dev_init (int const dev_idx, ab_dev_params_t const * const dp, long const nIrqNum)
{/*{{{*/
	int j;
	int err;

	/* dev basic init */
	err = basicdev_init(dev_idx, dp, nIrqNum);
	if(err){
		goto __exit_fail;
	}


	if(g_flags & AB_HWI_SKIP_CHANNELS){
		goto __exit_success;
	}

	/* download fw and chans init */
	for(j=0; j<CHANS_PER_DEV; j++){
		/* channel init */
		err = chan_init (dev_idx, j, dp->type, dp->chans_idx[j]);
		if(err){
			goto __exit_fail;
		}
	}

__exit_success:
	return 0;
__exit_fail:
	return -1;
}/*}}}*/

static int 
basicdev_init( int const dev_idx, ab_dev_params_t const * const dp, 
		long const nIrqNum)
{ /*{{{*/
	IFX_int32_t err;
	VINETIC_BasicDeviceInit_t binit;
	int cfg_fd;
	char dev_node[ 50 ];

	if(g_flags & AB_HWI_SKIP_DEVICES){
		goto __exit_success;
	}

	memset(&binit, 0, sizeof(binit));
	binit.AccessMode = dp->AccessMode;
	binit.nBaseAddress = dp->nBaseAddress;
	binit.nIrqNum = nIrqNum;

	sprintf(dev_node,VIN_DEV_NODE_PREFIX"%d0", dev_idx+1);

	cfg_fd = open(dev_node, O_RDWR);
	if(cfg_fd==-1){
		ab_g_err_idx = AB_ERR_NO_FILE;
		sprintf(ab_g_err_str, "%s() opening vinetic device node '%s'",
				__func__, dev_node);
		goto __exit_fail;
	}

	err = ioctl(cfg_fd, FIO_VINETIC_DEV_RESET, 0);
	if(err){
		ab_g_err_idx = AB_ERR_UNKNOWN;
		sprintf(ab_g_err_str, "%s() device reset fails (ioctl)",__func__);
		get_last_err(cfg_fd);
		goto __exit_fail_close;
	}

	err = ioctl (cfg_fd, FIO_VINETIC_BASICDEV_INIT, &binit);
	if(err){
		ab_g_err_idx = AB_ERR_UNKNOWN;
		sprintf(ab_g_err_str, "%s() BasicDev init fails (ioctl)",__func__);
		get_last_err(cfg_fd);
		goto __exit_fail_close;
	}

	close (cfg_fd);
__exit_success:
	return 0;
__exit_fail_close:
	close (cfg_fd);
__exit_fail:
	return -1;
}/*}}}*/

static int
chan_init(int const dev_idx, int const chan_idx, dev_type_t const dt,
		int const abs_chan_idx)
{/*{{{*/
	IFX_TAPI_CH_INIT_t init;
	VINETIC_IO_INIT vinit;
	struct bbd_format_s {
		unsigned char * buf;
		unsigned long size;
	} bbd_download;
	char cnode[ 50 ];
	int cfd;
	int err;

	/* Initialize channel */
	sprintf(cnode, "/dev/vin%d%d", dev_idx+1, chan_idx+1);

	cfd = open(cnode, O_RDWR);
	if (cfd==-1){
		ab_g_err_idx = AB_ERR_NO_FILE;
		sprintf(ab_g_err_str,"%s() opening vinetic channel node",__func__);
		goto __exit_fail;
	}

	memset(&vinit, 0, sizeof(vinit));
	memset(&bbd_download, 0, sizeof (bbd_download));

	err = pd_ram_load();
	if(err){
		goto __exit_fail_close;
	}

	vinit.pPRAMfw   = fw_pram;
	vinit.pram_size = fw_pram_size;
	vinit.pDRAMfw   = fw_dram;
	vinit.dram_size = fw_dram_size;

	if(dt==dev_type_FXO){
		err = cram_fxo_load();
		if(err){
			goto __exit_fail_close;
		}
		vinit.pBBDbuf  = fw_cram_fxo;
		vinit.bbd_size = fw_cram_fxo_size;
	} else if(dt==dev_type_FXS){
		err = cram_fxs_load();
		if(err){
			goto __exit_fail_close;
		}
		vinit.pBBDbuf   = fw_cram_fxs;
		vinit.bbd_size  = fw_cram_fxs_size;
	} else if(dt==dev_type_VF){
		err = cram_vf_load (g_types[abs_chan_idx]);
		if(err){
			goto __exit_fail_close;
		}
		if        (g_types[abs_chan_idx] == vf_type_N2){
			vinit.pBBDbuf  = fw_cram_vfn2;
			vinit.bbd_size = fw_cram_vfn2_size;
		} else if (g_types[abs_chan_idx] == vf_type_N4){
			vinit.pBBDbuf  = fw_cram_vfn4;
			vinit.bbd_size = fw_cram_vfn4_size;
		} else if (g_types[abs_chan_idx] == vf_type_T2){
			vinit.pBBDbuf  = fw_cram_vft2;
			vinit.bbd_size = fw_cram_vft2_size;
		} else if (g_types[abs_chan_idx] == vf_type_T4){
			vinit.pBBDbuf  = fw_cram_vft4;
			vinit.bbd_size = fw_cram_vft4_size;
		}
	}

	bbd_download.buf  = vinit.pBBDbuf;
	bbd_download.size = vinit.bbd_size;

	/* Set the pointer to the VINETIC dev specific init structure */
	memset(&init, 0, sizeof(init));
	init.pProc = (IFX_void_t *) &vinit;
	/* Init the VoIP application */
	init.nMode = IFX_TAPI_INIT_MODE_VOICE_CODER;

	/* Initialize channel */

	err = ioctl(cfd, IFX_TAPI_CH_INIT, &init);
	if( err ){
		ab_g_err_idx = AB_ERR_UNKNOWN;
		sprintf(ab_g_err_str,"%s() initilizing channel with firmware (ioctl)",
				__func__);
		get_last_err(cfd);
		goto __exit_fail_close;
	}

	err = ioctl(cfd, FIO_VINETIC_BBD_DOWNLOAD, &bbd_download);
	if( err ){
		ab_g_err_idx = AB_ERR_UNKNOWN;
		sprintf(ab_g_err_str,"%s() initilizing channel(bbd) with firmware (ioctl)",
				__func__);
		get_last_err(cfd);
		goto __exit_fail_close;
	}

	err = chan_init_tune( cfd, chan_idx, dev_idx, dt );
	if( err ){
		goto __exit_fail_close;
	}

	close(cfd);
	return 0;
__exit_fail_close:
	close(cfd);
__exit_fail:
	return -1;
}/*}}}*/

static int 
chan_init_tune( int const rtp_fd, int const chan_idx, int const dev_idx,
		dev_type_t const dtype )
{/*{{{*/
	IFX_TAPI_MAP_DATA_t datamap;
	IFX_TAPI_LINE_TYPE_CFG_t lineTypeCfg;
	IFX_TAPI_SIG_DETECTION_t dtmfDetection;
	IFX_TAPI_SIG_DETECTION_t faxSig;
	int err = 0;

	memset(&datamap, 0, sizeof (datamap));
	memset(&lineTypeCfg, 0, sizeof (lineTypeCfg));

	/* Set channel type */	
	if(dtype == dev_type_FXS) {
		lineTypeCfg.lineType = IFX_TAPI_LINE_TYPE_FXS_NB;
	} else if(dtype == dev_type_FXO) {
		lineTypeCfg.lineType = IFX_TAPI_LINE_TYPE_FXO_NB;
		lineTypeCfg.nDaaCh = dev_idx * CHANS_PER_DEV + chan_idx;
	} else if(dtype == dev_type_VF){
		lineTypeCfg.lineType = IFX_TAPI_LINE_TYPE_VF;
	}
	err = ioctl (rtp_fd, IFX_TAPI_LINE_TYPE_SET, &lineTypeCfg);
	if( err ){
		ab_g_err_idx = AB_ERR_UNKNOWN;
		strcpy(ab_g_err_str, "setting channel type (ioctl)" );
		get_last_err(rtp_fd);
		goto ab_chan_init_tune__exit;
	} 

	/* Map channels */	
	datamap.nDstCh = chan_idx; 
	datamap.nChType = IFX_TAPI_MAP_TYPE_PHONE;
	err = ioctl(rtp_fd, IFX_TAPI_MAP_DATA_ADD, &datamap);
	if( err ){
		ab_g_err_idx = AB_ERR_UNKNOWN;
		strcpy(ab_g_err_str, "mapping channel to it`s own data (ioctl)");
		get_last_err(rtp_fd);
		goto ab_chan_init_tune__exit;
	} 

	if(dtype == dev_type_FXS) {
		char data[10] = {0xFF,0xFF,0xF0,0,0,0,0,0,0,0};
		IFX_TAPI_RING_CADENCE_t ringCadence;

		/* ENABLE detection of DTMF tones 
		 * from local interface (ALM X) */
		memset(&dtmfDetection, 0, sizeof (dtmfDetection));
		dtmfDetection.sig = IFX_TAPI_SIG_DTMFTX;
		err = ioctl (rtp_fd,IFX_TAPI_SIG_DETECT_ENABLE,&dtmfDetection);
		if( err ){
			ab_g_err_idx = AB_ERR_UNKNOWN;
			strcpy(ab_g_err_str, "trying to enable DTMF ALM signal "
					"detection (ioctl)" );
			get_last_err(rtp_fd);
			goto ab_chan_init_tune__exit;
		}
		/* configure ring candence */
		memset(&ringCadence, 0, sizeof(ringCadence));
		memcpy(&ringCadence.data, data, sizeof(data));
		ringCadence.nr = sizeof(data) * 8;
		err = ioctl(rtp_fd, IFX_TAPI_RING_CADENCE_HR_SET, &ringCadence);
		if( err ){
			ab_g_err_idx = AB_ERR_UNKNOWN;
			strcpy(ab_g_err_str, "trying to configure ring cadence (ioctl)");
			get_last_err(rtp_fd);
			goto ab_chan_init_tune__exit;
		}
	} else if(dtype == dev_type_FXO) {
		/* DISABLE detection of DTMF tones 
		 * from local interface (ALM X) */
		IFX_TAPI_FXO_OSI_CFG_t osi_cfg;
		memset(&dtmfDetection, 0, sizeof (dtmfDetection));
		dtmfDetection.sig = IFX_TAPI_SIG_DTMFTX;
		err = ioctl (rtp_fd,IFX_TAPI_SIG_DETECT_DISABLE,&dtmfDetection);
		if( err ){
			ab_g_err_idx = AB_ERR_UNKNOWN;
			strcpy(ab_g_err_str, "trying to disable DTMF ALM signal "
					"detection (ioctl)" );
			get_last_err(rtp_fd);
			goto ab_chan_init_tune__exit;
		}
		/* set OSI timing */
		memset(&osi_cfg, 0, sizeof (osi_cfg));
		osi_cfg.nOSIMax = FXO_OSI_MAX;
		err = ioctl(rtp_fd, IFX_TAPI_FXO_OSI_CFG_SET, &osi_cfg);
		if( err ){
			ab_g_err_idx = AB_ERR_UNKNOWN;
			strcpy(ab_g_err_str, "trying to set OSI timing (ioctl)" );
			get_last_err(rtp_fd);
			goto ab_chan_init_tune__exit;
		}
	}
	if(dtype == dev_type_FXO || dtype == dev_type_FXS){
		/* ENABLE detection of FAX signals */
		memset (&faxSig, 0, sizeof(faxSig));
		faxSig.sig = IFX_TAPI_SIG_CEDRX | IFX_TAPI_SIG_CEDTX |
			IFX_TAPI_SIG_CEDENDRX | IFX_TAPI_SIG_CEDENDTX;
		err = ioctl (rtp_fd,IFX_TAPI_SIG_DETECT_ENABLE,&faxSig);
		if(err){
			ab_g_err_idx = AB_ERR_UNKNOWN;
			strcpy(ab_g_err_str, "trying to enable FAX signal detection (ioctl)" );
			get_last_err(rtp_fd);
			goto ab_chan_init_tune__exit;
		}
	}
	/* for VF do not enable any signal detections 
	 * tag__ may be it should be desabled manually - should test.
	 * */
	return 0;
ab_chan_init_tune__exit:
	return -1;
};/*}}}*/

static int
pd_ram_load( void )
{/*{{{*/
	int err;
	if( !fw_pram){
		err = fw_masses_init_from_path (&fw_pram, &fw_pram_size, 
				AB_FW_PRAM_NAME );
		if(err){
			goto __exit_fail;
		}
	}
	if( !fw_dram){
		err = fw_masses_init_from_path (&fw_dram, &fw_dram_size, 
				AB_FW_DRAM_NAME );
		if(err){
			goto __exit_fail;
		}
	}
	return 0; 
__exit_fail:
	return -1;
}/*}}}*/

static int
cram_fxs_load( void )
{/*{{{*/
	int err;
	if( !fw_cram_fxs){
		err = fw_masses_init_from_path (&fw_cram_fxs, &fw_cram_fxs_size, 
				AB_FW_CRAM_FXS_NAME);
		if(err){
			goto __exit_fail;
		}
	}
	return 0; 
__exit_fail:
	return -1;
}/*}}}*/

static int
cram_fxo_load( void )
{/*{{{*/
	int err;
	if( !fw_cram_fxo){
		err = fw_masses_init_from_path (&fw_cram_fxo, &fw_cram_fxo_size, 
				AB_FW_CRAM_FXO_NAME);
		if(err){
			goto __exit_fail;
		}
	}
	return 0; 
__exit_fail:
	return -1;
}/*}}}*/

static int
cram_vf_load( enum vf_type_e const type )
{/*{{{*/
	int err;
	char const * vf_name;
	unsigned char ** vf_mas;
	unsigned long * vf_size_ptr;

	if        (type == vf_type_N2){
		vf_name     = AB_FW_CRAM_VFN2_NAME;
		vf_mas      = &fw_cram_vfn2;
		vf_size_ptr = &fw_cram_vfn2_size;
	} else if (type == vf_type_N4){
		vf_name     = AB_FW_CRAM_VFN4_NAME;
		vf_mas      = &fw_cram_vfn4;
		vf_size_ptr = &fw_cram_vfn4_size;
	} else if (type == vf_type_T2){
		vf_name     = AB_FW_CRAM_VFT2_NAME;
		vf_mas      = &fw_cram_vft2;
		vf_size_ptr = &fw_cram_vft2_size;
	} else if (type == vf_type_T4){
		vf_name     = AB_FW_CRAM_VFT4_NAME;
		vf_mas      = &fw_cram_vft4;
		vf_size_ptr = &fw_cram_vft4_size;
	}
	if( !(*vf_mas)){
		err = fw_masses_init_from_path (vf_mas, vf_size_ptr, vf_name);
		if(err){
			goto __exit_fail;
		}
	}
	return 0; 
__exit_fail:
	return -1;
}/*}}}*/

static int 
fw_masses_init_from_path (unsigned char ** const fw_buff, 
		unsigned long * const buff_size, char const * const path )
{/*{{{*/
	int fd;

	fd = open(path, O_RDONLY);
	if( fd <= 0 ) {
		ab_g_err_idx = AB_ERR_NO_FILE;
		strcpy(ab_g_err_str, "Opening firmware file");
		goto fw_masses_init_from_path__exit;
	}
	*buff_size = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);

	(*fw_buff) = malloc(*buff_size);
	if( ! (*fw_buff) ){
		ab_g_err_idx = AB_ERR_NO_MEM;
		goto fw_masses_init_from_path__open_file;
	}

	if(read(fd, (*fw_buff), *buff_size) != *buff_size){
		ab_g_err_idx = AB_ERR_NO_FILE;
		strcpy(ab_g_err_str, "Reading firmware file");
		goto fw_masses_init_from_path__alloc_mem;
	}

	close(fd);
	return 0;

fw_masses_init_from_path__alloc_mem:
	free((char*)(*fw_buff));
	(*fw_buff) = NULL;
fw_masses_init_from_path__open_file:
	close(fd);
	*buff_size = 0;
fw_masses_init_from_path__exit:
	return -1;
}/*}}}*/

static void 
fw_masses_free( void )
{/*{{{*/
	if(fw_pram) {
		free(fw_pram);
		fw_pram_size = 0;
		fw_pram = NULL;
	}
	if(fw_dram) {
		free(fw_dram);
		fw_dram_size = 0;
		fw_dram = NULL;
	}
	if(fw_cram_fxs) {
		free(fw_cram_fxs);
		fw_cram_fxs_size = 0;
		fw_cram_fxs = NULL;
	}
	if(fw_cram_fxo) {
		free(fw_cram_fxo);
		fw_cram_fxo_size = 0;
		fw_cram_fxo = NULL;
	}
}/*}}}*/

static int 
create_vin_board (ab_board_params_t const * const bp)
{/*{{{*/
	int i;
	int err;
	mode_t uma;
	static int dev_idx = 0;

	uma = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;

	for(i=0; i<DEVS_PER_BOARD_MAX; i++){
		if(bp->devices[i].type != dev_type_ABSENT){
			int j;
			char dev_name[50];
			memset(dev_name, 0, sizeof(dev_name));
			snprintf (dev_name, sizeof(dev_name), "%s%d0",
					VIN_DEV_NODE_PREFIX, dev_idx+1);
			err = mknod(dev_name, S_IFCHR|uma,MKDEV(VINETIC_MAJOR,
					(dev_idx+1)*10));
			if(err){
				ab_g_err_idx = AB_ERR_UNKNOWN;
				sprintf(ab_g_err_str, "%s() ERROR : drv_vinetic dev file "
						"create :%s", __func__, strerror(errno));
				goto __exit_fail;
			}
			for(j=0; j<CHANS_PER_DEV; j++){
				char ch_name[50];
				memset(ch_name, 0, sizeof(ch_name));
				snprintf (ch_name, sizeof(ch_name), "%s%d%d",
						VIN_DEV_NODE_PREFIX, dev_idx+1, j+1);
				err = mknod(ch_name, S_IFCHR|uma, MKDEV(
						VINETIC_MAJOR,(dev_idx+1)*10+j+1));
				if(err){
					ab_g_err_idx = AB_ERR_UNKNOWN;
					sprintf(ab_g_err_str, "%s() ERROR : drv_vinetic chan "
							"file create :%s", __func__, strerror(errno));
					goto __exit_fail;
				}
			}
			dev_idx++;
		}
	}
	return 0;
__exit_fail:
	return -1;
}/*}}}*/

static void
get_last_err(int fd)
{/*{{{*/
	int error;
	ioctl (fd, FIO_VINETIC_LASTERR, &error);
	ab_g_err_extra_value = error;
}/*}}}*/

static int
dev_gpio_reset (ab_dev_t const * const dev)
{/*{{{*/
	/* Configure 3/7 pins as output and set it to out values. */
	VINETIC_IO_GPIO_CONTROL gpio;
	ab_t * ab;
	int fd_dev;
	int out;
	int i;
	int j;
	int err;

	memset(&gpio, 0, sizeof(gpio));

	ab = dev->parent;
	fd_dev = dev->cfg_fd;

	j = ab->chans_num;
	for (out=0,i=0; i<j; i++){
		enum vf_type_e tp = ab->chans[i].type_if_vf;
		if((ab->chans[i].parent == dev)/*our device*/ &&
				(tp == vf_type_N2 || tp == vf_type_T2) /*should set 1*/){
			out |= (ab->chans[i].idx == 1) ? VINETIC_IO_DEV_GPIO_3:VINETIC_IO_DEV_GPIO_7;
		}
	}

	/* Reserve the pins 3/7, for exclusive access */
	gpio.nGpio = VINETIC_IO_DEV_GPIO_3 | VINETIC_IO_DEV_GPIO_7;
	err = ioctl(fd_dev, FIO_VINETIC_GPIO_RESERVE, (IFX_int32_t) &gpio);
	if(err){
		ab_g_err_idx = AB_ERR_UNKNOWN;
		sprintf(ab_g_err_str, "%s() ERROR : FIO_VINETIC_GPIO_RESERVE",__func__);
		goto __exit_fail;
	}
	/* now gpio contains the iohandle required for subsequent accesses */
	/* select pins 3,7 --> set to ’1’*/
	/* Configure them as output - doc=0.. src=1*/
	/* nGpio and nMask revert in doc */
	gpio.nGpio = VINETIC_IO_DEV_GPIO_3 | VINETIC_IO_DEV_GPIO_7;
	gpio.nMask = VINETIC_IO_DEV_GPIO_3 | VINETIC_IO_DEV_GPIO_7;
	err = ioctl(fd_dev, FIO_VINETIC_GPIO_CONFIG, &gpio);
	if(err){
		ab_g_err_idx = AB_ERR_UNKNOWN;
		sprintf(ab_g_err_str, "%s() ERROR : FIO_VINETIC_GPIO_CONFIG",__func__);
		goto __exit_fail_release;
	}

	/* nMask: select pins 3,7 --> set to ’1’ or '0' */
	gpio.nMask = VINETIC_IO_DEV_GPIO_3 | VINETIC_IO_DEV_GPIO_7;
	/* nGpio: pins 3 and 7 to out value */
	gpio.nGpio = out;
	err = ioctl(fd_dev, FIO_VINETIC_GPIO_SET, &gpio);
	if(err){
		ab_g_err_idx = AB_ERR_UNKNOWN;
		sprintf(ab_g_err_str, "%s() ERROR : FIO_VINETIC_GPIO_SET",__func__);
		goto __exit_fail_release;
	}

	/* release GPIO pins */
	err = ioctl(fd_dev, FIO_VINETIC_GPIO_RELEASE, &gpio);
	if(err){
		ab_g_err_idx = AB_ERR_UNKNOWN;
		sprintf(ab_g_err_str, "%s() ERROR : FIO_VINETIC_GPIO_RELEASE",__func__);
		goto __exit_fail_release;
	}

	return 0;

__exit_fail_release:
	ioctl(fd_dev, FIO_VINETIC_GPIO_RELEASE, &gpio);
__exit_fail:
	return -1;
}/*}}}*/

int 
ab_devs_vf_gpio_reset (ab_t const * const ab)
{/*{{{*/
	/* go through all devices and init gpio for VF channels */
	int i;
	for (i=0; i<ab->devs_num; i++){
		if(ab->devs[i].type == ab_dev_type_VF){
			if(dev_gpio_reset(&(ab->devs[i]))){
				goto __exit_fail;
			}
		}
	} 
	return 0;
__exit_fail:
	return -1;
}/*}}}*/
