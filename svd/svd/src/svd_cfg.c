/**
 * @file svd_cfg.c
 * Configuration implementation.
 * It contains startup \ref g_so and main \ref g_conf
 * 		configuration features implementation.
 */

/*Includes {{{*/
#include "svd.h"
#include "libconfig.h"
#include "svd_cfg.h"
#include "svd_log.h"

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <errno.h>

#include <getopt.h>
/*}}}*/

/** @defgroup STARTUP_I Stratup internals.
 *  @ingroup STARTUP
 *  Internal startup definitons.
 *  @{*/
#define ERR_SUCCESS 0
#define ERR_MEMORY_FULL 1
#define ERR_UNKNOWN_OPTION 2
static unsigned char g_err_no;
/** @}*/


/** @defgroup CFG_N Config file text values.
 *  @ingroup CFG_M
 *  Text values that can be in config file.
 *  @{*/
#define CONF_CODEC_G729 "g729"
#define CONF_CODEC_ALAW "aLaw"
#define CONF_CODEC_G723 "g723"
#define CONF_CODEC_ILBC133 "iLBC_133"
/*#define CONF_CODEC_ILBC152 "iLBC_152"*/
#define CONF_CODEC_G729E "g729e"
#define CONF_CODEC_G72616 "g726_16"
#define CONF_CODEC_G72624 "g726_24"
#define CONF_CODEC_G72632 "g726_32"
#define CONF_CODEC_G72640 "g726_40"
#define CONF_CODEC_BITPACK_RTP "rtp"
#define CONF_CODEC_BITPACK_AAL2 "aal2"
#define CONF_JB_TYPE_FIXED "fixed"
#define CONF_JB_TYPE_ADAPTIVE "adaptive"
#define CONF_JB_LOC_OFF "off"
#define CONF_JB_LOC_ON "on"
#define CONF_JB_LOC_SI "SI"
#define CONF_VAD_ON "on"
#define CONF_VAD_NOVAD "off"
#define CONF_VAD_G711 "g711"
#define CONF_VAD_CNG_ONLY "CNG_only"
#define CONF_VAD_SC_ONLY "SC_only"
#define CONF_WLEC_TYPE_OFF "off"
#define CONF_WLEC_TYPE_NE  "NE"
#define CONF_WLEC_TYPE_NFE "NFE"
#define CONF_WLEC_NLP_ON   "on"
#define CONF_WLEC_NLP_OFF  "off"
#define CONF_FXO_PULSE   "pulse"
#define CONF_FXO_TONE    "tone"
#define CONF_VF_TRANSIT "transit"
#define CONF_VF_NORMAL  "normal"
#define CONF_VF_2_WIRED "2w"
#define CONF_VF_4_WIRED "4w"
/** @}*/

/** @defgroup CFG_IF Config internal functions.
 *  @ingroup CFG_M
 *  This functions using while reading config file.
 *  @{*/
/** Init self router ip and number.*/
static int self_values_init (void);
/** Init codec_t from rec_set. */
static void init_codec_el(struct config_setting_t const * const rec_set,
		int const prms_offset, codec_t * const cod);
/** Init main configuration.*/
static int main_init (ab_t const * const ab);
/** Init fxo channels parameters.*/
static int fxo_init (ab_t const * const ab);
/** Init route table configuration.*/
static int routet_init (void);
/** Init AUDIO parameters configuration.*/
static int audio_init (void);
/** Init WLEC parameters configuration.*/
static int wlec_init (ab_t const * const ab);
/** Init voice frequency channels configuration.*/
static int voicef_init (ab_t const * const ab);
/** Init hot line configuration.*/
static int hotline_init (ab_t const * const ab);
/** Init addres book configuration.*/
static int addressb_init (void);
/** Init internal, external and fax codecs configuration.*/
static int codecs_init (void);
/** Print error message if something occures.*/
static void error_message (void);
/** Print help message.*/
static void show_help (void);
/** Print version message.*/
static void show_version (void);
/** @}*/

/**
 * Init startup parameters structure \ref g_so.
 *
 * \param[in] argc parameters count.
 * \param[in] argv parameters values.
 * \retval 0 if etherything ok
 * \retval error_code if something nasty happens
 * 		\arg \ref ERR_MEMORY_FULL - not enough memory
 * 		\arg \ref ERR_UNKNOWN_OPTION - bad startup option.
 * \remark
 *		It sets help, version and debug tags to \ref g_so struct.
 */
int
startup_init( int argc, char ** argv )
{/*{{{*/
	int option_IDX;
	int option_rez;
	char * short_options = "hVd:";
	struct option long_options[ ] = {
		{ "help", no_argument, NULL, 'h' },
		{ "version", no_argument, NULL, 'V' },
		{ "debug", required_argument, NULL, 'd' },
		{ NULL, 0, NULL, 0 }
		};

	g_err_no = ERR_SUCCESS;

	/* INIT WITH DEFAULT VALUES */
	g_so.help = 0;
	g_so.version = 0;
	g_so.debug_level = -1;

	/* INIT FROM SYSTEM CONFIG FILE "/etc/routine" */
	/* INIT FROM SYSTEM ENVIRONMENT */
	/* INIT FROM USER DOTFILE "$HOME/.routine" */
	/* INIT FROM USER ENVIRONMENT */
	/* INIT FROM STARTUP KEYS */

	opterr = 0;
	while ((option_rez = getopt_long (
			argc, argv, short_options,
			long_options, &option_IDX)) != -1) {
		switch( option_rez ){
			case 'h': {
				g_so.help = 1;
				return 1;
			}
			case 'V': {
				g_so.version = 1;
				return 1;
			}
			case 'd': {
				g_so.debug_level = strtol(optarg, NULL, 10);
				if(g_so.debug_level > 9){
					g_so.debug_level = 9;
				} else if(g_so.debug_level < 0){
					g_so.debug_level = 0;
				}
				return 0;
			}
			case '?' :{
				/* unknown option found */
				g_err_no = ERR_UNKNOWN_OPTION;
				return g_err_no;
			}
		}
	}
	return 0;
}/*}}}*/

/**
 * 	Destroys startup parameters structure \ref g_so.
 *
 * \param[in] argc parameters count.
 * \param[in] argv parameters values.
 * \remark
 *		It prints necessary messages.
 */
void
startup_destroy( int argc, char ** argv )
{/*{{{*/
	if( g_so.help ){
		show_help( );
	} else if( g_so.version ){
		show_version( );
	}
	error_message( );
}/*}}}*/

/**
 *	Parses configuration file and write parsed info into \ref g_conf.
 *
 * \param[in] ab ata-board hardware structure.
 *
 * \retval 0 success.
 * \retval -1 in error case.
 * \remark
 *		It init`s main routine configuration \ref g_conf.
 */
int
svd_conf_init( ab_t const * const ab )
{/*{{{*/
	/* default presets */
	memset (&g_conf, 0, sizeof(g_conf));
	strcpy (g_conf.lo_ip, "127.0.0.1");
	if(		main_init (ab) 	||
			fxo_init (ab)	||
			routet_init()	||
			audio_init()	||
			wlec_init (ab)	||
			voicef_init(ab) ||
			hotline_init(ab) ||
			addressb_init()	||
			codecs_init()
			){
		goto __exit_fail;
	}

	conf_show();
	return 0;

__exit_fail:
	conf_show();
	return -1;
}/*}}}*/

/**
 * Show config parameters.
 *
 * \remark
 * 		It`s debug feature.
 */
