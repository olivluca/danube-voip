/**
 * @file svd.h
 * Main routine definitions.
 * It containes structures and definitions, uses in every part or routine.
 */
#ifndef __SVD_H__
#define __SVD_H__

typedef struct svd_s svd_t;
typedef struct svd_chan_s svd_chan_t;
typedef struct sip_account_s sip_account_t;

/* define type of context pointers for callbacks */
/*{{{*/
#define NUA_IMAGIC_T    ab_chan_t
#define NUA_HMAGIC_T    sip_account_t
#define NUA_MAGIC_T     svd_t
#define SOA_MAGIC_T     svd_t
#define SU_ROOT_MAGIC_T svd_t
/*}}}*/

/* Using in svd_cfg.h too.*/
#define COD_NAME_LEN 15

/* used to transmitt digits with SIP INFO,
 * first val is indicator, that it was tone digit,
 * second val is the digit itself or '*', '#', 'A'-'D' */
#define INFO_STR "tone:%d\ndigit:'%c'\n"
#define INFO_STR_LENGTH 50

/** dial round buffer size for digits in queue. */
#define DIAL_RBUF_LEN 15

/* Includes {{{*/
#include "config.h"
#include "ab_api.h"
#include "sofia.h"
#include "svd_log.h"
#include "svd_cfg.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/unistd.h>
#include <errno.h>
#include <pthread.h>


#include "drv_tapi_io.h"
/*}}}*/

/** Channel context structure - store channel info / status / etc.*/
struct svd_chan_s
{/*{{{*/
	int chan_idx; /**< Index of the channel */ 
	struct dial_state_s {
		char digits [ADDR_PAYLOAD_LEN]; /**< collected digits.*/
		int num_digits; /**< how many digits have been collected.*/
	} dial_status; /**< Dial status and values, gets in dial process.*/

	codec_t vcod; /**< voice coder */
	codec_t fcod; /**< faxmodem coder */

	char sdp_cod_name[COD_NAME_LEN]; /**< SDP selected codec.*/
	int sdp_payload; /**< SDP Selected payload.*/
	int rtp_sfd; /**< RTP socket file descriptor.*/
	int rtp_port; /**< Local RTP port.*/

	sip_account_t * account; //**<Account used by this call */
	int remote_port; /**< Remote RTP port.*/
	char * remote_host; /**< Remote RTP host.*/

	int local_wait_idx; /**< Local wait index.*/
	int remote_wait_idx; /**< Remote wait index.*/

	nua_handle_t * op_handle;/**< NUA handle for channel.*/

	su_timer_t * dtmf_tmr; /**< Collect dtmf timer. */
	
};/*}}}*/

/** Routine main context structure.*/
struct svd_s
{/*{{{*/
	su_root_t *root;	/**< Pointer to application root.*/
	su_home_t home[1];	/**< Our memory home.*/
	nua_t * nua;		/**< Pointer to NUA object.*/
	ab_t * ab;		/**< Pointer to ATA Boards object.*/
	char outbound_ip [IP_LEN_MAX]; /**< Outbound ip address.*/
	int ifd; /**< Interface socket file deskriptor. */
};/*}}}*/

#endif /* __SVD_H__ */

