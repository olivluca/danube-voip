/**
 * @file svd_if.c
 * Main file of the svd_interface programm.
 * It containes options parsing and engine start.
 * */

/* Includes {{{ */
#include "svd_if.h"
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
/*}}}*/

/**
 * Show help message.
 */
static void
show_help( void )
{/*{{{*/
	fprintf( stdout,
"\
Usage: %s [OPTION]\n\
SIP VoIP User agent interface.\n\
\n\
Mandatory arguments to long options are mandatory for short options too.\n\
  -h, --help         display this help and exit\n\
  -V, --version      displey current version and license info\n\
\n\
	All commands should be put in STDIN:\n\
	get_jb_stat[chan_N/all/act/*;*]\n\
	get_rtcp_stat[chan_N/all/act/*;*]\n\
	shutdown[]\n\
	get_regs[]\n\
	Execution example :\n\
	echo \'get_jb_stat[4;*]\' %s\n\
	Means, that you want to get jitter buffer statistics from the\n\
			fourth channel.\n\
	echo \'get_rtcp_stat[*;*]\' %s\n\
	Means, that you want to get RTCP statistics from the all currently\n\
			active channels.\n\
\n\
Report bugs to <%s>.\n\
"
		, "svd_if", "svd_if", "luca@ventoso.org");
}/*}}}*/

/**
 * Show program version, built info and license.
 */
static void
show_version( void )
{/*{{{*/
	fprintf( stdout,
"\
%s-%s, built [%s]-[%s]\n\n\
Copyright (C) 2007 Free Software Foundation, Inc.\n\
This is free software.  You may redistribute copies of it under the terms of\n\
the GNU General Public License <http://www.gnu.org/licenses/gpl.html>.\n\
There is NO WARRANTY, to the extent permitted by law.\n\
\n\
Written by Vladimir Luchko. <%s>\n\
"
		, "svd_if", "0.1",
		__DATE__, __TIME__, "vlad.luch@mail.ru");
}/*}}}*/

/**
 * Main.
 * \param[in] argc 	arguments count
 * \param[in] argv 	arguments values
 * \retval 0 	etherything is fine
 * \retval -1 	error occures
 */
int
main (int argc, char ** argv)
{/*{{{*/
	int err = 0;
	int option_IDX;
	int option_rez;
	char * short_options = "hV";
	char err_msg [ERR_MSG_SIZE] = {0,};
	struct option long_options[ ] = {
		{ "help", no_argument, NULL, 'h' },
		{ "version", no_argument, NULL, 'V' },
		{ NULL, 0, NULL, 0 }
		};

	while ((option_rez = getopt_long ( argc, argv, short_options,
			long_options, &option_IDX)) != -1) {
		if      ((option_rez == 'h') ||
				 (option_rez == '?')){
			show_help();
		} else if(option_rez == 'V'){
			show_version();
		}
		goto __exit;
	}

	/* start engine clien part */
	err = svd_if_cli_start (err_msg);
	if(err){
		fprintf( stderr, "%s : %s\n", "svd_if", err_msg);
	}

__exit:
	return err;
}/*}}}*/