void
conf_show( void )
{/*{{{*/
	struct adbk_record_s * curr_ab_rec;
	struct hot_line_s    * curr_hl_rec;
	struct rttb_record_s * curr_rt_rec;
	struct voice_freq_s  * curr_vf_rec;
	int i;
	int j;

	SU_DEBUG_3(("=========================\n"));
	SU_DEBUG_3(("%s[%s] : ", g_conf.self_number, g_conf.self_ip));
	SU_DEBUG_3(("log["));

	if( g_conf.log_level == -1 ){
		SU_DEBUG_3(("no] : "));
	} else {
		SU_DEBUG_3(("%d] : ", g_conf.log_level));
	}
	SU_DEBUG_3(("ports[%d:%d]\n",
			g_conf.rtp_port_first,
			g_conf.rtp_port_last));

	for (i=0; g_conf.int_codecs[i].type != cod_type_NONE; i++){
		SU_DEBUG_3(("t:%d/sz%d/pt:0x%X__[%d:%d]::[%d:%d:%d:%d]\n",
				g_conf.int_codecs[i].type,
				g_conf.int_codecs[i].pkt_size,
				g_conf.int_codecs[i].user_payload,
				g_conf.int_codecs[i].jb.jb_type,
				g_conf.int_codecs[i].jb.jb_loc_adpt,
				g_conf.int_codecs[i].jb.jb_scaling,
				g_conf.int_codecs[i].jb.jb_init_sz,
				g_conf.int_codecs[i].jb.jb_min_sz,
				g_conf.int_codecs[i].jb.jb_max_sz
				));
	}

	SU_DEBUG_3(("SIP net : %d\n",g_conf.sip_set.all_set));
	if(g_conf.sip_set.all_set){
		SU_DEBUG_3((	"\tCodecs:\n"));
		for (i=0; g_conf.sip_set.ext_codecs[i].type != cod_type_NONE; i++){
			SU_DEBUG_3(("t:%d/sz%d/pt:0x%X__[%d:%d]::[%d:%d:%d:%d]\n",
					g_conf.sip_set.ext_codecs[i].type,
					g_conf.sip_set.ext_codecs[i].pkt_size,
					g_conf.sip_set.ext_codecs[i].user_payload,
					g_conf.sip_set.ext_codecs[i].jb.jb_type,
					g_conf.sip_set.ext_codecs[i].jb.jb_loc_adpt,
					g_conf.sip_set.ext_codecs[i].jb.jb_scaling,
					g_conf.sip_set.ext_codecs[i].jb.jb_init_sz,
					g_conf.sip_set.ext_codecs[i].jb.jb_min_sz,
					g_conf.sip_set.ext_codecs[i].jb.jb_max_sz
					));
		}

		SU_DEBUG_3((	"\tRegistRar   : '%s'\n"
				"\tUser/Pass   : '%s/%s'\n"
				"\tUser_URI    : '%s'\n"
				"\tSIP_channel : '%d'\n",
				g_conf.sip_set.registrar,
				g_conf.sip_set.user_name,
				g_conf.sip_set.user_pass,
				g_conf.sip_set.user_URI,
				g_conf.sip_set.sip_chan));
	}

	/* RTP parameters *//*
	for (i=0; i<CHANS_MAX; i++){
		rtp_rec = &g_conf.rtp_prms[i];
		SU_DEBUG_3(("%d: vol(%d/%d) vh(%d/%d)\n",
				i, rtp_rec->COD_Tx_vol, rtp_rec->COD_Rx_vol,
				rtp_rec->VAD_cfg, rtp_rec->HPF_is_ON));
	}
	*/
	/* PSTN parameters *//*
	for (i=0; i<CHANS_MAX; i++){
		SU_DEBUG_3(("%d: %d\n", i, g_conf.fxo_PSTN_type[i]));
	}
	*/

	if(g_conf.address_book.records_num){
		SU_DEBUG_3(("AddressBook :\n"));
	}
	j = g_conf.address_book.records_num;
	for(i = 0; i < j; i++){
		curr_ab_rec = &g_conf.address_book.records[ i ];
		SU_DEBUG_3(("\t%d/\"%s\" : %s\n",
				i+1, curr_ab_rec->id, curr_ab_rec->value));
	}

	SU_DEBUG_3(("HotLine :\n"));
	for(i=0; i<CHANS_MAX; i++){
		curr_hl_rec = &g_conf.hot_line[ i ];
		if(curr_hl_rec->is_set){
			SU_DEBUG_3(("\t%d : %s\n", i, curr_hl_rec->value ));
		}
	}

	SU_DEBUG_3(("VoiceF :\n"));
	for(i=0; i<CHANS_MAX; i++){
		curr_vf_rec = &g_conf.voice_freq[ i ];
		if( !curr_vf_rec->is_set){
			continue;
		}
		SU_DEBUG_3(("t%d/s%d/up%d__[%d:%d]::[%d:%d:%d:%d]__",
				curr_vf_rec->vf_codec.type,
				curr_vf_rec->vf_codec.pkt_size,
				curr_vf_rec->vf_codec.user_payload,
				curr_vf_rec->vf_codec.jb.jb_type,
				curr_vf_rec->vf_codec.jb.jb_loc_adpt,
				curr_vf_rec->vf_codec.jb.jb_scaling,
				curr_vf_rec->vf_codec.jb.jb_init_sz,
				curr_vf_rec->vf_codec.jb.jb_min_sz,
				curr_vf_rec->vf_codec.jb.jb_max_sz
				));

		SU_DEBUG_3(("i%d/id\"%s\":%s:%s/aic_%d\n",
				i, curr_vf_rec->id,
				curr_vf_rec->pair_route, curr_vf_rec->pair_chan,
				curr_vf_rec->am_i_caller));
	}
	if(g_conf.route_table.records_num){
		SU_DEBUG_3(("RouteTable :\n"));
	}
	j = g_conf.route_table.records_num;
	for(i = 0; i < j; i++){
		curr_rt_rec = &g_conf.route_table.records[ i ];
		SU_DEBUG_3(("\t%d/\"%s\" : %s\n",
				i+1, curr_rt_rec->id, curr_rt_rec->value));
	}
	SU_DEBUG_3(("=========================\n"));
}/*}}}*/

/**
 * Free allocated memory for main configuration structure.
 */
void
svd_conf_destroy (void)
{/*{{{*/
	int i;
	int j;
	struct hot_line_s * curr_rec;

	j = g_conf.address_book.records_num;
	if (j){
		struct adbk_record_s * curr_rec;
		for(i = 0; i < j; i++){
			curr_rec = &g_conf.address_book.records[ i ];
			if (curr_rec->id && curr_rec->id != curr_rec->id_s){
				free (curr_rec->id);
			}
			if (curr_rec->value &&
					curr_rec->value != curr_rec->value_s){
				free (curr_rec->value);
			}
		}
		free (g_conf.address_book.records);
	}

	for(i=0; i<CHANS_MAX; i++){
		curr_rec = &g_conf.hot_line[ i ];
		if (curr_rec->value && (curr_rec->value != curr_rec->value_s)){
			free (curr_rec->value);
		}
	}

	j = g_conf.route_table.records_num;
	if (j){
		struct rttb_record_s * curr_rec;
		for(i = 0; i < j; i++){
			curr_rec = &g_conf.route_table.records[ i ];
			if (curr_rec->id && curr_rec->id != curr_rec->id_s){
				free (curr_rec->id);
			}
		}
		free (g_conf.route_table.records);
	}

	memset(&g_conf, 0, sizeof(g_conf));
}/*}}}*/

/**
 * \param[out] cp codecs parameters.
 * \retval 0 Success;
 * \retval -1 Fail;
 * \remark
 * 	\c cp should be a pointer on the allocated memory for \c COD_MAS_SIZE
 * 	elements.
 */
