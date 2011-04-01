/**
 * @file svd.h
 * Main routine definitions.
 * It containes structures and definitions, uses in every part or routine.
 */
#ifndef __SVD_H__
#define __SVD_H__

typedef struct svd_s svd_t;
typedef struct svd_chan_s svd_chan_t;
extern svd_t * g_svd;

/* define type of context pointers for callbacks */
/*{{{*/
#define NUA_IMAGIC_T    ab_chan_t
#define NUA_HMAGIC_T    ab_chan_t
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
	struct dial_state_s {
		enum dial_state_e {
			dial_state_START,/**< Start state - no chan or route id entered.*/
			dial_state_ADDR_BOOK,/**< Entering addressbook identifier.*/
			dial_state_ROUTE_ID,/**< Entering route identifier.*/
			dial_state_CHAN_ID,/**< Entering channel identifier.*/
			dial_state_NET_ADDR,/**< Entering network address.*/
		} state; /**< State of dialing process.*/
		int tag; /**< Additional info in dialing process.*/
		enum self_e {
			self_UNDEFINED = 0, /**< Current state unknown.*/
			self_YES, /**< Yes - destination router is self.*/
			self_NO /**< No - destination is another router.*/
		} dest_is_self; /**< Is the desitnation router self?*/

		char * route_id; /**< Dest.\ router identifier if it is not self.*/
		char * route_ip; /**< Dest.\ router ip - points to \ref g_conf value.*/
		char chan_id [CHAN_ID_LEN]; /**< Dest.\ channel identificator.*/

		char * addrbk_id;/**< Address book identificator.*/
		char * addrbk_value; /**< Address book value - points to \ref g_conf value.*/
		char addr_payload [ADDR_PAYLOAD_LEN]; /**< SIP number or other info.*/
	} dial_status; /**< Dial status and values, gets in dial process.*/

	codec_t vcod; /**< voice coder */
	codec_t fcod; /**< faxmodem coder */

	char sdp_cod_name[COD_NAME_LEN]; /**< SDP selected codec.*/
	int sdp_payload; /**< SDP Selected payload.*/
	int rtp_sfd; /**< RTP socket file descriptor.*/
	int rtp_port; /**< Local RTP port.*/

	enum calltype_e {
		calltype_UNDEFINED, /**< call type was not defined */
		calltype_LOCAL, /**< call in the local network */
		calltype_REMOTE,/**< call to internet */
	} call_type; /**< Remote or local call */
	int caller_router_is_self; /**< Does we get call from self router.*/

	int remote_port; /**< Remote RTP port.*/
	char * remote_host; /**< Remote RTP host.*/

	int local_wait_idx; /**< Local wait index.*/
	int remote_wait_idx; /**< Remote wait index.*/

	int call_state; /**< Current callstate on channel.*/
	nua_handle_t * op_handle;/**< NUA handle for channel.*/

	/* DIAL SEQ */
	char dial_rbuf[DIAL_RBUF_LEN];
	int dial_put_idx;
	int dial_get_idx;

	/* RING */
	enum ring_state_e {
		ring_state_NO_RING_BEFORE,
		ring_state_INVITE_IN_QUEUE,
		ring_state_NO_TIMER_INVITE_SENT,
		ring_state_TIMER_UP_INVITE_SENT,
		ring_state_CANCEL_IN_QUEUE,
	} ring_state; /**< State of ring processing. */
	su_timer_t * ring_tmr; /**< Ring processing timer. */

	/* VOICE FREQUENCY */
	enum vf_tmr_request {
		vf_tmr_nothing=0,
		vf_tmr_reinvite,
	} vf_tmr_request;
	su_timer_t * vf_tmr; /**< VF-chan reinvite timer. */
	size_t vf_echo_count; // 0: echo replied, >0 - echo still not replied

	/* HOTLINE */
	unsigned char is_hotlined; /**< Is this channel hotline initiator.*/
	char * hotline_addr; /**< Hotline destintation address, points to
						   \ref g_conf value.*/
};/*}}}*/

/** Routine main context structure.*/
struct svd_s
{/*{{{*/
	su_root_t *root;	/**< Pointer to application root.*/
	su_home_t home[1];	/**< Our memory home.*/
	nua_t * nua;		/**< Pointer to NUA object.*/
	ab_t * ab;		/**< Pointer to ATA Boards object.*/
	nua_handle_t * op_reg; /**< Pointer NUA Handle registration object.*/
	char outbound_ip [IP_LEN_MAX]; /**< Outbound ip address.*/
	int ifd; /**< Interface socket file deskriptor. */
};/*}}}*/

#endif /* __SVD_H__ */

