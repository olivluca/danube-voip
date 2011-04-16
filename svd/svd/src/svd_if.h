/**
 * @file svd_if.h
 * Main svd_if routine definitions.
 * It containes structures and definitions, uses in cli/srv and both
 * parts or routine.
 */
#ifndef __SVD_IF_H__
#define __SVD_IF_H__

/** @defgroup IF_ALL Interface client and server common part.
 *  Works just on the both side of svd -- svd_if.
 *  @{*/
/** Socket path to communicate client with server */
#define SOCKET_PATH "/var/svd/"
/** Socket name to communicate client with server */
#define SOCKET_NAME "svd-socket"
/** Maximum message length passed to socket from client to server */
#define MAX_MSG_SIZE 256
/** Maximum error message length passed to functions */
#define ERR_MSG_SIZE 256
/** Chans number to work with */
#define CHANS_MAX 32

#define ARG_DEF  "*"
#define CHAN_ALL "all"
#define CHAN_ACT "act"
#define FMT_CLI  "cli"
#define FMT_JSON "json"

/** @}*/

/** @defgroup IF_SRV Interface server part.
 *  @ingroup IF_ALL
 *  Works just on the server side of svd -- svd_if.
 *  @{*/
/** Messages types between client and server */
enum msg_type_e {/*{{{*/
	msg_type_NONE = 0, /**< No message incoming */
	msg_type_GET_JB_STAT_TOTAL, /**< Get jitter buffer total statistics */
	msg_type_GET_JB_STAT, /**< Get jitter buffer statistics */
	msg_type_GET_RTCP_STAT, /**< Get RTCP statistics */
	msg_type_SHUTDOWN, /**< Close all connections and prepare for exit */
	msg_type_REGISTRATIONS, /**<Get status of sip registrations */
	msg_type_COUNT, /**< count of messages */
};/*}}}*/
/** Given channel in the message */
struct msg_ch_s {/*{{{*/
	enum ch_t_e {
		ch_t_NONE, /**< No channel for this message */
		ch_t_ONE, /**< Given number of one channel */
		ch_t_ALL, /**< Message for all channels */
		ch_t_ACTIVE, /**< Message for channels in the RTP/RTCP flow right now */
	} ch_t;
	int ch_if_one; /**< there chan number if it is ch_t_ONE */
};/*}}}*/
/** Parsed message from client */
struct svdif_msg_s {/*{{{*/
	enum msg_type_e type; /**< Incoming message type */
	struct msg_ch_s ch_sel; /**< Incoming channel type */
	enum msg_fmt_e {
		msg_fmt_JSON,
		msg_fmt_CLI,
	} fmt_sel; /**< Requested format */
};/*}}}*/
/** Create new interface socket.*/
int svd_if_srv_create (int * const sfd, char * const err_msg);
/** Close and unlink interface socket.*/
int svd_if_srv_destroy (int * const sfd, char * const err_msg);
/** Parse the message.*/
int svd_if_srv_parse (char const * const str, struct svdif_msg_s * const msg,
		char * const err_msg);
/** @}*/

/** @defgroup IF_CLI Interface client part.
 *  @ingroup IF_ALL
 *  Works just on the client side of svd -- svd_if.
 *  @{*/
/** Start the engine client part.*/
int svd_if_cli_start (char * const err_msg);
/** @}*/

#endif /* __SVD_IF_H__ */