int
svd_init_cod_params( cod_prms_t * const cp )
{/*{{{*/
	int i;
	memset(cp, 0, sizeof(*cp)*COD_MAS_SIZE);

	for (i=0; i<COD_MAS_SIZE; i++){
		cp[i].type = cod_type_NONE;
	}

	i=0;

	/* G711 ALAW parameters. */
	cp[i].type = cod_type_ALAW;
	if(strlen("PCMA") >= COD_NAME_LEN){
		goto __exit_fail;
	}
	strcpy(cp[i].sdp_name, "PCMA");
	cp[i].rate = 8000;
	i++;

	/* G729 parameters. */
	cp[i].type = cod_type_G729;
	if(strlen("G729") >= COD_NAME_LEN){
		goto __exit_fail;
	}
	strcpy(cp[i].sdp_name, "G729");
	cp[i].rate = 8000;
	i++;

	/* G729E parameters. */
	cp[i].type = cod_type_G729E;
	if(strlen("G729E") >= COD_NAME_LEN){
		goto __exit_fail;
	}
	strcpy(cp[i].sdp_name, "G729E");
	cp[i].rate = 8000;
	i++;

	/* G723 parameters. */
	cp[i].type = cod_type_G723;
	if(strlen("G723") >= COD_NAME_LEN){
		goto __exit_fail;
	}
	strcpy(cp[i].sdp_name, "G723");
	cp[i].rate = 8000;
	i++;

	/* iLBC_133 parameters. */
	cp[i].type = cod_type_ILBC_133;
	if(     strlen("iLBC") >= COD_NAME_LEN ||
			strlen("mode=30") >= FMTP_STR_LEN){
		goto __exit_fail;
	}
	strcpy(cp[i].sdp_name, "iLBC");
	strcpy(cp[i].fmtp_str, "mode=30");
	cp[i].rate = 8000;
	i++;

	/* iLBC_152 parameters.
	cp[i].type = cod_type_ILBC_152;
	if(     strlen("iLBC") >= COD_NAME_LEN ||
			strlen("mode=20") >= FMTP_STR_LEN){
		goto __exit_fail;
	}
	strcpy(cp[i].sdp_name, "iLBC");
	strcpy(cp[i].fmtp_str, "mode=20");
	cp[i].rate = 8000;
	i++;
	*/

	/* G726_16 parameters. */
	cp[i].type = cod_type_G726_16;
	if(strlen("G726-16") >= COD_NAME_LEN){
		goto __exit_fail;
	}
	strcpy(cp[i].sdp_name, "G726-16");
	cp[i].rate = 8000;
	i++;

	/* G726_ parameters. */
	cp[i].type = cod_type_G726_24;
	if(strlen("G726-24") >= COD_NAME_LEN){
		goto __exit_fail;
	}
	strcpy(cp[i].sdp_name, "G726-24");
	cp[i].rate = 8000;
	i++;

	/* G726_ parameters. */
	cp[i].type = cod_type_G726_32;
	if(strlen("G726-32") >= COD_NAME_LEN){
		goto __exit_fail;
	}
	strcpy(cp[i].sdp_name, "G726-32");
	cp[i].rate = 8000;
	i++;

	/* G726_ parameters. */
	cp[i].type = cod_type_G726_40;
	if(strlen("G726-40") >= COD_NAME_LEN){
		goto __exit_fail;
	}
	strcpy(cp[i].sdp_name, "G726-40");
	cp[i].rate = 8000;

	return 0;
__exit_fail:
	return -1;
}/*}}}*/

/**
 * 	Init`s self ip and number settings in main routine configuration
 *
 * \retval 0 success.
 * \retval -1 fail.
 * \remark
 * 	It should be run after route table initialization because
 * 	it uses this value for self values pointed to.
 */
static int
self_values_init( void )
{/*{{{*/
	char ** addrmas = NULL;
	int addrs_count;
	int route_records_num;
	struct rttb_record_s * curr_rec;
	int i;
	int j = 0;
	int sock;
	int self_found;

	/* get interfaces addresses on router */
	sock = socket (PF_INET, SOCK_STREAM, 0);
	if(sock == -1){
		SU_DEBUG_0 ((LOG_FNC_A(strerror(errno))));
		goto __exit_fail;
	}

	for (i=1;;i++){
		struct ifreq ifr;
		struct sockaddr_in *sin = (struct sockaddr_in *) &ifr.ifr_addr;
		char *ip;

		ifr.ifr_ifindex = i;
		if (ioctl (sock, SIOCGIFNAME, &ifr) < 0){
			break;
		}
		if (ioctl (sock, SIOCGIFADDR, &ifr) < 0){
			continue;
		}

		ip = inet_ntoa (sin->sin_addr);
		if( strcmp(ifr.ifr_name, "lo") ){
			addrmas = realloc (addrmas, sizeof(*addrmas)*(j+1));
			addrmas[j] = malloc(sizeof(char) * IP_LEN_MAX);
			memset(addrmas[j], 0, sizeof(char) * IP_LEN_MAX);
			strcpy(addrmas[j], ip);
			j++;
		}
	}
	close (sock);
	addrs_count = j;

	/* set self_ip and self_number according to route table and addrmas */
	self_found = 0;
	route_records_num = g_conf.route_table.records_num;
	for (i=0; i<route_records_num; i++){
		curr_rec = &g_conf.route_table.records[i];
		for (j=0; j<addrs_count; j++){
			if (!strcmp (addrmas[j], curr_rec->value) ){
				g_conf.self_ip = curr_rec->value;
				g_conf.self_number = curr_rec->id;
				self_found = 1;
			}
		}
	}

	/* free addrmas */
	for(i =0; i <addrs_count; i++){
		free (addrmas[i]);
	}
	free (addrmas);

	if( !self_found){
		SU_DEBUG_0 ((LOG_FNC_A("ERROR: "
				"No interfaces found with ip from route table")));
		goto __exit_fail;
	}
	return 0;
__exit_fail:
	return -1;
}/*}}}*/

/**
 * Initilize one codec element from appropriate config setting.
 *
 * \param[in] rec_set config setting.
 * \param[in] prms_offset from there starts the codec params in offset.
 * \param[out] cod codec_t element to initilize.
 */
