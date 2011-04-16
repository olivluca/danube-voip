/**
 * @file svd_server_if.c
 * Interface server command functions.
 * It containes executions of interface commands.
 * */

/* Includes {{{ */
#include "svd.h"
#include "svd_if.h"
#include "svd_cfg.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>
#include <assert.h>
#include <errno.h>
/*}}}*/

/** Create interface for svd_if.*/
int svd_create_interface(svd_t * svd);
/** Interface handler.*/
static int svd_if_handler(su_root_magic_t * root, su_wait_t * w,
		su_wakeup_arg_t * user_data);
/** Execute given interface message.*/
static int svd_exec_msg(svd_t * const svd, char const * const buf,
		char ** const buff, int * const buff_sz);
/** Execute 'test' command.*/
static int svd_exec_jbt(svd_t * const svd, char ** const buff, int * const buff_sz);
/** Execute 'func' with 2 args.*/
static int svd_exec_2af(svd_t * const svd, struct svdif_msg_s * const msg,
		char ** const buff, int * const buff_sz, int (*func)(ab_chan_t * const chan,
		char ** const buf, int * const palc, enum msg_fmt_e const fm));
/** Execute 'shutdown' command.*/
static int svd_exec_shutdown(svd_t * svd, char ** const buff, int * const buff_sz);
/** Execute 'get_registrations' command.*/
static int svd_exec_regs(svd_t * svd, char ** const buff, int * const buff_sz);
/** Add to string another and resize it if necessary */
static int svd_addtobuf(char ** const buf, int * const palc, char const * fmt, ...);
/** Put chan rtcp statistics to buffer */
static int svd_rtcp_for_chan(ab_chan_t * const chan,
		char ** const buf, int * const palc, enum msg_fmt_e const fmt);
static int svd_jb_for_chan(ab_chan_t * const chan,
		char ** const buf, int * const palc, enum msg_fmt_e const fmt);
/**
 * Create socket and allocate handler for interface.
 *
 * \param[in] svd 	pointer to svd structure
 * \retval 0 	etherything is fine
 * \retval -1 	error occures
 * \remark
 * 		svd should be allocated already
 * 		It creates interface socket
 * 		It creates sofia-sip wait object
 * 		It allocates handler for interface socket
 */
int
svd_create_interface(svd_t * svd)
{/*{{{*/
	su_wait_t wait[1];
	char err_msg[ERR_MSG_SIZE] = {0,};
	int err;

	assert(svd);

	err = svd_if_srv_create(&svd->ifd, err_msg);
	if(err){
		SU_DEBUG_0 (("%s\n",err_msg));
		goto __exit_fail;
	}
	err = su_wait_create (wait, svd->ifd, POLLIN);
	if(err){
		SU_DEBUG_0 ((LOG_FNC_A ("su_wait_create() fails" ) ));
		goto __exit_fail;
	}
	err = su_root_register (svd->root, wait, svd_if_handler, svd, 0);
	if (err == -1){
		SU_DEBUG_0 ((LOG_FNC_A ("su_root_register() fails" ) ));
		goto __exit_fail;
	}

	return 0;
__exit_fail:
	return -1;
}/*}}}*/

/**
 * Destroy socket for interface.
 * \param[in] svd 	pointer to svd structure
 */
void
svd_destroy_interface(svd_t * svd)
{/*{{{*/
	int err;
	char err_msg[ERR_MSG_SIZE] = {0,};

	assert(svd);

	err = svd_if_srv_destroy(&svd->ifd, err_msg);
	if(err){
		SU_DEBUG_0 (("%s\n",err_msg));
	}
}/*}}}*/

/**
 * Read command from the interface and calls appropriate functions.
 *
 * \param[in] 		root 		root object that contain wait object.
 * \param[in] 		w			wait object that emits.
 * \retval -1	if somthing nasty happens.
 * \retval 0 	if etherything is ok.
 */
