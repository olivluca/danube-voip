#ifndef __AB_API_H__
#define __AB_API_H__

/** Maximum channels on the boards */
#define CHANS_MAX 32
/*{{{ typedefs */
typedef enum ab_dev_type_e ab_dev_type_t;
typedef enum cod_type_e cod_type_t;
typedef enum cid_std_e cid_std_t;
typedef struct codec_s codec_t;
typedef struct wlec_s wlec_t;
typedef struct jb_prms_s jb_prms_t;
typedef struct rtp_session_prms_s rtp_session_prms_t;
typedef struct ab_chan_s ab_chan_t;
typedef struct ab_dev_s ab_dev_t;
typedef struct ab_s ab_t;
typedef struct ab_fw_s ab_fw_t;
typedef struct ab_dev_event_s ab_dev_event_t;
/*}}}*/

enum jb_type_e {/*{{{*/
	jb_type_FIXED, 
	jb_type_ADAPTIVE
};/*}}}*/
enum jb_loc_adpt_e {/*{{{*/
	jb_loc_adpt_OFF, 
	jb_loc_adpt_ON, 
//	jb_loc_adpt_SI /**< local adaptation on with sample interpollation */
};/*}}}*/
struct jb_prms_s {/*{{{*/
	enum jb_type_e     jb_type;		/**< JB type */
	enum jb_loc_adpt_e jb_loc_adpt; /**< JB local adaptation type */
	char jb_scaling; /**< scaling value*16 [16;255], increase -> more delay */
	unsigned short jb_init_sz; /**< initial buffer size */
	unsigned short jb_min_sz; /**< minimal buffer size */
	unsigned short jb_max_sz; /**< maximum buffer size */
};/*}}}*/
enum cod_type_e {/*{{{*/
	cod_type_NONE,
	cod_type_G722_64,
	cod_type_ALAW,
	cod_type_G729,
	cod_type_G729E,
	cod_type_ILBC_133,
	cod_type_G723,
	cod_type_G726_16,
	cod_type_G726_24,
	cod_type_G726_32,
	cod_type_G726_40,
};/*}}}*/
enum cod_pkt_size_e {/*{{{*/
	cod_pkt_size_2_5,
	cod_pkt_size_5,
	cod_pkt_size_5_5,
	cod_pkt_size_10,
	cod_pkt_size_11,
	cod_pkt_size_20,
	cod_pkt_size_30,
	cod_pkt_size_40,
	cod_pkt_size_50,
	cod_pkt_size_60,
};/*}}}*/
enum bitpack_e {/*{{{*/
	bitpack_RTP,
	bitpack_AAL2,
};/*}}}*/
enum cid_std_e {/*{{{*/
	cid_OFF,
	cid_TELCORDIA,
	cid_ETSI_FSK,
	cid_ETSI_DTMF,
	cid_SIN,
	cid_NTT,
	cid_KPN_DTMF,
	cid_KPN_DTMF_FSK
};/*}}}*/
struct codec_s {/*{{{*/
	enum cod_type_e 	type;
	enum cod_pkt_size_e pkt_size;
	enum bitpack_e bpack;
	int user_payload;			/**< User preset to sdp payload. */
	int sdp_selected_payload;	/**< Selected in sdp session payload. */
	struct jb_prms_s jb;		/**< Jitter buffer parameters for this codec. */
};/*}}}*/
enum wlec_mode_e {/*{{{*/
	wlec_mode_UNDEF,
	wlec_mode_OFF,
	wlec_mode_NE,
	wlec_mode_NFE,
};/*}}}*/
enum wlec_nlp_e {/*{{{*/
	wlec_nlp_ON,
	wlec_nlp_OFF,
};/*}}}*/
enum wlec_window_size_e {/*{{{*/
	wlec_window_size_4 = 4,
	wlec_window_size_6 = 6,
	wlec_window_size_8 = 8,
	wlec_window_size_16 = 16,
};/*}}}*/
struct wlec_s {/*{{{*/
	enum wlec_mode_e mode;
	enum wlec_nlp_e nlp;
	enum wlec_window_size_e ne_nb;
	enum wlec_window_size_e fe_nb;
	enum wlec_window_size_e ne_wb;
};/*}}}*/
enum vad_cfg_e {/*{{{*/
	vad_cfg_OFF,
	vad_cfg_ON,
	vad_cfg_G711,
	vad_cfg_CNG_only,
	vad_cfg_SC_only
};/*}}}*/
struct rtp_session_prms_s {/*{{{*/
	int enc_dB; /**< Coder enc gain */
	int dec_dB; /**< Coder dec gain */
	enum vad_cfg_e VAD_cfg; /**< Voice Activity Detector configuration */
	unsigned char HPF_is_ON; /**< High Pass Filter is ON? */
};/*}}}*/
enum ab_dev_type_e {/*{{{*/
	ab_dev_type_FXO,   /**< Device type is FXO */
	ab_dev_type_FXS,   /**< Device type is FXS */
	ab_dev_type_VF     /**< Device type is Tonal Frequency */
};/*}}}*/
enum ab_chan_tone_e {/*{{{*/
	ab_chan_tone_MUTE, /**< Mute any tone */
	ab_chan_tone_DIAL,   /**< Play dial tone */
	ab_chan_tone_BUSY,   /**< Play busy tone */
	ab_chan_tone_RINGBACK   /**< Play ringback tone */
};/*}}}*/
enum ab_chan_ring_e {/*{{{*/
	ab_chan_ring_MUTE, /**< Mute the ring */
	ab_chan_ring_RINGING   /**< Make ring */
};/*}}}*/
enum ab_chan_hook_e {/*{{{*/
	ab_chan_hook_ONHOOK, /**< onhook state */
	ab_chan_hook_OFFHOOK /**< offhook state */
};/*}}}*/
enum ab_chan_linefeed_e {/*{{{*/
	ab_chan_linefeed_DISABLED, /**< Set linefeed to disabled */
	ab_chan_linefeed_STANDBY, /**< Set linefeed to standby */
	ab_chan_linefeed_ACTIVE /**< Set linefeed to active */
};/*}}}*/
enum ab_dev_event_e {/*{{{*/
	ab_dev_event_NONE, /**< No event */
	ab_dev_event_UNCATCHED, /**< Unknown event */
	ab_dev_event_FXO_RINGING, /**< Ring on FXO */
	ab_dev_event_FXS_DIGIT_TONE, /**< Dial a digit on FXS in tone mode */
	ab_dev_event_FXS_DIGIT_PULSE, /**< Dial a digit on FXO in pulse mode */
	ab_dev_event_FXS_ONHOOK, /**< Onhook on FXS */
	ab_dev_event_FXS_OFFHOOK, /**< Offhook on FXS */
	ab_dev_event_FM_CED, /**< CED and CEDEND FAX events */
	ab_dev_event_COD, /**< Coder event */
	ab_dev_event_TONE, /**< Tone generator event */
};/*}}}*/
enum vf_type_e {/*{{{*/
	vf_type_DEFAULT = 0,
	vf_type_N4 = 0, /**< normal 4-wired */
	vf_type_N2,     /**< normal 2-wired */
	vf_type_T4,    /**< transit 4-wired */
	vf_type_T2,    /**< transit 2-wired */
};/*}}}*/
struct ab_chan_status_s {/*{{{*/
	enum ab_chan_tone_e	tone;	/**< tone state */
	enum ab_chan_ring_e	ring;	/**< ring state */
	enum ab_chan_hook_e	hook;	/**< hoot state */
	enum ab_chan_linefeed_e	linefeed; /**< linefeed state */
};/*}}}*/
struct ab_dev_event_s {/*{{{*/
	enum ab_dev_event_e id; /**< Event identificator */
	unsigned char ch;	/**< Ret Channel of event */
	unsigned char more;	/**< is there more events */
	long data;		/**< Event specific data */
};/*}}}*/
struct ab_chan_jb_stat_s {/*{{{*/
enum jb_type_e nType; /** Jitter buffer type */
unsigned short nBufSize; /** Current jitter buffer size */
unsigned short nMaxBufSize; /** Maximum estimated jitter buffer size */
unsigned short nMinBufSize; /** Minimum estimated jitter buffer size */
unsigned short nPODelay; /** Playout delay */
unsigned short nMaxPODelay; /** Maximum playout delay */
unsigned short nMinPODelay; /** Minimum playout delay */
unsigned long nPackets; /** Received packet number */
unsigned short nInvalid; /** Invalid packet number */
unsigned short nLate; /** Late packets number */
unsigned short nEarly; /** Early packets number */
unsigned short nResync; /** Resynchronizations number */
unsigned long nIsUnderflow; /** Total number of injected samples since the beginning of the connection or since the last statistic reset due to jitter buffer underflows */
unsigned long nIsNoUnderflow; /** Total number of injected samples since the beginning of the connection or since the last statistic reset in case of normal jitter buffer operation, which means when there is not a jitter buffer underflow */
unsigned long nIsIncrement; /** Total number of injected samples since the beginning of the connection or since the last statistic reset in case of jitter buffer increments */
unsigned long nSkDecrement; /** Total number of skipped lost samples since the beginning of the connection or since the last statistic reset in case of jitter buffer decrements */
unsigned long nDsDecrement; /** Total number of dropped samples since the beginning of the connection or since the last statistic reset in case of jitter buffer decrements */
unsigned long nDsOverflow; /** Total number of dropped samples since the beginning of the connection or since the last statistic reset in case of jitter buffer overflows */
unsigned long nSid; /** Total number of comfort noise samples since the beginning of the connection or since the last statistic reset */
unsigned long nRecBytesH; /** Number of received bytes high part including event packets */
unsigned long nRecBytesL; /** Number of received bytes low part including event packets */
};/*}}} */
struct ab_chan_rtcp_stat_s {/*{{{*/
	unsigned long ssrc; /**< Sender generating this report */
	unsigned long rtp_ts; /**< RTP time stamp */
	unsigned long psent; /**< Sent packet count */
	unsigned long osent; /**< Sent octets count */
	unsigned long rssrc; /**< Data source */
	unsigned char fraction; /**< Receivers fraction loss */
	unsigned long lost; /**< Receivers packet lost */
	unsigned long last_seq; /**< Extended last seq nr. received */
	unsigned long jitter; /**< Receives interarrival jitter */
};/*}}}*/
struct ab_chan_stat_s {/*{{{*/
	int is_up; /**< Is now channel in RTP flow? */
	int con_cnt; /**< Connections count */
	/* Avarage Jitter buffer statistics */
	unsigned long pcks_avg; /**< Average packets number per connection */
	double invalid_pc;  /**< Invalid packet percent */
	double late_pc;  /**< Late packet percent */
	double early_pc;  /**< Early packet percent */
	double resync_pc;  /**< Resynchronizations percent */
	/* Current/last connection statistics */
	struct ab_chan_jb_stat_s jb_stat; /**< Jitter Buffer statistics */
	struct ab_chan_rtcp_stat_s rtcp_stat; /**< RTCP statistics */
};/*}}}*/
struct ab_chan_s {/*{{{*/
	unsigned int idx;   /**< Channel index on device (from 1) */
	unsigned char abs_idx; /**< Channel index on boards (from 0) */
	ab_dev_t * parent;  /**< device that channel belongs */
	enum vf_type_e type_if_vf; /**< VF type if it is VF channel (see parent type)*/
	enum cid_std_e cid_std; /**<Caller id standard */
	int rtp_fd;         /**< Channel file descriptor */
	struct ab_chan_status_s status;  /**< Channel status info */
	struct ab_chan_stat_s statistics; /**< Jitter Buffer and RTCP statistics */
	void * ctx; /**< Channel context pointer (for user app) */
};/*}}}*/
struct ab_dev_s {/*{{{*/
	unsigned int idx;	/**< Device index on boards (from 1) */
	ab_dev_type_t type;	/**< Device type */
	ab_t * parent;		/**< Parent board pointer */
	int cfg_fd;         /**< Device config file descriptor */
	void * ctx; /**< Device context (for user app) */
};/*}}}*/
struct ab_s {/*{{{*/
	unsigned int devs_num;	/**< Devices number on the boards */
	ab_dev_t * devs;	/**< Devices of the boards */
	unsigned int chans_num;	/**< Channels number on the boards */
	ab_chan_t * chans;	/**< Channels of the boards according to idx */
	ab_chan_t * pchans[CHANS_MAX]; /**< Pointers to channels according to abs_idx*/
	unsigned int chans_per_dev;/**< Channels number per device */
};/*}}}*/