static void
init_codec_el(struct config_setting_t const *const rec_set,
		int const prms_offset, codec_t *const cod)
{/*{{{*/
	char const * codel = NULL;
	float scal;

	/* codec type */
	codel = config_setting_get_string_elem (rec_set, prms_offset);
	if       ( !strcmp(codel, CONF_CODEC_G729)){
		cod->type = cod_type_G729;
	} else if( !strcmp(codel, CONF_CODEC_ALAW)){
		cod->type = cod_type_ALAW;
	} else if( !strcmp(codel, CONF_CODEC_G723)){
		cod->type = cod_type_G723;
	} else if( !strcmp(codel, CONF_CODEC_ILBC133)){
		cod->type = cod_type_ILBC_133;
		/*
	} else if( !strcmp(codel, CONF_CODEC_ILBC152)){
		cod->type = cod_type_ILBC_152;
		*/
	} else if( !strcmp(codel, CONF_CODEC_G729E)){
		cod->type = cod_type_G729E;
	} else if( !strcmp(codel, CONF_CODEC_G72616)){
		cod->type = cod_type_G726_16;
	} else if( !strcmp(codel, CONF_CODEC_G72624)){
		cod->type = cod_type_G726_24;
	} else if( !strcmp(codel, CONF_CODEC_G72632)){
		cod->type = cod_type_G726_32;
	} else if( !strcmp(codel, CONF_CODEC_G72640)){
		cod->type = cod_type_G726_40;
	}

	/* codec packet size */
	codel = config_setting_get_string_elem (rec_set, prms_offset+1);
	if       ( !strcmp(codel, "2.5")){
		cod->pkt_size = cod_pkt_size_2_5;
	} else if( !strcmp(codel, "5")){
		cod->pkt_size = cod_pkt_size_5;
	} else if( !strcmp(codel, "5.5")){
		cod->pkt_size = cod_pkt_size_5_5;
	} else if( !strcmp(codel, "10")){
		cod->pkt_size = cod_pkt_size_10;
	} else if( !strcmp(codel, "11")){
		cod->pkt_size = cod_pkt_size_11;
	} else if( !strcmp(codel, "20")){
		cod->pkt_size = cod_pkt_size_20;
	} else if( !strcmp(codel, "30")){
		cod->pkt_size = cod_pkt_size_30;
	} else if( !strcmp(codel, "40")){
		cod->pkt_size = cod_pkt_size_40;
	} else if( !strcmp(codel, "50")){
		cod->pkt_size = cod_pkt_size_50;
	} else if( !strcmp(codel, "60")){
		cod->pkt_size = cod_pkt_size_60;
	}

	/* codec payload type */
	cod->user_payload = config_setting_get_int_elem (rec_set, prms_offset+2);

	/* codec bitpack */
	codel = config_setting_get_string_elem (rec_set, prms_offset+3);
	if       ( !strcmp(codel, CONF_CODEC_BITPACK_RTP)){
		cod->bpack = bitpack_RTP;
	} else if( !strcmp(codel, CONF_CODEC_BITPACK_AAL2)){
		cod->bpack = bitpack_AAL2;
	}

	/* jb type */
	codel = config_setting_get_string_elem (rec_set, prms_offset+4);
	if       ( !strcmp(codel, CONF_JB_TYPE_FIXED)){
		cod->jb.jb_type = jb_type_FIXED;
	} else if( !strcmp(codel, CONF_JB_TYPE_ADAPTIVE)){
		cod->jb.jb_type = jb_type_ADAPTIVE;
	}

	/* local adaptation type */
	codel = config_setting_get_string_elem (rec_set, prms_offset+5);
	if       ( !strcmp(codel, CONF_JB_LOC_OFF)){
		cod->jb.jb_loc_adpt = jb_loc_adpt_OFF;
	} else if( !strcmp(codel, CONF_JB_LOC_ON)){
		cod->jb.jb_loc_adpt = jb_loc_adpt_ON;
	} else if( !strcmp(codel, CONF_JB_LOC_SI)){
		cod->jb.jb_loc_adpt = jb_loc_adpt_SI;
	}

	/* scaling factor */
	scal = config_setting_get_float_elem(rec_set, prms_offset+6);
	if((int)scal == 0){
		cod->jb.jb_scaling = config_setting_get_int_elem(rec_set,prms_offset+6)*16;
	} else {
		cod->jb.jb_scaling = scal * 16;
	}
	if(cod->jb.jb_scaling == 0){
		cod->jb.jb_scaling = 255;
	}

	/* buffer limitations */
	cod->jb.jb_init_sz= config_setting_get_int_elem(rec_set, prms_offset+7)*8;
	cod->jb.jb_min_sz = config_setting_get_int_elem(rec_set, prms_offset+8)*8;
	cod->jb.jb_max_sz = config_setting_get_int_elem(rec_set, prms_offset+9)*8;
}/*}}}*/

/**
 * Init`s main settings in main routine configuration \ref g_conf structure.
 *
 * \param[in] ab ata-board hardware structure.
 *
 * \retval 0 success.
 * \retval -1 fail.
 */
static int
main_init( ab_t const * const ab )
{/*{{{*/
	struct config_t cfg;
	char const * str_elem = NULL;
	int err;

	config_init (&cfg);

	/* Load the file */
	if (!config_read_file (&cfg, MAIN_CONF_NAME)){
		err = config_error_line (&cfg);
		SU_DEBUG_0(("%s(): Config file syntax error in line %d\n",
				__func__, err));
		goto __exit_fail;
	}

	/* log */
	g_conf.log_level = config_lookup_int (&cfg, "log");

	/* rtp_port_first/last */
	g_conf.rtp_port_first = config_lookup_int (&cfg, "rtp_port_first");
	g_conf.rtp_port_last = config_lookup_int (&cfg, "rtp_port_last");

	/* tos */
	g_conf.sip_tos = config_lookup_int (&cfg, "sip_tos");
	g_conf.rtp_tos = config_lookup_int (&cfg, "rtp_tos");

	/* SIP settings */
	g_conf.sip_set.all_set = 0;

	/* app.sip_registrar */
	str_elem = config_lookup_string (&cfg, "sip_registrar");
	if( !str_elem ){
		goto __exit_success;
	}
	strncpy (g_conf.sip_set.registrar, str_elem, REGISTRAR_LEN);

	/* app.sip_username */
	str_elem = config_lookup_string (&cfg, "sip_username");
	if( !str_elem ){
		goto __exit_success;
	}
	strncpy (g_conf.sip_set.user_name, str_elem, USER_NAME_LEN);

	/* app.sip_password */
	str_elem = config_lookup_string (&cfg, "sip_password");
	if( !str_elem ){
		goto __exit_success;
	}
	strncpy (g_conf.sip_set.user_pass, str_elem, USER_PASS_LEN);

	/* app.sip_uri */
	str_elem = config_lookup_string (&cfg, "sip_uri");
	if( !str_elem ){
		goto __exit_success;
	}
	strncpy (g_conf.sip_set.user_URI, str_elem, USER_URI_LEN);

	/* app.sip_chan */
	g_conf.sip_set.sip_chan = config_lookup_int (&cfg, "sip_chan");
	if((!ab->pchans[g_conf.sip_set.sip_chan]) ||
		(ab->pchans[g_conf.sip_set.sip_chan]->parent->type != ab_dev_type_FXS)){
		SU_DEBUG_2(("ATTENTION!! [%02d] is not FXS, as in %s.. "
				"EXIT\n", g_conf.sip_set.sip_chan, MAIN_CONF_NAME));
		goto __exit_fail;
	}

	g_conf.sip_set.all_set = 1;

__exit_success:
	config_destroy (&cfg);
	return 0;
__exit_fail:
	config_destroy (&cfg);
	return err;
}/*}}}*/

/**
 * Init`s fxo settings in main routine configuration \ref g_conf structure.
 *
 * \retval 0 success.
 * \retval -1 fail.
 */
static int
fxo_init( ab_t const * const ab )
{/*{{{*/
	struct config_t cfg;
	struct config_setting_t * set;
	struct config_setting_t * rec_set;
	enum pstn_type_e * curr_rec;
	char const * elem;
	int rec_num;
	int i;
	int err;

	config_init (&cfg);

	/* Load the file */
	if (!config_read_file (&cfg, FXO_CONF_NAME)){
		err = config_error_line (&cfg);
		SU_DEBUG_0(("%s(): Config file syntax error in line %d\n",
				__func__, err));
		goto __exit_fail;
	}

	/* Standart params for all chans already set
	 * when it memset(&g_conf,0) before this function call */

	/* Get values */
	set = config_lookup (&cfg, "fxo_prms" );
	if( !set ){
		/* We will use standart params for all channels */
		goto __exit_success;
	}

	rec_num = config_setting_length (set);

	if(rec_num > CHANS_MAX){
		SU_DEBUG_0(("%s(): Too many channels (%d) in config - max is %d\n",
				__func__, rec_num, CHANS_MAX));
		goto __exit_fail;
	}

	for(i=0; i<rec_num; i++){
		int abs_idx;
		rec_set = config_setting_get_elem (set, i);

		/* get chan id */
		elem = config_setting_get_string_elem (rec_set, 0);
		abs_idx = strtol(elem, NULL, 10);

		if((!ab->pchans[abs_idx]) ||
			(ab->pchans[abs_idx]->parent->type != ab_dev_type_FXO)){
			SU_DEBUG_2(("ATTENTION!! [%02d] is not FXO, as in %s.. "
					"continue\n", abs_idx, FXO_CONF_NAME));
		}

		curr_rec = &g_conf.fxo_PSTN_type[ abs_idx ];

		/* get fxo line type */
		elem = config_setting_get_string_elem (rec_set, 1);
		if       ( !strcmp(elem, CONF_FXO_PULSE)){
			*curr_rec = pstn_type_PULSE_ONLY;
		} else if( !strcmp(elem, CONF_FXO_TONE)){
			*curr_rec = pstn_type_TONE_AND_PULSE;
		}
	}
__exit_success:
	config_destroy (&cfg);
	return 0;
__exit_fail:
	config_destroy (&cfg);
	return -1;
}/*}}}*/