static int
svd_if_handler(su_root_magic_t * root, su_wait_t * w, su_wakeup_arg_t * user_data)
{/*{{{*/
	unsigned char buf [MAX_MSG_SIZE];
	char * abuf = NULL;
	int abuf_sz = 0;
	char abuf_err[] = "{\"result\":\"fail\"}\n";
	int received;
	struct sockaddr_un cl_addr;
	int cl_addr_len;
	int cnt;
	int err;

	svd_t * svd = user_data;

	assert( svd->ifd != -1 );

	memset(&cl_addr, 0, sizeof(cl_addr));
	cl_addr_len = sizeof(cl_addr);

	/* read socket data */
	received = recvfrom (svd->ifd, buf, sizeof(buf), 0,
			(struct sockaddr * __restrict__)&cl_addr, &cl_addr_len );
	if (received == 0){
		SU_DEBUG_2 ((LOG_FNC_A("wrong interface event - no data")));
		goto __exit_fail;
	} else if (received < 0){
		SU_DEBUG_2 (("IF ERROR: recvfrom(): %d(%s)\n", errno, strerror(errno)));
		goto __exit_fail;
	}

	/* Parse and execute msg */
	err = svd_exec_msg (svd, buf, &abuf, &abuf_sz);
	if(err){
		abuf = abuf_err;
		abuf_sz = sizeof(abuf_err);
	}

	/* answer to the client */
	cnt = sendto(svd->ifd, abuf, abuf_sz, 0,
			(struct sockaddr * __restrict__)&cl_addr, sizeof(cl_addr));
	if(cnt == -1){
		SU_DEBUG_2(("server sending error (%s) (buf size: %d)\n",
				strerror(errno), abuf_sz));
		goto __abuf_alloc;
	} else if(cnt != abuf_sz){
		SU_DEBUG_2(("server sending error %d of %d sent\n",cnt,abuf_sz));
	}

	if((abuf != abuf_err) && abuf){
		free (abuf);
	}
	return 0;
__abuf_alloc:
	if((abuf != abuf_err) && abuf){
		free (abuf);
	}
__exit_fail:
	return -1;
}/*}}}*/

/**
 * Do the all necessory job on given message.
 *
 * \param[in]	svd		svd.
 * \param[in]	buf		message from the client.
 * \param[out]	buff	buffer to put the answer to the client.
 * \param[out]	buff_sz	size of the generated answer.
 * \retval -1	if somthing nasty happens.
 * \retval 0 	if etherything is ok.
 */
static int
svd_exec_msg(svd_t * const svd, char const * const buf,
		char ** const buff, int * const buff_sz)
{/*{{{*/
	struct svdif_msg_s msg;
	char err_msg [ERR_MSG_SIZE];
	int err;

	memset(&msg, 0, sizeof(msg));
	err = svd_if_srv_parse (buf, &msg, err_msg);
	if(err){
		SU_DEBUG_2 (("parsing (%s) error: %s\n", buf, err_msg));
		goto __exit_fail;
	}

	if       (msg.type == msg_type_GET_JB_STAT_TOTAL){
		err = svd_exec_jbt(svd, buff, buff_sz);
	} else if(msg.type == msg_type_GET_JB_STAT){
		err = svd_exec_2af(svd, &msg, buff, buff_sz, svd_jb_for_chan);
	} else if(msg.type == msg_type_GET_RTCP_STAT){
		err = svd_exec_2af(svd, &msg, buff, buff_sz, svd_rtcp_for_chan);
	} else if(msg.type == msg_type_SHUTDOWN){
		err = svd_exec_shutdown(svd, buff, buff_sz);
	} else if(msg.type == msg_type_REGISTRATIONS){
		err = svd_exec_regs(svd, buff, buff_sz);
	}
	if(err){
		goto __exit_fail;
	}
	return 0;
__exit_fail:
	return -1;
}/*}}}*/

/**
 * Executes 'jbt' command and creates buffer with answer.
 *
 * \param[in]	svd		svd.
 * \param[out]	buff	buffer to put the answer.
 * \param[out]	buff_sz	buffer size.
 * \retval -1	if somthing nasty happens.
 * \retval 0 	if etherything is ok.
 * \remark
 *	It allocates memory for buffer with answer. Caller must free this
 *	memory than he is not need it more.
 */
