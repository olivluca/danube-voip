/**
 * @file svd_engine_if.c
 * Engine of the svd_interface programm.
 * It containes both server and clinet sides of the engine.
 * */

/* Includes {{{ */
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/stat.h>
#include <poll.h>
#include <sys/ioctl.h>

#include "svd_if.h"
/*}}}*/

int
svd_if_srv_create (int * const sfd, char * const err_msg)
{/*{{{*/
	struct sockaddr_un sv_addr;
    struct stat sbuf;
	int err;

    if( (err = stat(SOCKET_PATH,&sbuf)) ){
		if( errno != ENOENT ){
			snprintf(err_msg,ERR_MSG_SIZE,"Error getting info about %s",SOCKET_PATH);
			return -1;
		}
		if( mkdir(SOCKET_PATH,(S_IRWXU | S_IRGRP | S_IXGRP)) ){
			snprintf(err_msg,ERR_MSG_SIZE,"Cannot create dir %s",SOCKET_PATH);
			return -1;
		}
		if( stat(SOCKET_PATH,&sbuf) ){
			snprintf(err_msg,ERR_MSG_SIZE,"Error creating dir %s",SOCKET_PATH);
			return -1;
		}
    }
    if( !S_ISDIR(sbuf.st_mode) ){
		snprintf (err_msg,ERR_MSG_SIZE,"Error: %s is not directory",SOCKET_PATH);
		return -1;
    }

	memset (&sv_addr, 0, sizeof(sv_addr));
	sv_addr.sun_family = AF_UNIX;
	strcpy(sv_addr.sun_path, SOCKET_PATH SOCKET_NAME);

	/* for any case - normal - it fails */
	unlink (sv_addr.sun_path);

	if (-1 == (*sfd= socket(AF_UNIX, SOCK_DGRAM, 0))){
		snprintf(err_msg,ERR_MSG_SIZE,"Error: can`t create socket (%d)",errno);
		return -1;
	}

	if(-1 == bind (*sfd, (struct sockaddr*)&sv_addr, SUN_LEN(&sv_addr))){
		snprintf(err_msg,ERR_MSG_SIZE,"Error: can`t bind socket (%d)",errno);
		return -1;
	}
	return 0;
}/*}}}*/

int
svd_if_srv_destroy (int * const sfd, char * const err_msg)
{/*{{{*/
	if(close (*sfd)){
		snprintf(err_msg,ERR_MSG_SIZE,"Error can`t close %s (%s)",
				SOCKET_PATH  SOCKET_NAME, strerror(errno));
		return -1;
	}
	if(unlink (SOCKET_PATH SOCKET_NAME)){
		snprintf(err_msg,ERR_MSG_SIZE,"Error can`t unlink %s (%s)",
				SOCKET_PATH  SOCKET_NAME, strerror(errno));
		return -1;
	}
	return 0;
}/*}}}*/

int
svd_if_cli_start (char * const err_msg)
{/*{{{ */
	/*	create client socket
	 *	send 'message\0' from stdin
	 *  read the answer (size|message)
	 *  write the message to stdout
	 */
	int socket_fd;
	struct sockaddr_un sv_addr;
	struct sockaddr_un cl_addr;
	int sv_addr_len;
	int cnt;
	char out_buf [MAX_MSG_SIZE] = {0,};
	char * in_buf = NULL;
	int msg_sz;
	struct pollfd fds;

	socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if(socket_fd < 0){
		snprintf(err_msg,ERR_MSG_SIZE,"Can`t create socket (%s)\n",strerror(errno));
		goto __exit_fail;
	}

	memset(&cl_addr, 0, sizeof(cl_addr));
	cl_addr.sun_family = AF_UNIX;
	strcpy(cl_addr.sun_path, "/var/svd/svd_if.XXXXXX");
	if(mkstemp(cl_addr.sun_path) < 0){
		snprintf(err_msg,ERR_MSG_SIZE,"Unable to generate temporary file %s (%s)\n",
				cl_addr.sun_path, strerror(errno));
		goto __close;
	}
	int err=unlink(cl_addr.sun_path);
	if(err < 0 && errno!=ENOENT){
		snprintf(err_msg,ERR_MSG_SIZE,"Unable to remove local controlsocket %s (%s)\n",
				cl_addr.sun_path, strerror(errno));
		goto __close;
	}

	if(-1== bind(socket_fd, (struct sockaddr*)&cl_addr, SUN_LEN(&cl_addr))){
		snprintf(err_msg,ERR_MSG_SIZE,"Can`t bind %s (%s)\n",
				cl_addr.sun_path, strerror(errno));
		goto __close;
	}

	memset(&sv_addr, 0, sizeof(sv_addr));
	sv_addr.sun_family = AF_UNIX;
	strcpy(sv_addr.sun_path, SOCKET_PATH SOCKET_NAME);
	sv_addr_len = sizeof(sv_addr);

	memset(out_buf, 0, sizeof(out_buf));
	cnt = read(0, out_buf, sizeof(out_buf));
	if(out_buf[cnt-1] == '\n'){
		out_buf[cnt-1] = '\0';
	}

	cnt = sendto(socket_fd, out_buf, strlen(out_buf)+1, 0,
			(struct sockaddr * __restrict__)&sv_addr, SUN_LEN(&sv_addr) );
	if(cnt == -1){
		snprintf(err_msg,ERR_MSG_SIZE,"client sending error (%s)\n",strerror(errno));
		goto __close;
	}

	/* waiting for answer */
	memset (&fds, 0, sizeof(fds));
	fds.fd = socket_fd;
	fds.events = POLLIN;
	if(-1== poll (&fds, 1, -1)){
		snprintf (err_msg, ERR_MSG_SIZE, "poll: %s", strerror(errno));
		goto __close;
	}
	if( (fds.revents & POLLHUP) ||
		(fds.revents & POLLERR) ||
		(fds.revents & POLLNVAL)){
		snprintf (err_msg, ERR_MSG_SIZE, "poll: bad revents 0x%X", fds.revents);
		goto __close;
	}
	/* we have some data in socket */
	ioctl (socket_fd, FIONREAD, &msg_sz);

	/* read the message */
	in_buf = malloc (msg_sz * sizeof(*in_buf));
	if( !in_buf){
		snprintf (err_msg, ERR_MSG_SIZE, "malloc: not enough memory");
		goto __close;
	}
	memset (in_buf, 0, msg_sz * sizeof(*in_buf));
	cnt = recvfrom (socket_fd, in_buf, msg_sz, 0,
			(struct sockaddr * __restrict__)&sv_addr, &sv_addr_len);
	if(cnt == -1){
		goto __malloc;
	}

	/* drop the answer to stdout */
	printf("%s",in_buf);

	free (in_buf);
	close (socket_fd);
	unlink (cl_addr.sun_path);
	return 0;
__malloc:
	free (in_buf);
__close:
	close (socket_fd);
	unlink (cl_addr.sun_path);
__exit_fail:
	return -1;
}/*}}}*/