/**
 * Init`s route table in main routine configuration \ref g_conf structure.
 *
 * \retval 0 success.
 * \retval -1 fail.
 */
static int
routet_init( void )
{/*{{{*/
	struct config_t cfg;
	struct config_setting_t * set;
	struct config_setting_t * rec_set;
	struct rttb_record_s * curr_rec;
	char const * elem;
	int rec_num;
	char use_id_local_buf = 1;
	int id_len;
	int i;
	int err;

	config_init (&cfg);

	/* Load the file */
	if (!config_read_file (&cfg, ROUTET_CONF_NAME)){
		err = config_error_line (&cfg);
		SU_DEBUG_0(("%s(): Config file syntax error in line %d\n",
				__func__, err));
		goto __exit_fail;
	}

	/* Get values */
	set = config_lookup (&cfg, "route_table" );
	if( !set ){
		/* one router in the system */
		g_conf.self_ip = g_conf.lo_ip;
		goto __exit_success;
	}

	rec_num = config_setting_length (set);

	g_conf.route_table.records_num = rec_num;
	g_conf.route_table.records = malloc (
			rec_num * sizeof(*(g_conf.route_table.records)));
	if( !g_conf.route_table.records ){
		SU_DEBUG_0((LOG_FNC_A(LOG_NOMEM)));
		goto __exit_fail;
	}
	memset(g_conf.route_table.records, 0,
			rec_num * sizeof(*(g_conf.route_table.records)));

	rec_set = config_setting_get_elem (set, 0);
	elem = config_setting_get_string_elem (rec_set, 0);
	id_len = strlen(elem);

	g_conf.route_table.id_len = id_len;

	if (id_len + 1 > ROUTE_ID_LEN_DF){
		use_id_local_buf = 0;
	}

	for(i = 0; i < rec_num; i++){
		curr_rec = &g_conf.route_table.records[ i ];
		rec_set = config_setting_get_elem (set, i);

		/* get id */
		elem = config_setting_get_string_elem (rec_set, 0);

		if (use_id_local_buf){
			curr_rec->id = curr_rec->id_s;
		} else {
			curr_rec->id = malloc((id_len + 1)* sizeof(*(curr_rec->id)));
			if( !curr_rec->id ){
				SU_DEBUG_0 ((LOG_FNC_A(LOG_NOMEM)));
				goto __exit_fail;
			}
		}
		strcpy(curr_rec->id, elem);

		/* get value */
		elem = config_setting_get_string_elem (rec_set, 1);
		strcpy (curr_rec->value, elem);
	}

	err = self_values_init();
	if(err){
		goto __exit_fail;
	}

__exit_success:
	config_destroy (&cfg);
	return 0;
__exit_fail:
	if(g_conf.route_table.records) {
		if( !use_id_local_buf){
			for(i=0; i<rec_num; i++){
				curr_rec = &g_conf.route_table.records[ i ];
				if( curr_rec->id && curr_rec->id != curr_rec->id_s ){
					free (curr_rec->id);
					curr_rec->id = NULL;
				}
			}
		}
		free(g_conf.address_book.records);
		g_conf.address_book.records = NULL;
	}
	config_destroy (&cfg);
	return -1;
}/*}}}*/

/*
 * Init`s voice freq records in main routine configuration \ref g_conf structure.
 *
 * \param[in] ab ata-board hardware structure.
 *
 * \retval 0 success.
 * \retval -1 fail.
 */