static int
svd_exec_jbt(svd_t * const svd, char ** const buff, int * const buff_sz)
{/*{{{*/
	int i;
	int cn = svd->ab->chans_num;
	unsigned char is_up = 0;
	unsigned long nInvalid = 0;
	unsigned long nLate = 0;
	unsigned long nEarly = 0;
	unsigned long nResync = 0;
	unsigned long nIsUnderflow = 0;
	unsigned long nIsNoUnderflow = 0;
	unsigned long nIsIncrement = 0;
	unsigned long nSkDecrement = 0;
	unsigned long nDsDecrement = 0;
	unsigned long nDsOverflow = 0;
	unsigned long nSid = 0;
	int err;

	for (i=0; i<cn; i++){
		ab_chan_t * chan = &svd->ab->chans[i];
		struct ab_chan_jb_stat_s * s = &chan->statistics.jb_stat;
		err = ab_chan_media_jb_refresh(chan);
		if(err){
			if(svd_addtobuf(buff, buff_sz,
					"{\"error\":\"jb_refresh error: %s\"}\n",ab_g_err_str)){
				goto __exit_fail;
			}
			goto __exit_success;
		}
		if(chan->statistics.is_up){
			is_up++;
			nInvalid += s->nInvalid;
			nLate += s->nLate;
			nEarly += s->nEarly;
			nResync += s->nResync;
			nIsUnderflow += s->nIsUnderflow;
			nIsNoUnderflow += s->nIsNoUnderflow;
			nIsIncrement += s->nIsIncrement;
			nSkDecrement += s->nSkDecrement;
			nDsDecrement += s->nDsDecrement;
			nDsOverflow += s->nDsOverflow;
			nSid += s->nSid;
		}
	}
	/* out to buffer */
		err = svd_addtobuf(buff, buff_sz,
"Channel up/down: %d / %d\n\
Packets Invalid: %lu\n\
Packets Late: %lu\n\
Packets Early: %lu\n\
Resynchronizations: %lu\n\
Injected Samples (JB Underflows): %lu\n\
Injected Samples (JB Not Underflows): %lu\n\
Injected Samples (JB Increments): %lu\n\
Skipped Lost Samples (JB Decrements): %lu\n\
Dropped Samples (JB Decrements): %lu\n\
Dropped Samples (JB Overflows): %lu\n\
Comfort Noise Samples: %lu\n",
		is_up, (cn-is_up),
		nInvalid,nLate,nEarly,nResync,nIsUnderflow,nIsNoUnderflow,nIsIncrement,
		nSkDecrement,nDsDecrement,nDsOverflow,nSid);
__exit_success:
	return 0;
__exit_fail:
	return -1;
}/*}}}*/

/**
 * Executes 'func' with 2 args (chan and fmt) and creates buffer with answer.
 *
 * \param[in]	svd		svd.
 * \param[in]	msg		message to execute.
 * \param[out]	buff	buffer to put the answer.
 * \param[out]	buff_sz	buffer size.
 * \param[in]	func	function to execute.
 * \retval -1	if somthing nasty happens.
 * \retval 0 	if etherything is ok.
 * \remark
 *	It allocates memory for buffer with answer. Caller must free this
 *	memory than he is not need it more.
 */