/* ERROR HANDLING *//*{{{*/
/** No errors */
#define AB_ERR_NO_ERR 		0
/** In most cases ioctl error */
#define AB_ERR_UNKNOWN		1
/** Not enough memory */
#define AB_ERR_NO_MEM		2
/** error on file operation */
#define AB_ERR_NO_FILE		3
/** Bad parameter set */
#define AB_ERR_BAD_PARAM 	4
/** Hardware problem */
#define AB_ERR_NO_HARDWARE  5

/** global error characteristic string length */
#define ERR_STR_LENGTH		256

/** global error index */
extern int ab_g_err_idx;
/** global error characteristic string */
extern char ab_g_err_str[ERR_STR_LENGTH];
/** global error extra value (using in some cases) */
extern int ab_g_err_extra_value;
/*}}}*/

/** @defgroup AB_BASIC ACTIONS Basic libab interface
	Basic interface.
  @{  */ /*{{{*/
/** Do not reload modules */
#define AB_HWI_SKIP_MODULES 0x01
/** Do not make basicdev_init */
#define AB_HWI_SKIP_DEVICES 0x02
/** Do not load firmware */
#define AB_HWI_SKIP_CHANNELS 0x04

/* Firmware files path - set your values there. */
/** Firmware PRAM for any channel */
#define AB_FW_PRAM_NAME "/lib/firmware/pramfw.bin"
/** Firmware DRAM for any channel */
#define AB_FW_DRAM_NAME "/lib/firmware/dramfw.bin"
/** Firmware CRAM for FXS channel */
#define AB_FW_CRAM_FXS_NAME "/lib/firmware/cramfw_fxs.bin"
/** Firmware CRAM for FXO channel */
#define AB_FW_CRAM_FXO_NAME "/lib/firmware/cramfw_fxo.bin"
/** Firmware CRAM for VF normal 2-wired channel */
#define AB_FW_CRAM_VFN2_NAME "/lib/firmware/cramfw_vfn2.bin"
/** Firmware CRAM for VF normal 4-wired channel */
#define AB_FW_CRAM_VFN4_NAME "/lib/firmware/cramfw_vfn4.bin"
/** Firmware CRAM for VF transit 2-wired channel */
#define AB_FW_CRAM_VFT2_NAME "/lib/firmware/cramfw_vft2.bin"
/** Firmware CRAM for VF transit 4-wired channel */
#define AB_FW_CRAM_VFT4_NAME "/lib/firmware/cramfw_vft4.bin"