static int
voicef_init( ab_t const * const ab )
{/*{{{*/
	/* ("chan_id", "pair_route_id", "pair_chan_id",
	 * 		"codec_name", "pkt_sz", payload_type, "bitpack") */
	struct config_t cfg;
	struct config_setting_t * set;
	struct config_setting_t * rec_set;
	struct voice_freq_s * curr_rec;
	char const * elem;
	int elem_len;
	int rec_num;
	int i;
	int err;

	config_init (&cfg);

	/* Load the file */
	if (!config_read_file (&cfg, VOICEF_CONF_NAME)){
		err = config_error_line (&cfg);
		SU_DEBUG_0(("%s(): Config file syntax error in line %d\n",
				__func__, err));
		goto __exit_fail;
	}

	set = config_lookup (&cfg, "voice_freq" );
	if( !set){
		/* no vf-channels */
		goto __exit_success;
	}

	rec_num = config_setting_length (set);

	if(rec_num > CHANS_MAX){
		SU_DEBUG_0(("%s(): Too many channels (%d) in config - max is %d\n",
				__func__, rec_num, CHANS_MAX));
		goto __exit_fail;
	}

	for(i=0; i<rec_num; i++){
		int chan_id;
		int pair_chan;
		int router_is_self;

		rec_set = config_setting_get_elem (set, i);

		/* get chan_id */
		elem = config_setting_get_string_elem (rec_set, 0);
		chan_id = strtol (elem, NULL, 10);

		if((!ab->pchans[chan_id]) ||
			(ab->pchans[chan_id]->parent->type != ab_dev_type_VF)){
			SU_DEBUG_2(("ATTENTION!! [%02d] is not VF, as in %s.. "
					"ignore config value\n",chan_id, VOICEF_CONF_NAME));
			continue;
		}

		curr_rec = &g_conf.voice_freq[ chan_id ];
		if( curr_rec->is_set){
			SU_DEBUG_2(("You shouldn`t set params for both pairs!\n"));
			continue;
		}
		curr_rec->is_set = 1;
		curr_rec->am_i_caller = 1;

		/* set chan_id to rec */
		snprintf(curr_rec->id, CHAN_ID_LEN, "%02d", chan_id);

		/* get pair_route_id */
		elem = config_setting_get_string_elem (rec_set, 1);
		elem_len = strlen(elem);
		if (elem_len+1 < ROUTE_ID_LEN_DF){
			curr_rec->pair_route = curr_rec->pair_route_s;
		} else {
			curr_rec->pair_route = malloc(
					(elem_len+1)*sizeof(*(curr_rec->pair_route)));
			if( !curr_rec->pair_route){
				SU_DEBUG_0((LOG_FNC_A(LOG_NOMEM)));
				goto __exit_fail;
			}
		}
		strcpy (curr_rec->pair_route, elem);

		router_is_self =
			(curr_rec->pair_route[0] == SELF_MARKER)
			||
			(g_conf.self_number &&
				!strcmp(curr_rec->pair_route, g_conf.self_number));
		if( router_is_self){
			if(curr_rec->pair_route &&
					curr_rec->pair_route != curr_rec->pair_route_s){
				free (curr_rec->pair_route);
			}
			curr_rec->pair_route = NULL;
		}

		/* get pair_chan_id */
		elem = config_setting_get_string_elem (rec_set, 2);
		pair_chan = strtol (elem, NULL, 10);

		/* set pair_chan to rec */
		snprintf(curr_rec->pair_chan, CHAN_ID_LEN, "%02d", pair_chan);

		/* get codec params */
		init_codec_el(rec_set, 3, &curr_rec->vf_codec);

		/* Create automatic mirror record if dest router is self */
		if(curr_rec->pair_route == NULL){
			if((!ab->pchans[pair_chan]) ||
				(ab->pchans[pair_chan]->parent->type != ab_dev_type_VF)){
				SU_DEBUG_2(("ATTENTION!! [%02d] is not VF, as in %s.. "
						"ignore config value\n",pair_chan, VOICEF_CONF_NAME));
				/* remove current channel VF-record
				 * because we can`t connect to not VF-channel */
				curr_rec->is_set = 0;
				continue;
			}
			struct voice_freq_s * mirr_rec = &g_conf.voice_freq[ pair_chan ];
			/* copy params */
			memcpy(&g_conf.voice_freq[ pair_chan ], &g_conf.voice_freq[ chan_id ],
					sizeof(g_conf.voice_freq[chan_id]));
			/* revert pair, self channels, vol and caller flag on mirror record */
			snprintf (mirr_rec->id, CHAN_ID_LEN, "%02d", pair_chan);
			snprintf (mirr_rec->pair_chan, CHAN_ID_LEN, "%02d", chan_id);

			/* set am_i_caller to the elder chan */
			if(strtol(mirr_rec->id,NULL,10) < chan_id){
				mirr_rec->am_i_caller = 0;
			} else {
				g_conf.voice_freq[ chan_id ].am_i_caller = 0;
			}
		}
	}

	/* read hw.conf to get channel type */
	/* vf_types:
	 * (
	 *	("chan_id", "wire_type", "normal/transit")
	 * );*/
	config_destroy (&cfg);
	config_init (&cfg);

	/* Load the file */
	if (!config_read_file (&cfg, VF_CONF_NAME)){
		err = config_error_line (&cfg);
		goto __exit_success;
	}

	set = config_lookup (&cfg, "vf_types" );
	if( !set){
		/* no vf-channels */
		goto __exit_success;
	}

	rec_num = config_setting_length (set);

	if(rec_num > CHANS_MAX){
		SU_DEBUG_1(("%s() Too many channels (%d) in config - max is %d\n",
				__func__, rec_num, CHANS_MAX));
		goto __exit_fail;
	}

	for(i=0; i<rec_num; i++){
		int chan_id;
		rec_set = config_setting_get_elem (set, i);

		/* get id */
		elem = config_setting_get_string_elem (rec_set, 0);
		chan_id = strtol (elem, NULL, 10);

		if((!ab->pchans[chan_id]) ||
			(ab->pchans[chan_id]->parent->type != ab_dev_type_VF)){
			SU_DEBUG_2(("ATTENTION!! [%02d] is not VF, as in %s.. "
					"ignore config value\n", chan_id, VF_CONF_NAME));
			continue;
		}

		curr_rec = &g_conf.voice_freq[ chan_id ];

		/* get wire_type */
		elem = config_setting_get_string_elem (rec_set, 1);
		if       (!strcmp(elem, CONF_VF_4_WIRED)){
			/* get normal/transit type */
			elem = config_setting_get_string_elem (rec_set, 2);
			if       (!strcmp(elem, CONF_VF_NORMAL)){
				ab->pchans[chan_id]->type_if_vf =
					curr_rec->type = vf_type_N4;
			} else if(!strcmp(elem, CONF_VF_TRANSIT)){
				ab->pchans[chan_id]->type_if_vf =
					curr_rec->type = vf_type_T4;
			}
		} else if(!strcmp(elem, CONF_VF_2_WIRED)){
			/* get normal/transit type */
			elem = config_setting_get_string_elem (rec_set, 2);
			if       (!strcmp(elem, CONF_VF_NORMAL)){
				ab->pchans[chan_id]->type_if_vf =
					curr_rec->type = vf_type_N2;
			} else if(!strcmp(elem, CONF_VF_TRANSIT)){
				ab->pchans[chan_id]->type_if_vf =
					curr_rec->type = vf_type_T2;
			}
		}
	}

__exit_success:
	config_destroy (&cfg);
	return 0;
__exit_fail:
	for(i=0; i<CHANS_MAX; i++){
		curr_rec = &g_conf.voice_freq[ i ];
		if( curr_rec->is_set && curr_rec->pair_route &&
				curr_rec->pair_route != curr_rec->pair_route_s ){
			free (curr_rec->pair_route);
			curr_rec->pair_route = NULL;
			curr_rec->is_set = 0;
		}
	}
	config_destroy (&cfg);
	return -1;
}/*}}}*/

/**
 * Init`s hot line records in main routine configuration \ref g_conf structure.
 *
 * \param[in] ab ata-board hardware structure.
 *
 * \retval 0 success.
 * \retval -1 fail.
 */
static int
hotline_init( ab_t const * const ab )
{/*{{{*/
	struct config_t cfg;
	struct config_setting_t * set;
	struct config_setting_t * rec_set;
	struct hot_line_s * curr_rec;
	char const * elem;
	int rec_num;
	int elem_len;
	int i;
	int err;

	config_init (&cfg);

	/* Load the file */
	if (!config_read_file (&cfg, HOTLINE_CONF_NAME)){
		err = config_error_line (&cfg);
		SU_DEBUG_0(("%s(): Config file syntax error in line %d\n",
				__func__, err));
		goto __exit_fail;
	}

	set = config_lookup (&cfg, "hot_line" );
	if( !set ){
		/* no hotline records */
		goto __exit_success;
	}

	rec_num = config_setting_length (set);

	if(rec_num > CHANS_MAX){
		SU_DEBUG_0(("%s(): Too many channels (%d) in config - max is %d\n",
				__func__, rec_num, CHANS_MAX));
		goto __exit_fail;
	}

	for(i=0; i<rec_num; i++){
		int chan_idx;
		rec_set = config_setting_get_elem (set, i);
		/* get id */
		elem = config_setting_get_string_elem (rec_set, 0);
		chan_idx = strtol(elem, NULL, 10);
		curr_rec = &g_conf.hot_line[ chan_idx ];

		if((!ab->pchans[chan_idx]) ||
			((ab->pchans[chan_idx]->parent->type != ab_dev_type_FXS) &&
			 (ab->pchans[chan_idx]->parent->type != ab_dev_type_FXO))
			){
			SU_DEBUG_2(("ATTENTION!! [%02d] is not FXS or FXO, as in %s.. "
					"ignore config value\n", chan_idx, HOTLINE_CONF_NAME));
			continue;
		}
		curr_rec->is_set = 1;

		/* get value */
		elem = config_setting_get_string_elem (rec_set, 1);
		elem_len = strlen(elem);
		if (elem_len+1 < VALUE_LEN_DF ){
			curr_rec->value = curr_rec->value_s;
		} else {
			curr_rec->value = malloc((elem_len+1)* sizeof(*(curr_rec->value)));
			if( !curr_rec->value ){
				SU_DEBUG_0((LOG_FNC_A(LOG_NOMEM)));
				goto __exit_fail;
			}
		}
		strcpy (curr_rec->value, elem);
	}

__exit_success:
	config_destroy (&cfg);
	return 0;
__exit_fail:
	for(i=0; i<CHANS_MAX; i++){
		curr_rec = &g_conf.hot_line[ i ];
		if( curr_rec->value && (curr_rec->value != curr_rec->value_s) ){
			free (curr_rec->value);
			curr_rec->value = NULL;
		}
	}
	config_destroy (&cfg);
	return -1;
}/*}}}*/

/**
 * Init`s address book records in main routine configuration
 * 		\ref g_conf structure.
 *
 * \retval 0 success.
 * \retval -1 fail.
 */
