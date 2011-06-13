/**
 * @file svd.c
 * Main file of the project.
 * It containes main initializtions and main cycle start.
 * */

/* Includes {{{ */
#include "svd.h"
#include "svd_cfg.h"
#include "svd_ua.h"
#include "svd_atab.h"
#include "svd_server_if.h"
#include "svd_if.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <syslog.h>
#include <signal.h>
#include <netinet/ip.h>
#include <errno.h>
/*}}}*/

/** Name of the daemon (using in logs).*/
#define DAEMON_NAME "svd"

unsigned int g_f_cnt= 0;
unsigned int g_f_offset = 0;

/** Switch to daemon mode.*/
static int 	svd_daemonize(void);
/** Create svd structure.*/
static svd_t * svd_create(ab_t * const ab);
/** Destroy svd structure.*/
static void svd_destroy( svd_t ** svd );
/** Logging function.*/
static void svd_logger(void *logarg, char const *format, va_list ap);
/** Set logging function.*/
static void svd_log_set( int const level, int const debug);

/* svd pointer for termination handler */
static svd_t * main_svd;
/* termination handler */
static void term_handler(int signum)
{
  svd_shutdown(main_svd);
}

/**
 * Main.
 *
 * \param[in] argc 	arguments count
 * \param[in] argv 	arguments values
 * \retval 0 	etherything is fine
 * \retval -1 	error occures
 * \remark
 *		In real it shold never returns if etherything is fine
 *		because of main cycle.
 */
int
main (int argc, char ** argv)
{/*{{{*/
	svd_t *svd;
	ab_t * ab = NULL;
	int err = 0;
	int nothing_to_do;

	nothing_to_do = startup_init( argc, argv );
	if( nothing_to_do ){
		goto __startup;
	}

	/* daemonization */
	err = svd_daemonize ();
	if(err){
		goto __startup;
	}
	/* the daemon from now */

	su_init();

	/* preliminary log settings */
	if(g_so.debug_level == -1){
		/* debug do not set */
		openlog( DAEMON_NAME, LOG_PID, LOG_LOCAL5 );
		syslog( LOG_INFO, "starting" );
		svd_log_set (0,0);
	} else {
		/* debug to stderr is set */
		svd_log_set (g_so.debug_level, 1);
	}

	/* init hardware structures */
	ab = ab_create();
	if( !ab){
		SU_DEBUG_0 ((LOG_FNC_A(ab_g_err_str)));
		goto __su;
	}

	/* create svd structure */
	/* uses !!g_conf */
	svd = svd_create (ab);
	if (svd == NULL) {
		goto __conf;
	}

	/* create interface */
	err = svd_create_interface(svd);
	if(err){
		goto __if;
	}

	/* set termination handler to shutdown svd */
	main_svd = svd;
	signal(SIGTERM, term_handler);
	
	/* run main cycle */
	su_root_run (svd->root);

__if:
	svd_destroy_interface(svd);
	svd_destroy (&svd);
__conf:
	svd_conf_destroy ();
	ab_destroy (&ab);
__su:
	su_deinit ();
	syslog( LOG_NOTICE, "terminated" );
	closelog();
__startup:
	startup_destroy (argc, argv);
	return err;
}/*}}}*/

/**
 * Switch main process to daemon mode.
 *
 * \retval 0 	etherything is fine
 * \retval -1 	error occures
 * \remark
 * 		Error messages will pass to stderr if debug is on,
 * 		otherwise, stderr redirect to /dev/null as stdout and stdin
 */
static int
svd_daemonize (void)
{/*{{{*/
	pid_t pid;
	pid_t sid;

	/* Fork off the parent process */
	pid = fork();
	if (pid < 0) {
		fprintf(stderr, "unable to fork daemon, code=%d (%s)\n",
				errno, strerror(errno));
		goto __exit_fail;
	}

	/* If we got a good PID, then we can exit the parent process. */
	if (pid > 0) {
		exit(EXIT_SUCCESS);
	}

	/* At this point we are executing as the child process */

	/* Cancel certain signals */
	signal(SIGCHLD,SIG_DFL); /* A child process dies */
	signal(SIGTSTP,SIG_IGN); /* Various TTY signals */
	signal(SIGTTOU,SIG_IGN);
	signal(SIGTTIN,SIG_IGN);
	signal(SIGHUP, SIG_IGN); /* Ignore hangup signal */
	signal(SIGTERM,SIG_DFL); /* Die on SIGTERM */

	/* Change the file mode mask */
	umask(0);

	/* Create a new SID for the child process */
	sid = setsid();
	if (sid < 0) {
		fprintf(stderr,"unable to create a new session, code %d (%s)\n",
				errno, strerror(errno));
		goto __exit_fail;
	}

	/* Change the current working directory.  This prevents the current
	directory from being locked; hence not being able to remove it. */
	if ((chdir("/")) < 0) {
		fprintf(stderr,"unable to change directory to %s, code %d (%s)\n",
				"/", errno, strerror(errno));
		goto __exit_fail;
	}

	/* Redirect standard files to /dev/null */
	freopen( "/dev/null", "r", stdin);
	freopen( "/dev/null", "w", stdout);
	if(g_so.debug_level == -1){
		freopen( "/dev/null", "w", stderr);
	}

	return 0;

__exit_fail:
	return -1;
}/*}}}*/

/**
 * Create the svd structure with all appropriate initializations.
 *
 * \retval NULL 			something nasty happens
 * \retval valid_pointer 	new svd structure
 * \remark
 *		It init`s the internal ab structure
 *  	It calls all appropriate functions to create sofia-sip objects
 *  	It uses \ref g_conf values
 */