static int
svd_exec_2af(svd_t * const svd, struct svdif_msg_s * const msg,
		char ** const buff, int * const buff_sz, int (*func)(ab_chan_t * const chan,
		char ** const buf, int * const palc, enum msg_fmt_e const fm))
{/*{{{*/
	if       (msg->ch_sel.ch_t == ch_t_ONE){/*{{{*/
		int ch_n = msg->ch_sel.ch_if_one;
		if((ch_n<0) || (ch_n>=CHANS_MAX)){
			if(svd_addtobuf(buff, buff_sz,
					"{\"error\":\"wrong chan num %d\"}\n",ch_n)){
				goto __exit_fail;
			}
			goto __exit_success;
		} else if(!svd->ab->pchans[msg->ch_sel.ch_if_one]){
			if(svd_addtobuf(buff, buff_sz,
					"{\"error\":\"wrong chan num %d\"}\n",ch_n)){
				goto __exit_fail;
			}
			goto __exit_success;
		}
		if(func(svd->ab->pchans[msg->ch_sel.ch_if_one],
				buff, buff_sz, msg->fmt_sel)){
			goto __exit_fail;
		}/*}}}*/
	} else if(msg->ch_sel.ch_t == ch_t_ALL){/*{{{*/
		int i;
		int cn = svd->ab->chans_num;
		if(msg->fmt_sel == msg_fmt_JSON){
			if(svd_addtobuf(buff, buff_sz,"[\n")){
				goto __exit_fail;
			}
		}
		for (i=0; i<cn-1; i++){
			if(func(&svd->ab->chans[i], buff, buff_sz, msg->fmt_sel)){
				goto __exit_fail;
			}
			if(msg->fmt_sel == msg_fmt_JSON){
				if(svd_addtobuf(buff, buff_sz,",\n")){
					goto __exit_fail;
				}
			}
		}
		if(func(&svd->ab->chans[i], buff, buff_sz, msg->fmt_sel)){
			goto __exit_fail;
		}
		if(msg->fmt_sel == msg_fmt_JSON){
			if(svd_addtobuf(buff, buff_sz,"]\n")){
				goto __exit_fail;
			}
		}/*}}}*/
	} else if(msg->ch_sel.ch_t == ch_t_ACTIVE){/*{{{*/
		int i;
		int cn = svd->ab->chans_num;
		enum state_e {state_FIRST,state_SECOND,state_OTHER} st = state_FIRST;

		if(msg->fmt_sel == msg_fmt_JSON){
			if(svd_addtobuf(buff, buff_sz,"[\n")){
				goto __exit_fail;
			}
		}
		for (i=0; i<cn-1; i++){
			if(svd->ab->chans[i].statistics.is_up){
				if(st == state_OTHER){
					if(msg->fmt_sel == msg_fmt_JSON){
						if(svd_addtobuf(buff, buff_sz,",\n")){
							goto __exit_fail;
						}
					}
				}
				if(func(&svd->ab->chans[i], buff, buff_sz, msg->fmt_sel)){
					goto __exit_fail;
				}
				if(msg->fmt_sel == msg_fmt_JSON){
					if(st == state_FIRST){
						if(svd_addtobuf(buff, buff_sz,",\n")){
							goto __exit_fail;
						}
						st = state_SECOND;
					} else if(st == state_SECOND){
						st = state_OTHER;
					}
				}
			}
		}
		if(svd->ab->chans[i].statistics.is_up){
			if(st == state_OTHER){
				if(msg->fmt_sel == msg_fmt_JSON){
					if(svd_addtobuf(buff, buff_sz,",\n")){
						goto __exit_fail;
					}
				}
			}
			if(func(&svd->ab->chans[i], buff, buff_sz, msg->fmt_sel)){
				goto __exit_fail;
			}
		}
		if(msg->fmt_sel == msg_fmt_JSON){
			if(svd_addtobuf(buff, buff_sz,"]\n")){
				goto __exit_fail;
			}
		}
	}/*}}}*/
__exit_success:
	return 0;
__exit_fail:
	return -1;
}/*}}}*/

static int
svd_exec_regs(svd_t * svd, char ** const buff, int * const buff_sz)
{/*{{{*/
	int i;
	sip_account_t * account;
	int accounts;
	
	if(svd_addtobuf(buff, buff_sz,"[\n")){
		goto __exit_fail;
	}
	accounts=su_vector_len(g_conf.sip_account);
	for (i=0; i<accounts; i++) {
		account = su_vector_item(g_conf.sip_account, i);
		if(svd_addtobuf(buff, buff_sz, "{\"account\":\"%d\", \"uri\":\"%s\", \"registered\":\"%d\", \"last_message\":\"%s\"}",
		  i, account->user_URI, account->registered, account->registration_reply)) {
			goto __exit_fail;
		}
		if (i<accounts-1) {
			if(svd_addtobuf(buff, buff_sz,",\n")){
				goto __exit_fail;
			}
		}
	}
	if(svd_addtobuf(buff, buff_sz,"\n]\n")){
		goto __exit_fail;
	}
	return 0;
__exit_fail:
	return -1;
}/*}}}*/