/** Basic drivers loading and hardware initialization */
// int ab_hardware_init (enum vf_type_e * const types, int const flags);
/** Create the ab_t object */
ab_t* ab_create (void);
/** Destroy the ab_t object */
void ab_destroy (ab_t ** ab);
/** Init channel with given CRAM file */
//int ab_chan_cram_init (ab_chan_t const * const chan, char const * const path);
/** Init device gpio with given channel types */
//int ab_devs_vf_gpio_reset (ab_t const * const ab);
/** @} */
/*}}}*/

/** @defgroup AB_RINGS_TONES Ringing and toneplay libab interface.
	Rings and Tones.
  @{ */ /*{{{*/
/** Play ring or mute it */
int ab_FXS_line_ring( ab_chan_t * const chan, enum ab_chan_ring_e ring, char * number, char * name );
/** Play tone or mute it */
int ab_FXS_line_tone( ab_chan_t * const chan, enum ab_chan_tone_e tone );
/** Change linefeed mode */
int ab_FXS_line_feed( ab_chan_t * const chan, enum ab_chan_linefeed_e feed );
/** Onhook or offhook on FXO line */
int ab_FXO_line_hook( ab_chan_t * const chan, enum ab_chan_hook_e hook );
/** Dial a digit on FXO line */
int ab_FXO_line_digit( 
		ab_chan_t * const chan, 
		char const data_length, char const * const data,
		char const nInterDigitTime, char const nDigitPlayTime, 
		char const pulseMode);