static svd_t *
svd_create (ab_t * const ab)
{/*{{{*/
	svd_t * svd;
	int tos;
	int err;
DFS
	svd = malloc( sizeof(*svd) );
	if ( !svd) {
    		SU_DEBUG_0 (("svd_create() not enough memory\n"));
		goto __exit_fail;
	}

	memset (svd, 0, sizeof(*svd));

	/* svd home initialization */
	if(su_home_init(svd->home) != 0){
    		SU_DEBUG_0 (("svd_create() su_home_init() failed\n"));
		goto __exit_fail;
	}
	
	/* read svd *.conf files */
	err = svd_conf_init (ab, svd->home);
	if (err){
		goto __exit_fail;
	}

	/* change log level, if it is not debug mode, from config sets */
	if (g_so.debug_level == -1){
		svd_log_set (g_conf.log_level, 0);
	}

	/* extended SIP parser */
	if(sip_update_default_mclass(sip_extend_mclass(NULL)) < 0) {
		SU_DEBUG_0 (("svd_create() sip_update_default_mclass() failed\n"));
		goto __exit_fail;
	}
	
	/* svd root creation */
	svd->root = su_root_create (svd);
	if (svd->root == NULL) {
    		SU_DEBUG_0 (("svd_create() su_root_create() failed\n"));
		goto __exit_fail;
	}

	/* init svd->ab with existing structure */
	svd->ab = ab;

	/* create ab structure of svd and handle callbacks */
	/* uses !!g_cnof */
	err = svd_atab_create (svd);
	if( err ) {
		goto __exit_fail;
	}

	/* launch the SIP stack */
	/* *
	 * NUTAG_AUTOANSWER (1)
	 * NUTAG_PROXY (),
	 * NUTAG_AUTH ("scheme""realm""user""password"),
	 * NUTAG_AUTHTIME (3600),
	 * NUTAG_M_DISPLAY (),
	 * */
	//tos = g_conf.sip_tos & IPTOS_TOS_MASK;
	tos = g_conf.sip_tos & 0xFF;
	svd->nua = nua_create (svd->root, svd_nua_callback, svd,
			SIPTAG_USER_AGENT_STR ("svd VoIP agent"),
			SOATAG_AF (SOA_AF_IP4_IP6),
			TPTAG_TOS (tos),
	 		NUTAG_ALLOW ("INFO"),
			NUTAG_AUTOALERT (1),
			NUTAG_ENABLEMESSAGE (1),
			NUTAG_ENABLEINVITE (1),
			NUTAG_DETECT_NETWORK_UPDATES (NUA_NW_DETECT_TRY_FULL), 
			TAG_NULL () );
	if (!svd->nua) {
		SU_DEBUG_0 (("Network is not initialized\n"));
		goto __exit_fail;
	}

	nua_set_params(svd->nua,
		      NUTAG_OUTBOUND ("gruuize no-outbound validate "
				      "natify use-rport options-keepalive"),
		      TAG_NULL () );

	svd_refresh_registration (svd);
	nua_get_params(svd->nua, TAG_ANY(), TAG_NULL());
DFE
	return svd;
__exit_fail:
DFE
	if(svd){
		svd_destroy (&svd);
	}
	return NULL;
}/*}}}*/

/**
 * Correct destroy function for svd structure
 *
 * \param[in] svd 	pointer to pointer to svd structure
 * 		that should be destroyed
 * \remark
 * 		It destroy the internal ab structure
 * 		It calls all appropriate functions to destroy sofia-sip objects
 * 		It destroys the structure and sets the pointer *svd to NULL
 */
static void
svd_destroy( svd_t ** svd )
{/*{{{*/
DFS
	if(*svd){
		svd_atab_delete (*svd);

		if((*svd)->nua){
			svd_shutdown (*svd);
		}
		if((*svd)->root){
			su_root_destroy ((*svd)->root);
		}
		if((*svd)->home){
			su_home_deinit ((*svd)->home);
		}

		free (*svd);
		*svd = NULL;
	}
DFE
}/*}}}*/

/**
 * Logging callback function
 *
 * \param[in] logarg 	debug value or (-1) if logging is off
 * \param[in] format 	message format (internal sofia log value)
 * \param[in] ap 		message arguments (internal sofia log value)
 * \remark
 *		It calls for every log action and make a decision what to do.
 */
static void
svd_logger(void *logarg, char const *format, va_list ap)
{/*{{{*/
	if( (int)logarg == -1){
		/* do not log anything */
		return;
	} else if ( (int)logarg ) {
		/* debug is on - log to stderr */
		vfprintf(stderr, format, ap);
	} else {
		/* debug is off - standart log */
		vsyslog (LOG_INFO, format, ap);
	}
}/*}}}*/

/**
 * Sets the log configuration
 *
 * \param[in] level
 *		\arg \c -1 - do not log anything
 *		\arg \c 0 - very pure logs to \c 9 - very verbose
 * \param[in] debug
 *		\arg \c 1 - log to stderr or
 *		\arg \c 0 - log to jornal
 * \remark
 *		It attaches the callback logger function with proper params and uses
 *		sofia sip logging system
 */
static void
svd_log_set( int const level, int const debug)
{/*{{{*/
	if (level == -1){
 		/* do not log anything */
		su_log_set_level (NULL, 0);
		su_log_redirect (NULL, svd_logger, (void*)-1);
	} else {
		su_log_set_level (NULL, level);
		su_log_redirect (NULL,svd_logger,(void*)debug);
	}
}/*}}}*/