static int
svd_exec_shutdown(svd_t * svd, char ** const buff, int * const buff_sz)
{/*{{{*/
	/* svd shutdown */
	svd_shutdown(svd);

	if(svd_addtobuf(buff, buff_sz,"{\"shutdown\":\"starting\"}\n")){
		goto __exit_fail;
	}
	return 0;
__exit_fail:
	return -1;
}/*}}}*/

static int
svd_addtobuf(char ** const buf, int * const palc, char const * fmt, ...)
{/*{{{*/
	va_list ap;
	int n;
	int psz;
	char * nbuf;

	if(!(*buf)){
		*palc = 300;
		*buf = malloc(*palc);
		if(!(*buf)){
			SU_DEBUG_2((LOG_NOMEM_A("malloc for resizer")));
			printf("no mem for malloc\n");
			goto __exit_fail;
		}
		memset(*buf, 0, sizeof(**buf));
	}

	psz = strlen(*buf);

	while (1) {
		/* Try to print in the allocated space. */
		va_start(ap, fmt);
		n = vsnprintf((*buf)+psz, (*palc)-psz, fmt, ap);
		va_end(ap);
		/* If that worked, return */
		if (n > -1 && n < (*palc)-psz){
			goto __exit_success;
		}
		if(n < 0){
			/* error */
			SU_DEBUG_2(("vsprintf:%s",strerror(errno)));
			goto __exit_fail;
		} else {
			/* not enough space */
			if ((nbuf = realloc (*buf, (*palc)*2)) == NULL) {
				SU_DEBUG_2(("realloc:%s",strerror(errno)));
				goto __exit_fail;
			}
			*buf = nbuf;
			memset((*buf)+(*palc), 0, sizeof(*palc));
			(*palc) *= 2;
		}
	}
__exit_success:
return 0;
__exit_fail:
return -1;
}/*}}}*/

static int
svd_rtcp_for_chan(ab_chan_t * const chan, char ** const buf, int * const palc,
		enum msg_fmt_e const fmt)
{/*{{{*/
	int err;
	char yn[5] = {0,};
	struct ab_chan_rtcp_stat_s const * const s = &chan->statistics.rtcp_stat;
	err = ab_chan_media_rtcp_refresh(chan);
	if(err){
		if(svd_addtobuf(buf, palc,
				"{\"error\":\"rtcp_refresh error: %s\"}\n",ab_g_err_str)){
			goto __exit_fail;
		}
		goto __exit_success;
	}
	if(chan->statistics.is_up){
		strcpy(yn,"YES");
	} else {
		strcpy(yn,"NO");
	}
	err = svd_addtobuf(buf, palc,
"{\"chanid\": \"%02d\",\"isUp\":\"%s\",\"con_N\":\"%d\",\"RTCP statistics\":{\n\
\"ssrc\":\"0x%08lX\",\"rtp_ts\":\"0x%08lX\",\"psent\":\"%ld\",\"osent\":\"%ld\",\n\
\"fraction\":\"0x%02lX\",\"lost\":\"%ld\",\"last_seq\":\"%ld\",\"jitter\":\"0x%08lX\"}}\n",
	chan->abs_idx,yn,chan->statistics.con_cnt,s->ssrc,s->rtp_ts,s->psent,
	s->osent,s->fraction,s->lost,s->last_seq,s->jitter);
	if(err){
		goto __exit_fail;
	}
__exit_success:
	return 0;
__exit_fail:
	return -1;
}/*}}}*/