static int
addressb_init( void )
{/*{{{*/
	struct config_t cfg;
	struct config_setting_t * set;
	struct config_setting_t * rec_set;
	struct adbk_record_s * curr_rec;
	char const * elem;
	int elem_len;
	int rec_num;
	char use_id_local_buf = 1;
	int id_len;
	int i;
	int err;

	config_init (&cfg);

	/* Load the file */
	if (!config_read_file (&cfg, ADDRESSB_CONF_NAME)){
		err = config_error_line (&cfg);
		SU_DEBUG_0(("%s(): Config file syntax error in line %d\n",
				__func__, err));
		goto __exit_fail;
	}

	set = config_lookup (&cfg, "address_book" );
	if( !set ){
		g_conf.address_book.records_num = 0;
		goto __exit_success;
	}

	rec_num = config_setting_length (set);

	g_conf.address_book.records_num = rec_num;
	g_conf.address_book.records = malloc (rec_num *
			sizeof(*(g_conf.address_book.records)));
	if( !g_conf.address_book.records ){
		SU_DEBUG_0((LOG_FNC_A(LOG_NOMEM)));
		goto __exit_fail;
	}
	memset(g_conf.address_book.records, 0, rec_num *
			sizeof(*(g_conf.address_book.records)));

	rec_set = config_setting_get_elem (set, 0);
	elem = config_setting_get_string_elem (rec_set, 0);
	id_len = strlen(elem);

	g_conf.address_book.id_len = id_len;

	if (id_len + 1 > ADBK_ID_LEN_DF){
		use_id_local_buf = 0;
	}

	for(i = 0; i < rec_num; i++){
		curr_rec = &g_conf.address_book.records[ i ];
		rec_set = config_setting_get_elem (set, i);

		/* get id */
		elem = config_setting_get_string_elem (rec_set, 0);

		if (use_id_local_buf){
			curr_rec->id = curr_rec->id_s;
		} else {
			curr_rec->id = malloc( (id_len + 1) * sizeof(*(curr_rec->id)));
			if( !curr_rec->id ){
				SU_DEBUG_0((LOG_FNC_A(LOG_NOMEM)));
				goto __exit_fail;
			}
		}
		strcpy(curr_rec->id, elem);

		/* get value */
		elem = config_setting_get_string_elem (rec_set, 1);
		elem_len = strlen(elem);
		if (elem_len+1 < VALUE_LEN_DF ){
			curr_rec->value = curr_rec->value_s;
		} else {
			curr_rec->value = malloc((elem_len+1) * sizeof(*(curr_rec->value)));
			if( !curr_rec->value){
				SU_DEBUG_0((LOG_FNC_A(LOG_NOMEM)));
				goto __exit_fail;
			}
		}
		strcpy (curr_rec->value, elem);
	}

__exit_success:
	config_destroy (&cfg);
	return 0;
__exit_fail:
	if(g_conf.address_book.records) {
		for(i=0; i<rec_num; i++){
			curr_rec = &g_conf.address_book.records[ i ];
			if( curr_rec->id && curr_rec->id != curr_rec->id_s ){
				free (curr_rec->id);
				curr_rec->id = NULL;
			}
			if( curr_rec->value && curr_rec->value != curr_rec->value_s ){
				free (curr_rec->value);
				curr_rec->value = NULL;
			}
		}
		free(g_conf.address_book.records);
		g_conf.address_book.records = NULL;
	}
	config_destroy (&cfg);
	return -1;
}/*}}}*/

/**
 * Init`s codecs settings in main routine configuration \ref g_conf structure.
 *
 * \retval 0 success.
 * \retval -1 fail.
 *
 * \remark
 *	It must calls after main init becouse it can modify
 *	g_conf.sip_set.all_set setting.
 */
static int
codecs_init( void )
{/*{{{*/
	struct config_t cfg;
	struct config_setting_t * set = NULL;
	struct config_setting_t * rec_set = NULL;
	int i;
	int rec_num;
	int err;

	config_init (&cfg);

	/* Load the file */
	if (!config_read_file (&cfg, CODECS_CONF_NAME)){
		err = config_error_line (&cfg);
		SU_DEBUG_0(("%s(): Config file syntax error in line %d\n",
				__func__, err));
		goto __exit_fail;
	}

	/* set all to NONE */
	memset(g_conf.int_codecs, 0, sizeof(g_conf.int_codecs));
	memset(g_conf.sip_set.ext_codecs, 0, sizeof(g_conf.sip_set.ext_codecs));
	for (i=0; i<COD_MAS_SIZE; i++){
		g_conf.int_codecs[i].type = cod_type_NONE;
		g_conf.sip_set.ext_codecs[i].type = cod_type_NONE;
	}

	/* CODECS FOR INTERNAL USAGE */
	set = config_lookup (&cfg, "int_codecs" );
	if( !set ){
		SU_DEBUG_0(("No int_codecs entries in config file"));
		goto __exit_fail;
	}
	rec_num = config_setting_length (set);

	/* init one by one */
	for (i=0; i<rec_num; i++){
		rec_set = config_setting_get_elem (set, i);
		init_codec_el(rec_set, 0, &g_conf.int_codecs[i]);
	}

	/* CODECS FOR EXTERNAL USAGE */
	set = config_lookup (&cfg, "ext_codecs");
	if( !set){
		g_conf.sip_set.all_set = 0;
	} else {
		rec_num = config_setting_length (set);

		/* init one by one */
		for (i=0; i<rec_num; i++){
			rec_set = config_setting_get_elem (set, i);
			init_codec_el(rec_set, 0, &g_conf.sip_set.ext_codecs[i]);
		}
	}

	/* CODECS FOR FAX USAGE */
	/* set type and standart payload type values */
	g_conf.fax.codec_type = cod_type_ALAW;
	g_conf.fax.internal_pt = g_conf.fax.external_pt = ALAW_PT_DF;

	/* set internal fax payload type if it defined in config file */
	for (i=0; g_conf.int_codecs[i].type!=cod_type_NONE; i++){
		if(g_conf.int_codecs[i].type == g_conf.fax.codec_type){
			g_conf.fax.internal_pt = g_conf.int_codecs[i].user_payload;
			break;
		}
	}
	/* set external fax payload type if it defined in config file */
	if(g_conf.sip_set.all_set){
		for (i=0; g_conf.sip_set.ext_codecs[i].type!=cod_type_NONE; i++){
			if(g_conf.sip_set.ext_codecs[i].type == g_conf.fax.codec_type){
				g_conf.fax.external_pt =
						g_conf.sip_set.ext_codecs[i].user_payload;
				break;
			}
		}
	}

	config_destroy (&cfg);
	return 0;
__exit_fail:
	config_destroy (&cfg);
	return err;
}/*}}}*/

/**
 * Init`s AUDIO parameters in main routine configuration \ref g_conf structure.
 *
 * \retval 0 success.
 * \retval -1 fail.
 */