/** Parse the message.*/
int
svd_if_srv_parse (char const * const str, struct svdif_msg_s * const msg,
		char * const err_msg)
{/*{{{*/
	struct svdif_comm_msg_s {
		char const * cmd;
		enum ch_t_e ch_df;
		enum msg_fmt_e fmt_df;
	} cmdtps[] = {
		{"not_a_command", ch_t_NONE  , msg_fmt_CLI},
		{"jbt",           ch_t_NONE  , msg_fmt_CLI},
		{"get_jb_stat",   ch_t_ACTIVE, msg_fmt_JSON},
		{"get_rtcp_stat", ch_t_ACTIVE, msg_fmt_JSON},
		{"shutdown",      ch_t_NONE  , msg_fmt_CLI},
		{"get_regs",      ch_t_NONE  , msg_fmt_JSON},
	};
	char pstr[MAX_MSG_SIZE] = {0,};
	char *command = NULL;
	int i;

	/* for show the string and error in caller function */
	strcpy(pstr,str);

	/* get command */
	command = strtok(pstr,"[");
	if( !command){
		snprintf(err_msg,ERR_MSG_SIZE,"no command in msg\n");
		goto __exit_fail;
	}
	for (i=0; i<msg_type_COUNT; i++){
		if( !strcmp(cmdtps[i].cmd, command)){
			msg->type = i;
			break;
		}
	}
	if(msg->type == msg_type_NONE){
		snprintf(err_msg,ERR_MSG_SIZE,"unknown command '%s'\n",command);
		goto __exit_fail;
	}

	/* get args if necessary */
	if( (msg->type == msg_type_GET_JB_STAT_TOTAL) ||
		(msg->type == msg_type_REGISTRATIONS) ||
		(msg->type == msg_type_SHUTDOWN)){
		/* nothing to do */
	} else {/* jb/rtcp stat */
		/* args : [chan;fmt] */
		/* get chan */
		char * arg = strtok(NULL, ";");
		if( !arg){
			snprintf(err_msg,ERR_MSG_SIZE,"no channel given\n");
			goto __exit_fail;
		} else if( !strcmp(arg,ARG_DEF)){
			msg->ch_sel.ch_t = cmdtps[msg->type].ch_df;
		} else if( !strcmp(arg,CHAN_ALL)){
			msg->ch_sel.ch_t = ch_t_ALL;
		} else if( !strcmp(arg,CHAN_ACT)){
			msg->ch_sel.ch_t = ch_t_ACTIVE;
		} else {
			/* get one number */
			msg->ch_sel.ch_t = ch_t_ONE;
			msg->ch_sel.ch_if_one = strtol(arg,NULL,10);
		}
		/* get format */
		arg = strtok(NULL, "]");
		if( !arg){
			snprintf(err_msg,ERR_MSG_SIZE,"no format given\n");
			goto __exit_fail;
		} else if( !strcmp(arg,ARG_DEF)){
			msg->fmt_sel = cmdtps[msg->type].fmt_df;
		} else if( !strcmp(arg,FMT_CLI)){
			msg->fmt_sel = msg_fmt_CLI;
		} else if( !strcmp(arg,FMT_JSON)){
			msg->fmt_sel = msg_fmt_JSON;
		} else {
			snprintf(err_msg,ERR_MSG_SIZE,"unknown format '%s' given\n",arg);
			goto __exit_fail;
		}
	}
	return 0;
__exit_fail:
	return -1;
}/*}}}*/