static int
svd_jb_for_chan(ab_chan_t * const chan, char ** const buf, int * const palc,
		enum msg_fmt_e const fmt)
{/*{{{*/
	int err;
	char yn[10] = {0,};
	char tp[10] = {0,};
	struct ab_chan_jb_stat_s const * const s = &chan->statistics.jb_stat;
	err = ab_chan_media_jb_refresh(chan);
	if(err){
		if(svd_addtobuf(buf, palc,
				"{\"error\":\"jb_refresh error: %s\"}\n",ab_g_err_str)){
			goto __exit_fail;
		}
		goto __exit_success;
	}
	if(chan->statistics.is_up){
		strcpy(yn,"Up");
	} else {
		strcpy(yn,"Down");
	}
	if(chan->statistics.jb_stat.nType == jb_type_FIXED){
		strcpy(tp,"fixed");
	} else {
		strcpy(tp,"adaptive");
	}
	if(fmt == msg_fmt_JSON){
		err = svd_addtobuf(buf, palc,
"{\"chanid\": \"%02d\",\"state\":\"%s\",\"con_N\":\"%d\",\"JB statistics\":{\"tp\":\"%s\",\n\
\"PksAvg\":\"%lu\",\"invPC\":\"%4.2f\",\"latePC\":\"%4.2f\",\"earlyPC\":\"%4.2f\",\"resyncPC\":\"%4.2f\",\n\
\"BS\":\"%u\",\"maxBS\":\"%u\",\"minBS\":\"%u\",\"POD\":\"%u\",\"maxPOD\":\"%u\",\"minPOD\":\"%u\",\n\
\"nPks\":\"%lu\",\"nInv\":\"%u\",\"nLate\":\"%u\",\"nEarly\":\"%u\",\"nResync\":\"%u\",\n\
\"nIsUn\":\"%lu\",\"nIsNoUn\":\"%lu\",\"nIsIncr\":\"%lu\",\n\
\"nSkDecr\":\"%lu\",\"nDsDecr\":\"%lu\",\"nDsOwrf\":\"%lu\",\n\
\"nSid\":\"%lu\",\"nRecvBytesH\":\"%lu\",\"nRecvBytesL\":\"%lu\"}}\n",
		chan->abs_idx,yn,chan->statistics.con_cnt,tp,chan->statistics.pcks_avg,
		chan->statistics.invalid_pc,chan->statistics.late_pc,
		chan->statistics.early_pc,
		chan->statistics.resync_pc,s->nBufSize/8,s->nMaxBufSize/8,s->nMinBufSize/8,
		s->nPODelay,s->nMaxPODelay,s->nMinPODelay,s->nPackets,s->nInvalid,s->nLate,
		s->nEarly,s->nResync,s->nIsUnderflow,s->nIsNoUnderflow,s->nIsIncrement,
		s->nSkDecrement,s->nDsDecrement,s->nDsOverflow,s->nSid,
		s->nRecBytesH,s->nRecBytesL);
	} else if(fmt == msg_fmt_CLI){
		err = svd_addtobuf(buf, palc,
"Channel:%02d (%s)\n\
Connecions number: %d\n\
Jitter Buffer Type: %s\n\
Jitter Buffer Size: %u (%u - %u)\n\
Playout Delay: %u (%u - %u)\n\
Packets Number: %lu\n\
Packets Invalid: %u\n\
Packets Late: %u\n\
Packets Early: %u\n\
Resynchronizations: %u\n\
Injected Samples (JB Underflows): %lu\n\
Injected Samples (JB Not Underflows): %lu\n\
Injected Samples (JB Increments): %lu\n\
Skipped Lost Samples (JB Decrements): %lu\n\
Dropped Samples (JB Decrements): %lu\n\
Dropped Samples (JB Overflows): %lu\n\
Comfort Noise Samples: %lu\n",
		chan->abs_idx,yn,chan->statistics.con_cnt,tp,
		s->nBufSize/8,s->nMinBufSize/8,s->nMaxBufSize/8,
		s->nPODelay,s->nMinPODelay,s->nMaxPODelay,s->nPackets,
		s->nInvalid,s->nLate,s->nEarly,s->nResync,
		s->nIsUnderflow,s->nIsNoUnderflow,s->nIsIncrement,
		s->nSkDecrement,s->nDsDecrement,s->nDsOverflow,s->nSid);
	}
	if(err){
		goto __exit_fail;
	}
__exit_success:
	return 0;
__exit_fail:
	return -1;
}/*}}}*/
