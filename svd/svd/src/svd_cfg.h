/**
 * @file svd_cfg.h
 * Configuration interface.
 * It contains startup \ref g_so and main \ref g_conf configuration
 * 		structures and functions to manipulate with them.
 */

#ifndef __SVD_CFG_H__
#define __SVD_CFG_H__

#include "ab_api.h"
#include "sofia.h"
#include "svd.h"

/** @defgroup CFG_M Main configuration.
 *  It contains cfg files names, and functions for manipulations with
 *  main configuration.
 *  @{*/
#define MAIN_CONF_NAME      "/etc/svd/main.conf"
#define SIP_CONF_NAME       "/etc/svd/accounts.conf"
#define DIALPLAN_CONF_NAME  "/etc/svd/dialplan.conf"
#define CODECS_CONF_NAME    "/etc/svd/codecs.conf"
#define RTP_CONF_NAME       "/etc/svd/rtp.conf"
#define WLEC_CONF_NAME      "/etc/svd/wlec.conf"
/** @}*/

/** @defgroup CFG_DF Default values.
 *  @ingroup CFG_M
 *  Some default values that will be set if they will not find in config file.
 *  @{*/
#define ALAW_PT_DF 0
/** @}*/

/** @defgroup DIAL_MARK Dial string markers.
 * 	@ingroup DIAL_SEQ
 * 	Dialling markers.
 *  @{*/
/** Place call marker */
#define PLACE_CALL_MARKER '#'
/** @}*/

/** @defgroup STARTUP Startup configuration.
 *  \ref g_so struct (startup options) and functions to manipulate with it.
 *  @{*/
/*
COMMAND LINE KEYS:
  -h, --help		display this help and exit
  -V, --version		show version and exit
*/

/** Sturtup keys set. */
struct _startup_options {
	unsigned char help; /**< Show help and exit. */
	unsigned char version; /**< Show version and exit. */
	char debug_level; /**< Logging level in debug mode. */
} g_so;

/** Getting parameters from startup keys. */
int  startup_init( int argc, char ** argv );
/** Destroy startup parameters structures and print error messages. */
void startup_destroy( int argc, char ** argv );
/** @} */


/** @addtogroup CFG_M
 *  @{*/
/* g_conf inner definitions {{{*/
/** Codecs massives sizes.
 * First unusing codec \c type will be set as \c codec_type_NONE.
 * It should be greater then codecs count because application can
 * test the end of the list by \c == \c codec_type_NONE */
#define COD_MAS_SIZE 12
/** Address payload length.*/
#define ADDR_PAYLOAD_LEN 40 /* sip id or address, or full PSTN phone number */
/** IP number length.*/
#define IP_LEN_MAX	16 /* xxx.xxx.xxx.xxx\0 */
/*}}}*/

/** Reads config files and init \ref g_conf structure.*/
int  svd_conf_init( ab_t const * const ab );
/** Show the config information from \ref g_conf.*/
void conf_show( void );
/** Destroy \ref g_conf.*/
void svd_conf_destroy( void );


#define FMTP_STR_LEN 20

/** Codec rtp and sdp parameters.*/
typedef struct cod_prms_s {
	cod_type_t type;
	char sdp_name[COD_NAME_LEN];
	char fmtp_str[FMTP_STR_LEN];
	int rate;
} cod_prms_t;

/* g_conf inner structures {{{*/
/** Dial plan  record.*/
struct dplan_record_s {
	char *prefix; /**< prefix that matches this record.*/
	int prefixlen;
	char *replace; /**< if not empty, replace prefix with this value before dialling.*/
	int replacelen;
	int account; /**< sip account to use for this rule.*/
};
/** Address book.*/
struct dial_plan_s {
	unsigned int records_num; /**< Number of records in dial plan.*/
	struct dplan_record_s * records; /**< Records.*/
};
/** SIP registration and codec choise policy.*/
struct sip_account_s {
	unsigned char all_set; /**< Shall we register on sip server?*/
	int codecs[COD_MAS_SIZE];/**< Codecs sorted by priority.*/
	char *registrar; /**< SIP registrar address.*/
	char *user_name; /**< SIP user name.*/
	char *user_pass; /**< SIP user password.*/
	char *user_URI; /**< SIP URI.*/
	char *sip_domain; /**< sip domain for outgoing calls.*/
	char *rtp_interface; /**<interface to use for rtp traffic */
	int outgoing_priority[CHANS_MAX]; /**< priority of this account for each channel (lower number takse precedence */
	unsigned char ring_incoming[CHANS_MAX]; /**< which channels to ring for incoming calls */
	unsigned char registered; /**< Account correctly registered.*/
	char * registration_reply; /**<Last registration reply received from registrar-> */
	nua_handle_t * op_reg; /**< Pointer to NUA Handle for registration.*/
};
/** Fax parameters.*/
struct fax_s {
	enum cod_type_e codec_type;
	int internal_pt;
	int external_pt;
};
/** Routine main configuration struct.*/
struct svd_conf_s {/*{{{*/
	int channels; /**<Number of configured channels (from svd_chans_init). */
	char * self_number; /**< Pointer to corresponding rt.rec[].id or NULL.*/
	char * self_ip; /**< Pointer to corresponding rt.rec[].value or lo_ip.*/
	char lo_ip[IP_LEN_MAX]; /**< Local Address IP. */
	char log_level; /**< If log_level = -1 - do not log anything.*/
	struct fax_s fax;/**< Fax parameters.*/ /* FIXME */
	unsigned long rtp_port_first; /**< Min ports range bound for RTP.*/
	unsigned long rtp_port_last; /**< Max ports range bound for RTP.*/
	int sip_accounts; /** how many sip accounts have been defined */
	cod_prms_t cp[COD_MAS_SIZE]; /**<Codecs parameters.*/
	codec_t codecs[COD_MAS_SIZE];/**< Codecs definitions.*/
	struct sip_account_s * sip_account; /**< SIP settings for registration.*/
	struct dial_plan_s dial_plan; /**< Dial plan.*/
	struct rtp_session_prms_s audio_prms [CHANS_MAX]; /**< AUDIO channel params.*/
	struct wlec_s       wlec_prms    [CHANS_MAX]; /**< WLEC channel parameters.*/
	unsigned char sip_tos; /** Type of Service byte for sip-packets.*/
	unsigned char rtp_tos; /** Type of Service byte for rtp-packets.*/
} g_conf;/*}}}*/

/** @} */

#endif /* __SVD_CFG_H__ */