static int
audio_init( void )
{/*{{{*/
	struct config_t cfg;
	struct config_setting_t * set;
	struct config_setting_t * rec_set;
	struct rtp_session_prms_s * curr_rec;
	char const * elem;
	int rec_num;
	int i;
	int err;

	config_init (&cfg);

	/* Load the file */
	if (!config_read_file (&cfg, RTP_CONF_NAME)){
		err = config_error_line (&cfg);
		SU_DEBUG_0(("%s(): Config file syntax error in line %d\n",
				__func__, err));
		goto __exit_fail;
	}

	/* Standart params for all chans */
	curr_rec = &g_conf.audio_prms[0];
	curr_rec->enc_dB = 0;
	curr_rec->dec_dB = 0;
	curr_rec->VAD_cfg = vad_cfg_OFF;
	curr_rec->HPF_is_ON = 0;
	for (i=1; i<CHANS_MAX; i++){
		curr_rec = &g_conf.audio_prms[i];
		memcpy(curr_rec, &g_conf.audio_prms[0], sizeof(*curr_rec));
	}

	/* Get values */
	set = config_lookup (&cfg, "rtp_prms" );
	if( !set ){
		/* We will use standart params for all channels */
		goto __exit_success;
	}

	rec_num = config_setting_length (set);

	if(rec_num > CHANS_MAX){
		SU_DEBUG_0(("%s(): Too many channels (%d) in config - max is %d\n",
				__func__, rec_num, CHANS_MAX));
		goto __exit_fail;
	}

	for(i=0; i<rec_num; i++){
		int abs_idx;
		rec_set = config_setting_get_elem (set, i);

		/* get chan id */
		elem = config_setting_get_string_elem (rec_set, 0);
		abs_idx = strtol(elem, NULL, 10);

		curr_rec = &g_conf.audio_prms[ abs_idx ];

		curr_rec->enc_dB = config_setting_get_int_elem(rec_set, 1);
		curr_rec->dec_dB = config_setting_get_int_elem(rec_set, 2);
		elem = config_setting_get_string_elem (rec_set, 3);
		if( !strcmp(elem, CONF_VAD_NOVAD)){
			curr_rec->VAD_cfg = vad_cfg_OFF;
		} else if( !strcmp(elem, CONF_VAD_ON)){
			curr_rec->VAD_cfg = vad_cfg_ON;
		} else if( !strcmp(elem, CONF_VAD_G711)){
			curr_rec->VAD_cfg = vad_cfg_G711;
		} else if( !strcmp(elem, CONF_VAD_CNG_ONLY)){
			curr_rec->VAD_cfg = vad_cfg_CNG_only;
		} else if( !strcmp(elem, CONF_VAD_SC_ONLY)){
			curr_rec->VAD_cfg = vad_cfg_SC_only;
		}
		curr_rec->HPF_is_ON = config_setting_get_int_elem(rec_set, 4);
	}
__exit_success:
	config_destroy (&cfg);
	return 0;
__exit_fail:
	config_destroy (&cfg);
	return -1;
}/*}}}*/

/**
 * Init`s WLEC parameters in main routine configuration \ref g_conf structure.
 *
 * \param[in] ab ata-board hardware structure.
 *
 * \retval 0 success.
 * \retval -1 fail.
 */
static int
wlec_init( ab_t const * const ab )
{/*{{{*/
	struct config_t cfg;
	struct config_setting_t * set;
	struct config_setting_t * rec_set;
	struct wlec_s * curr_rec;
	ab_chan_t * cc; /* current channel */
	char const * elem;
	int rec_num;
	int i;
	int err;

	config_init (&cfg);

	/* Load the file */
	if (!config_read_file (&cfg, WLEC_CONF_NAME)){
		err = config_error_line (&cfg);
		SU_DEBUG_0(("%s(): Config file syntax error in line %d\n", __func__, err));
		goto __exit_fail;
	}

	/* Standart params for all chans */
	for (i=0; i<CHANS_MAX; i++){
		curr_rec = &g_conf.wlec_prms[i];
		cc = ab->pchans[i];
		if( !cc) {
			continue;
		}
		if       (cc->parent->type == ab_dev_type_FXO){
			curr_rec->mode = wlec_mode_NE;
			curr_rec->nlp = wlec_nlp_ON;
			curr_rec->ne_nb = 4;
			curr_rec->fe_nb = 4;
			curr_rec->ne_wb = 4;
		} else if(cc->parent->type == ab_dev_type_FXS){
			curr_rec->mode = wlec_mode_NE;
			curr_rec->nlp = wlec_nlp_OFF;
			curr_rec->ne_nb = 4;
			curr_rec->fe_nb = 4;
			curr_rec->ne_wb = 4;
		} else if(cc->parent->type == ab_dev_type_VF){
			curr_rec->mode = wlec_mode_OFF;
		}
	}

	/* Get values */
	set = config_lookup (&cfg, "wlec_prms" );
	if( !set ){
		/* We will use standart params for all channels */
		goto __exit_success;
	}

	rec_num = config_setting_length (set);

	if(rec_num > CHANS_MAX){
		SU_DEBUG_0(("%s(): Too many channels (%d) in config - max is %d\n",
				__func__, rec_num, CHANS_MAX));
		goto __exit_fail;
	}

	for(i=0; i<rec_num; i++){
		int abs_idx;
		rec_set = config_setting_get_elem (set, i);

		/* get chan id */
		elem = config_setting_get_string_elem (rec_set, 0);
		abs_idx = strtol(elem, NULL, 10);

		curr_rec = &g_conf.wlec_prms[ abs_idx ];

		/* get rtp params */
		elem = config_setting_get_string_elem (rec_set, 1);
		if       ( !strcmp(elem, CONF_WLEC_TYPE_OFF)){
			curr_rec->mode = wlec_mode_OFF;
		} else if( !strcmp(elem, CONF_WLEC_TYPE_NE)){
			curr_rec->mode = wlec_mode_NE;
		} else if( !strcmp(elem, CONF_WLEC_TYPE_NFE)){
			curr_rec->mode = wlec_mode_NFE;
		}

		elem = config_setting_get_string_elem (rec_set, 2);
		if( !strcmp(elem, CONF_WLEC_NLP_ON)){
			curr_rec->nlp = wlec_nlp_ON;
		} else if( !strcmp(elem, CONF_WLEC_NLP_OFF)){
			curr_rec->nlp = wlec_nlp_OFF;
		}

		curr_rec->ne_nb = config_setting_get_int_elem(rec_set, 3);
		curr_rec->fe_nb = config_setting_get_int_elem(rec_set, 4);
		/* tag__ to remove - because we not using wb */
		//curr_rec->ne_wb = config_setting_get_int_elem(rec_set, 5);

		if(curr_rec->mode == wlec_mode_NFE){
			if(curr_rec->ne_nb == 16){
				curr_rec->ne_nb = 8;
			}
			if(curr_rec->fe_nb == 16){
				curr_rec->fe_nb = 8;
			}
		}
	}
__exit_success:
	config_destroy (&cfg);
	return 0;
__exit_fail:
	config_destroy (&cfg);
	return -1;
}/*}}}*/

/**
 * Show error if something nasty happens.
 */
static void
error_message( void )
{/*{{{*/
	switch( g_err_no ){
		case ERR_SUCCESS :{
			return;
		}
		case ERR_MEMORY_FULL :{
			fprintf( stderr, "%s : not enough memory\n",
					PACKAGE_NAME );
			break;
		}
		case ERR_UNKNOWN_OPTION :{
			fprintf( stderr, "%s : invalid option\n",
					PACKAGE_NAME );
			break;
		}
	}
	fprintf( stderr,"Try '%s --help' for more information.\n",
			PACKAGE_NAME );
}/*}}}*/

/**
 * Show help message.
 */
static void
show_help( void )
{/*{{{*/
	fprintf( stdout,
"\
Usage: %s [OPTION]\n\
SIP VoIP User agent Daemon.\n\
\n\
Mandatory arguments to long options are mandatory for short options too.\n\
  -h, --help         display this help and exit\n\
  -V, --version      displey current version and license info\n\
  -d, --debug        set the debug level (form 0 to 9)\n\
\n\
	Execution example :\n\
	%s -d9\n\
	Means, that you wont start daemon in debug mode with \n\
			maximum debug output.\n\
\n\
Report bugs to <%s>.\n\
"
		, PACKAGE_NAME, PACKAGE_NAME, PACKAGE_BUGREPORT );
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
		, PACKAGE_NAME, PACKAGE_VERSION,
		__DATE__, __TIME__, PACKAGE_BUGREPORT);
}/*}}}*/