/** Play DTMF or busy/dial/ringing to the network connection (rtp-flow) */
int ab_FXS_netlo_play( ab_chan_t * const chan, char tone, char local );
/** @} */
/*}}}*/

/** @defgroup AB_EVENTS Events libab interface.
	Events.
  @{ *//*{{{*/
/** Get the events occures on given device */
int ab_dev_event_get( 
		ab_dev_t * const dev, 
		ab_dev_event_t * const evt, 
		unsigned char * const chan_available );
/** @} */
/*}}}*/

/** @defgroup AB_MEDIA Media libab interface.
	Codecs RTP-frames etc.
  @{ *//*{{{*/
/** Tune rtp parameters for fax transmittion */
int ab_chan_fax_pass_through_start( ab_chan_t * const chan );
/** Tune rtp parameters on selected chan */
int ab_chan_media_rtp_tune( ab_chan_t * const chan, codec_t const * const cod,
		codec_t const * const fcod, rtp_session_prms_t const * const rtpp, int te_payload);
/** Tune jitter buffer parameters on selected chan */
int ab_chan_media_jb_tune( ab_chan_t * const chan, jb_prms_t const * const jbp);
/** Tune wlec parameters on selected chan */
int ab_chan_media_wlec_tune( ab_chan_t * const chan, wlec_t const * const wp );
/** Switch on/off media on selected chan */
int ab_chan_media_switch( ab_chan_t * const chan, unsigned char const switch_up);
/** HOLD on/off encoding on selected chan */
int ab_chan_media_enc_hold( ab_chan_t * const chan, unsigned char const hold );
/** MUTE on/off encoder on selected chan */
int ab_chan_media_enc_mute( ab_chan_t * const chan, unsigned char const mute );
/** Refresh jitter buffer statistics of the channel */
int ab_chan_media_jb_refresh( ab_chan_t * const chan );
/** Refresh RTCP statistics of the channel */
int ab_chan_media_rtcp_refresh( ab_chan_t * const chan );
/** Set Caller id standard of the channel */
int ab_chan_cid_standard( ab_chan_t * const chan, const cid_std_t std);
/** @} */
/*}}}*/

#endif /* __AB_API_H__ */

