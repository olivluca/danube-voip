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

#include <uci.h>
#include <ucimap.h>
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

/**
 * Converts a string representation of a codec name to a codec type.
 *
 * \param[in] name name of the codec.
 * \retval  codec type (cod_type_NONE if name doesn't match any codec)
 */
static int get_codec_type(const char *name)
{
	int i;
	
	for (i=0; g_conf.cp[i].type != cod_type_NONE; i++)
		if (!strcasecmp(name, g_conf.cp[i].sdp_name))
			return (g_conf.cp[i].type);
	return(cod_type_NONE);
 }

struct uci_account {
	struct ucimap_section_data map;
	char *name;
	char *registrar;
	char *user_name;
	char *user_pass;
	char *user_URI;
	char *sip_domain;
	struct ucimap_list *outgoing_priority;
	struct ucimap_list *ring_incoming;
	struct ucimap_list *codecs;
	char *dtmf;
};

static int
account_init(struct uci_map *map, void *section, struct uci_section *s)
{
	struct uci_account *a = section;
	a->name = strdup(s->e.name);
	return 0;
}

static int
account_add(struct uci_map *map, void *section)
{
	struct uci_account *a = section;
	struct sip_account_s *s;
	int i;
	int k;
	int codec_type;

	s = malloc(sizeof(*s));
	if( !s ){
		SU_DEBUG_0((LOG_FNC_A(LOG_NOMEM)));
		goto __exit_fail;
	}
	memset(s, 0, sizeof(*s));
	su_vector_append(g_conf.sip_account, s);
	
	if (!a->registrar || !a->user_name || !a->user_pass ||
	    !a->user_URI || !a->sip_domain || !a->codecs || !a->outgoing_priority ||
	    !a->ring_incoming || !a->dtmf) {
		SU_DEBUG_0(("settings for account %s incomplete\n",a->name));
		goto __exit_fail;
	}
	 
	s->name = strdup(a->name); 
	s->registrar = strdup(a->registrar);
	s->user_name = strdup(a->user_name);
	s->user_pass = strdup(a->user_pass);
	s->user_URI = strdup(a->user_URI);
	s->sip_domain = strdup(a->sip_domain);
	
	k=0;
	for (i=0; i<a->codecs->n_items; i++){
	        codec_type = get_codec_type(a->codecs->item[i].s);
		if (codec_type != cod_type_NONE) 
			    s->codecs[k++]=codec_type;
	}
	for (i=0; i<a->outgoing_priority->n_items && i<g_conf.channels; i++)
		s->outgoing_priority[i] = a->outgoing_priority->item[i].i;
	for (i=0; i<a->ring_incoming->n_items && i<g_conf.channels; i++)
		s->ring_incoming[i] = a->ring_incoming->item[i].b;
	s->all_set = 1;	
__exit_fail:	
	free(section);
	return 0;
}

static struct uci_optmap account_uci_map[] =
{
	{
		UCIMAP_OPTION(struct uci_account, registrar),
		.type = UCIMAP_STRING,
		.name = "registrar",
	},{
		UCIMAP_OPTION(struct uci_account, user_name),
		.type = UCIMAP_STRING,
		.name = "user_name",
	},{
		UCIMAP_OPTION(struct uci_account, user_pass),
		.type = UCIMAP_STRING,
		.name = "user_pass",
	},{
		UCIMAP_OPTION(struct uci_account, user_URI),
		.type = UCIMAP_STRING,
		.name = "uri",
	},{
		UCIMAP_OPTION(struct uci_account, sip_domain),
		.type = UCIMAP_STRING,
		.name = "domain",
	},{
		UCIMAP_OPTION(struct uci_account, codecs),
		.type = UCIMAP_STRING | UCIMAP_LIST | UCIMAP_LIST_AUTO,
		.name = "codecs",
	},{
		UCIMAP_OPTION(struct uci_account, ring_incoming),
		.type = UCIMAP_BOOL | UCIMAP_LIST | UCIMAP_LIST_AUTO,
		.name = "ring",
	},{
		UCIMAP_OPTION(struct uci_account, outgoing_priority),
		.type = UCIMAP_INT | UCIMAP_LIST | UCIMAP_LIST_AUTO,
		.name = "priority",
	},{
		UCIMAP_OPTION(struct uci_account, dtmf),
		.type = UCIMAP_STRING,
		.name = "dtmf",
	},
};

static struct uci_sectionmap account_sectionmap = {
	UCIMAP_SECTION(struct uci_account, map),
	.type = "account",
	.init = account_init,
	.add = account_add,
	.options = account_uci_map,
	.n_options = ARRAY_SIZE(account_uci_map),
	.options_size = sizeof(struct uci_optmap),
};


static struct uci_sectionmap *svd_smap[] = {
	&account_sectionmap,
};

static struct uci_map svd_map = {
	.sections = svd_smap,
	.n_sections = ARRAY_SIZE(svd_smap),
};

int
config_load(void)
{
	int ret;
	struct uci_context *ctx = uci_alloc_context();
	struct uci_package *pkg;

	ucimap_init(&svd_map);
	ret = uci_load(ctx, "svd", &pkg);
	if (ret)
		return -1;
	ucimap_parse(&svd_map, pkg);
	return 0;
}

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
//#define CONF_JB_LOC_SI "SI"
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
/*First "real" codec (index 0 in various arrays)*/
#define CODEC_BASE cod_type_ALAW

/** @}*/

/** @defgroup CFG_IF Config internal functions.
 *  @ingroup CFG_M
 *  This functions using while reading config file.
 *  @{*/
/** Init self router ip and number.*/
static int self_values_init (void);
/** Init codec_t from rec_set. */
static void init_codec_el(struct config_setting_t const * const rec_set,
		int const line);
/** Init main configuration.*/
static int main_init (ab_t const * const ab);
/** Read sip accounts.*/
static int sip_init (ab_t const * const ab, su_home_t * home);
/** Init AUDIO parameters configuration.*/
static int audio_init (void);
/** Init WLEC parameters configuration.*/
static int wlec_init (ab_t const * const ab);
/** Init dialplan configuration.*/
static int dialplan_init (su_home_t * home);
/** Initilize codecs parameters structure */
static int svd_init_cod_params( cod_prms_t * const cp );
/** Init codecs definitions.*/
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
svd_conf_init( ab_t const * const ab, su_home_t * home )
{/*{{{*/
	/* default presets */
	memset (&g_conf, 0, sizeof(g_conf));
	if(		main_init (ab) 	||
			codecs_init()	||
			sip_init (ab, home)||
			audio_init()	||
			wlec_init (ab)	||
			dialplan_init(home)
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
	char * dtmf_name[] = {"off", "rfc2883", "info"};
	struct dplan_record_s * curr_dp_rec;
	struct sip_account_s  * curr_sip_rec;
	int i;
	int j;

	SU_DEBUG_3(("=========================\n"));
	SU_DEBUG_3(("channels %d : ", g_conf.channels));
	SU_DEBUG_3(("log["));

	if( g_conf.log_level == -1 ){
		SU_DEBUG_3(("no] : "));
	} else {
		SU_DEBUG_3(("%d] : ", g_conf.log_level));
	}
	SU_DEBUG_3(("ports[%d:%d]\n",
			g_conf.rtp_port_first,
			g_conf.rtp_port_last));

	for (i=i; i<COD_MAS_SIZE; i++) if (g_conf.codecs[i].type!=cod_type_NONE) {
		SU_DEBUG_3(("t:%s/sz%d/pt:0x%X__[%d:%d]::[%d:%d:%d:%d]\n",
				g_conf.cp[g_conf.codecs[i].type-CODEC_BASE].sdp_name,
				g_conf.codecs[i].pkt_size,
				g_conf.codecs[i].user_payload,
				g_conf.codecs[i].jb.jb_type,
				g_conf.codecs[i].jb.jb_loc_adpt,
				g_conf.codecs[i].jb.jb_scaling,
				g_conf.codecs[i].jb.jb_init_sz,
				g_conf.codecs[i].jb.jb_min_sz,
				g_conf.codecs[i].jb.jb_max_sz
				));
	}

	if (g_conf.sip_account)
	for (i=0; i<su_vector_len(g_conf.sip_account); i++) {
		curr_sip_rec = su_vector_item(g_conf.sip_account, i);  
		SU_DEBUG_3(("SIP net %d : %d\n", i, curr_sip_rec->all_set));
		if(curr_sip_rec->all_set){
			SU_DEBUG_3((	"\tCodecs:\t"));
			for (j=0; curr_sip_rec->codecs[j] != cod_type_NONE; j++){
			      SU_DEBUG_3(("%s ",
					g_conf.cp[curr_sip_rec->codecs[j]-CODEC_BASE].sdp_name
					));
			}
			SU_DEBUG_3(("\n"));

			SU_DEBUG_3((	"\tRegistrar   : '%s'\n"
					"\tUser/Pass   : '%s/%s'\n"
					"\tUser_URI    : '%s'\n",
					curr_sip_rec->registrar,
					curr_sip_rec->user_name,
					curr_sip_rec->user_pass,
					curr_sip_rec->user_URI));
					
			SU_DEBUG_3((	"\tRing incoming:\n"));
			for (j=0; j<g_conf.channels; j++){
			      SU_DEBUG_3(("\t\tchannel %d:%d\n",
					j,  
					curr_sip_rec->ring_incoming[j]
					));
			}
			SU_DEBUG_3((	"\tOutgoing priority:\n"));
			for (j=0; j<g_conf.channels; j++){
			      SU_DEBUG_3(("\t\tchannel %d:%d\n",
					j,  
					curr_sip_rec->outgoing_priority[j]
					));
			}
			SU_DEBUG_3((	"\tDtmf mode: %s\n", dtmf_name[curr_sip_rec->dtmf]));
		}
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

	if(g_conf.dial_plan){
		SU_DEBUG_3(("Dial plan :\n"));
	
		j = su_vector_len(g_conf.dial_plan);
		for(i = 0; i < j; i++){
			curr_dp_rec = su_vector_item(g_conf.dial_plan, i);
			SU_DEBUG_3(("\tPrefix \"%s\", Replace \"%s\", Account %d\n",
					curr_dp_rec->prefix, curr_dp_rec->replace, curr_dp_rec->account));
		}
	}

	SU_DEBUG_3(("=========================\n"));
}/*}}}*/

/**
 * Free allocated memory for main configuration structure.
 */
void
svd_conf_destroy (void)
{/*{{{*/
	if (g_conf.dial_plan)
	  su_vector_destroy(g_conf.dial_plan);
	
	if (g_conf.sip_account)
	  su_vector_destroy(g_conf.sip_account);
	
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
static int
svd_init_cod_params( cod_prms_t * const cp )
{/*{{{*/
	static char * empty = "";
	int i;
	memset(cp, 0, sizeof(*cp)*COD_MAS_SIZE);

	for (i=0; i<COD_MAS_SIZE; i++){
		cp[i].type = cod_type_NONE;
	}


	/* G711 ALAW parameters. */
	i=cod_type_ALAW-CODEC_BASE;
	cp[i].type = cod_type_ALAW;
	cp[i].sdp_name=strdup("PCMA");
	cp[i].fmtp_str=empty;
	cp[i].rate = 8000;

	/* G729 parameters. */
	i=cod_type_G729-CODEC_BASE;
	cp[i].type = cod_type_G729;
	cp[i].sdp_name=strdup("G729");
	cp[i].fmtp_str=empty;
	cp[i].rate = 8000;

	/* G729E parameters. */
	i=cod_type_G729E-CODEC_BASE;
	cp[i].type = cod_type_G729E;
	cp[i].sdp_name=strdup("G729E");
	cp[i].fmtp_str=empty;
	cp[i].rate = 8000;

	/* G723 parameters. */
	i=cod_type_G723-CODEC_BASE;
	cp[i].type = cod_type_G723;
	cp[i].sdp_name=strdup("G723");
	cp[i].fmtp_str=empty;
	cp[i].rate = 8000;

	/* iLBC_133 parameters. */
	i=cod_type_ILBC_133-CODEC_BASE;
	cp[i].type = cod_type_ILBC_133;
	cp[i].sdp_name=strdup("iLBC");
	cp[i].fmtp_str=strdup("mode=30");
	cp[i].rate = 8000;

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
	i=cod_type_G726_16-CODEC_BASE;
	cp[i].type = cod_type_G726_16;
	cp[i].sdp_name=strdup("G726-16");
	cp[i].fmtp_str=empty;
	cp[i].rate = 8000;

	/* G726_ parameters. */
	i=cod_type_G726_24-CODEC_BASE;
	cp[i].type = cod_type_G726_24;
	cp[i].sdp_name=strdup("G726-24");
	cp[i].fmtp_str=empty;
	cp[i].rate = 8000;

	/* G726_ parameters. */
	i=cod_type_G726_32-CODEC_BASE;
	cp[i].type = cod_type_G726_32;
	cp[i].sdp_name=strdup("G726-32");
	cp[i].fmtp_str=empty;
	cp[i].rate = 8000;

	/* G726_ parameters. */
	i=cod_type_G726_40-CODEC_BASE;
	cp[i].type = cod_type_G726_40;
	cp[i].sdp_name=strdup("G726-40");
	cp[i].fmtp_str=empty;
	cp[i].rate = 8000;
	
	/* telephone event parameters. */
	i=TELEPHONE_EVENT_CODEC-CODEC_BASE;
	cp[i].type = TELEPHONE_EVENT_CODEC;
	cp[i].sdp_name=strdup("telephone-event");
	cp[i].fmtp_str=strdup("0-16");
	cp[i].rate = 8000;

	return 0;
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
#warning FIX self_values_init
#if 0
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
#endif
	return 0;
__exit_fail:
	return -1;
	
}/*}}}*/

/**
 * Overrides one codec element from appropriate config setting.
 *
 * \param[in] rec_set config setting.
 * \param[in] prms_offset from there starts the codec params in offset.
 * \param[out] cod codec_t element to initilize.
 */
static void
init_codec_el(struct config_setting_t const *const rec_set,
		int const line)
{/*{{{*/
	char const * codel = NULL;
	float scal;
	int codec_type;
	codec_t * cod;

	/* codec type */
	codel = config_setting_get_string_elem (rec_set, 0);
	codec_type = get_codec_type(codel);
	if (codec_type == cod_type_NONE) {
		SU_DEBUG_0(("Wrong codec name \"%s\" in definition %d\n",
				codel, line-1));
		return;		
	}  
	
	cod=&g_conf.codecs[codec_type];

	/* codec packet size */
	codel = config_setting_get_string_elem (rec_set, 1);
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
	cod->user_payload = config_setting_get_int_elem (rec_set, 2);

	/* codec bitpack */
	codel = config_setting_get_string_elem (rec_set, 3);
	if       ( !strcmp(codel, CONF_CODEC_BITPACK_RTP)){
		cod->bpack = bitpack_RTP;
	} else if( !strcmp(codel, CONF_CODEC_BITPACK_AAL2)){
		cod->bpack = bitpack_AAL2;
	}

	/* jb type */
	codel = config_setting_get_string_elem (rec_set, 4);
	if       ( !strcmp(codel, CONF_JB_TYPE_FIXED)){
		cod->jb.jb_type = jb_type_FIXED;
	} else if( !strcmp(codel, CONF_JB_TYPE_ADAPTIVE)){
		cod->jb.jb_type = jb_type_ADAPTIVE;
	}

	/* local adaptation type */
	codel = config_setting_get_string_elem (rec_set, 5);
	if       ( !strcmp(codel, CONF_JB_LOC_OFF)){
		cod->jb.jb_loc_adpt = jb_loc_adpt_OFF;
	} else if( !strcmp(codel, CONF_JB_LOC_ON)){
		cod->jb.jb_loc_adpt = jb_loc_adpt_ON;
//	} else if( !strcmp(codel, CONF_JB_LOC_SI)){
//		cod->jb.jb_loc_adpt = jb_loc_adpt_SI;
	}

	/* scaling factor */
	scal = config_setting_get_float_elem(rec_set, 6);
	if((int)scal == 0){
		cod->jb.jb_scaling = config_setting_get_int_elem(rec_set,6)*16;
	} else {
		cod->jb.jb_scaling = scal * 16;
	}
	if(cod->jb.jb_scaling == 0){
		cod->jb.jb_scaling = 255;
	}

	/* buffer limitations */
	cod->jb.jb_init_sz= config_setting_get_int_elem(rec_set, 7)*8;
	cod->jb.jb_min_sz = config_setting_get_int_elem(rec_set, 8)*8;
	cod->jb.jb_max_sz = config_setting_get_int_elem(rec_set, 9)*8;
}/*}}}*/

/**
 * Init`s main settings in main routine configuration \ref g_conf structure.
 *
 * \param[in] ab ata-board hardware structure.
 *
 * \retval 0 success.
 * \retval -1 fail.
 */
static int my_config_lookup_int(const config_t *config, const char *path)
{

  int r;
  if (config_lookup_int(config, path, &r)==CONFIG_TRUE) return r;
  else return 0;
}
 
static int
main_init( ab_t const * const ab )
{/*{{{*/
	struct config_t cfg;
	int err;

	g_conf.channels = ab->chans_per_dev;
	config_init (&cfg);

	/* Load the file */
	if (!config_read_file (&cfg, MAIN_CONF_NAME)){
		err = config_error_line (&cfg);
		SU_DEBUG_0(("%s(): Config file syntax error in line %d\n",
				__func__, err));
		goto __exit_fail;
	}

	/* log */
	g_conf.log_level = my_config_lookup_int (&cfg, "log");

	/* rtp_port_first/last */
	g_conf.rtp_port_first = my_config_lookup_int (&cfg, "rtp_port_first");
	g_conf.rtp_port_last = my_config_lookup_int (&cfg, "rtp_port_last");

	/* tos */
	g_conf.sip_tos = my_config_lookup_int (&cfg, "sip_tos");
	g_conf.rtp_tos = my_config_lookup_int (&cfg, "rtp_tos");

	config_destroy (&cfg);
	return 0;
__exit_fail:
	config_destroy (&cfg);
	return err;
}/*}}}*/

/**
 * Frees one sip account.
 *
 * \param[in] elem account to free.
 */
static void
sip_free(void * elem)
{/*{{{*/
	struct sip_account_s * account = elem;
	if (account->registrar)
		free (account->registrar);
	if (account->user_name)
		free (account->user_name);
	if (account->user_pass)
		free (account->user_pass);
	if (account->user_URI)
		free (account->user_URI);
	if (account->sip_domain)
		free (account->sip_domain);
#ifndef DONT_BIND_TO_DEVICE
	if (account->rtp_interface)
		free (account->rtp_interface);
#endif	
	free (account);
}/*}}}*/

/**
 * Read sip accounts.
 *
 * \param[in] ab ata-board hardware structure.
 * \param[in] home su_home for memory allocation
 *
 * \retval 0 success.
 * \retval -1 fail.
 */
static int
sip_init( ab_t const * const ab, su_home_t * home )
{/*{{{*/
	struct config_t cfg;
	struct config_setting_t * set;
	struct config_setting_t * rec_set;
	struct config_setting_t * rec_subset;
	int codec_type;
	struct sip_account_s * curr_rec;
	char const * str_elem = NULL;
	int err;
	int rec_num, codecs_num;
	int i,j,k;

	config_init (&cfg);

	/* Load the file */
	if (!config_read_file (&cfg, SIP_CONF_NAME)){
		err = config_error_line (&cfg);
		SU_DEBUG_0(("%s(): Config file syntax error in line %d\n",
				__func__, err));
		goto __exit_fail;
	}
	
	set = config_lookup (&cfg, "accounts" );
	if( !set ){
		goto __exit_fail;
	}
	g_conf.sip_account = su_vector_create(home,sip_free);
	if( !g_conf.sip_account ){
		SU_DEBUG_0((LOG_FNC_A(LOG_NOMEM)));
		goto __exit_fail;
	}

	rec_num = config_setting_length (set);
	for(i = 0; i < rec_num; i++){
		curr_rec = malloc(sizeof(*curr_rec));
		if( !curr_rec ){
			SU_DEBUG_0((LOG_FNC_A(LOG_NOMEM)));
			goto __exit_fail;
		}
		memset(curr_rec, 0, sizeof(*curr_rec));
		su_vector_append(g_conf.sip_account, curr_rec);
		rec_set = config_setting_get_elem (set, i);
                
		curr_rec->all_set = 0;
		if (config_setting_lookup_string(rec_set, "sip_registrar", &str_elem) == CONFIG_FALSE)
		  continue;
		curr_rec->registrar = strdup(str_elem);
		
		if (config_setting_lookup_string(rec_set, "sip_username", &str_elem) == CONFIG_FALSE)
		  continue;
		curr_rec->user_name = strdup(str_elem);

		if (config_setting_lookup_string(rec_set, "sip_password", &str_elem) == CONFIG_FALSE)
		  continue;
		curr_rec->user_pass = strdup(str_elem);

		if (config_setting_lookup_string(rec_set, "sip_uri", &str_elem) == CONFIG_FALSE)
		  continue;
		curr_rec->user_URI = strdup(str_elem);
		
		if (config_setting_lookup_string(rec_set, "sip_domain", &str_elem) == CONFIG_FALSE)
		  continue;
		curr_rec->sip_domain = strdup(str_elem);

#ifndef DONT_BIND_TO_DEVICE
		if (config_setting_lookup_string(rec_set, "rtp_interface", &str_elem) == CONFIG_FALSE)
		  continue;
		curr_rec->rtp_interface = strdup(str_elem);
#endif
		
		/* CODECS */
		for (j=0; j<COD_MAS_SIZE; j++)
		      curr_rec->codecs[j]=cod_type_NONE;
		rec_subset = config_lookup_from(rec_set, "codecs");
		if( !rec_subset ){
			SU_DEBUG_0(("No codecs entries in config file %s for account %d\n", SIP_CONF_NAME, i ));
			continue;
		}
		codecs_num = config_setting_length (rec_subset);
		/* set one by one */
		k=0;
		for (j=0; j<codecs_num; j++){
		        codec_type = get_codec_type(config_setting_get_string_elem(rec_subset, j));
			if (codec_type != cod_type_NONE) 
			    curr_rec->codecs[k++]=codec_type;
		}
		
		curr_rec->dtmf=dtmf_off;
		if (config_setting_lookup_string(rec_set, "dtmf", &str_elem) ==CONFIG_TRUE) {
			if (!strcasecmp(str_elem,"rfc2883")) {
				curr_rec->dtmf=dtmf_2883;
				curr_rec->codecs[k++] = TELEPHONE_EVENT_CODEC;
			} else if (!strcasecmp(str_elem,"info")) {
				curr_rec->dtmf=dtmf_info;
			}
		}
		
		
		/* Ring incoming */
		rec_subset = config_lookup_from(rec_set, "ring_incoming");
		if( !rec_subset ){
			SU_DEBUG_0(("No ring_incoming entries in config file\n"));
			continue;
		}
		k = config_setting_length (rec_subset);
		for (j=0; j<k && j<CHANS_MAX; j++){
		        curr_rec->ring_incoming[j]=config_setting_get_bool_elem(rec_subset, j);
		}
		
		/* Outgoing priority */
		rec_subset = config_lookup_from(rec_set, "outgoing_priority");
		if( !rec_subset ){
			SU_DEBUG_0(("No outgoing_priority entries in config file\n"));
			continue;
		}
		k = config_setting_length (rec_subset);
		for (j=0; j<k && j<CHANS_MAX; j++){
		        curr_rec->outgoing_priority[j]=config_setting_get_int_elem(rec_subset, j);
		}
		
		curr_rec->all_set = 1;
	}	

	config_destroy (&cfg);
	return 0;
__exit_fail:
	config_destroy (&cfg);
	if (g_conf.sip_account) {
	  su_vector_destroy(g_conf.sip_account);
	  g_conf.sip_account = NULL;
	}
	return err;
}/*}}}*/

/**
 * Frees one dialplan entry.
 *
 * \param[in] elem account to free.
 */
static void
dialplan_free(void * elem)
{/*{{{*/
	struct dplan_record_s * record = elem;
	if (record->prefix)
		free (record->prefix);
	if (record->replace)
		free (record->replace);
	free (record);
}/*}}}*/
/**
 * Inits dial plan records in main routine configuration
 * 		\ref g_conf structure.
 *
 * \param[in] home su_home for memory allocation
 *
 * \retval 0 success.
 * \retval -1 fail.
 */
static int
dialplan_init( su_home_t * home )
{/*{{{*/
	struct config_t cfg;
	struct config_setting_t * set;
	struct config_setting_t * rec_set;
	struct dplan_record_s * dplan_rec;
	char const * elem;
	int rec_num;
	int i;
	int err;

	config_init (&cfg);

	/* Load the file */
	if (!config_read_file (&cfg, DIALPLAN_CONF_NAME)){
		err = config_error_line (&cfg);
		SU_DEBUG_0(("%s(): Config file syntax error in line %d\n",
				__func__, err));
		goto __exit_fail;
	}
	
	g_conf.dial_plan = su_vector_create(home, dialplan_free);
	if( !g_conf.dial_plan ){
		SU_DEBUG_0((LOG_FNC_A(LOG_NOMEM)));
		goto __exit_fail;
	}

	set = config_lookup (&cfg, "dial_plan" );
	if( !set ){
		goto __exit_success;
	}

	rec_num = config_setting_length (set);
	int max_account = su_vector_len(g_conf.sip_account);
	for(i = 0; i < rec_num; i++){
		dplan_rec = malloc(sizeof(*dplan_rec));
		if( !dplan_rec ){
			SU_DEBUG_0((LOG_FNC_A(LOG_NOMEM)));
			goto __exit_fail;
		}
		memset(dplan_rec, 0, sizeof(*dplan_rec));
		
		rec_set = config_setting_get_elem (set, i);

		/* get prefix */
		elem = config_setting_get_string_elem (rec_set, 0);
		if (elem) {
			dplan_rec->prefix = strdup(elem);
			dplan_rec->prefixlen = strlen(elem);
		} else
			dplan_rec->prefixlen = 0;

		/* get replace */
		elem = config_setting_get_string_elem (rec_set, 1);
		if (elem) {
			dplan_rec->replace = strdup(elem);
		} else {
			dplan_rec->replace = strdup("");
		}
		
		/* get account */
		dplan_rec->account = config_setting_get_int_elem(rec_set, 2)-1;
		
		/* bogus dial plan entry, ignore it */
		if (dplan_rec->account>=max_account || dplan_rec->account <0 || dplan_rec->prefixlen == 0) {
			free(dplan_rec);
			continue;
		}
		/* add the record to the dial plan */
		su_vector_append(g_conf.dial_plan, dplan_rec);
		
	}

__exit_success:
	config_destroy (&cfg);
	return 0;
__exit_fail:
	if(g_conf.dial_plan) {
		su_vector_destroy(g_conf.dial_plan);
		g_conf.dial_plan=NULL;
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
 *	g_conf.sip_account.all_set setting.
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
	
	/* Init codecs names */
	if (svd_init_cod_params(g_conf.cp)) {
		SU_DEBUG_0(("Error in svd_init_cod_params\n"));
		goto __exit_fail;
	}

	/* Load the file */
	if (!config_read_file (&cfg, CODECS_CONF_NAME)){
		err = config_error_line (&cfg);
		SU_DEBUG_0(("%s(): Config file syntax error in line %d\n",
				__func__, err));
		goto __exit_fail;
	}

	/* set all to NONE */
	memset(g_conf.codecs, 0, sizeof(g_conf.codecs));
	for (i=0; i<COD_MAS_SIZE; i++){
		g_conf.codecs[i].type = cod_type_NONE; /* FIXME*/
	}

	/* Default values for the codecs */
	g_conf.codecs[cod_type_ALAW].type=cod_type_ALAW;
	g_conf.codecs[cod_type_ALAW].pkt_size=cod_pkt_size_60;
	g_conf.codecs[cod_type_ALAW].bpack=bitpack_RTP;
	g_conf.codecs[cod_type_ALAW].user_payload=8;
	g_conf.codecs[cod_type_ALAW].jb.jb_type=jb_type_FIXED;
	g_conf.codecs[cod_type_ALAW].jb.jb_loc_adpt=jb_loc_adpt_OFF;
	g_conf.codecs[cod_type_ALAW].jb.jb_scaling=1.4*16;
	g_conf.codecs[cod_type_ALAW].jb.jb_init_sz=120*8;
	g_conf.codecs[cod_type_ALAW].jb.jb_min_sz=10*8;
	g_conf.codecs[cod_type_ALAW].jb.jb_max_sz=200*8;
	
	g_conf.codecs[cod_type_G729].type=cod_type_G729;
	g_conf.codecs[cod_type_G729].pkt_size=cod_pkt_size_60;
	g_conf.codecs[cod_type_G729].bpack=bitpack_RTP;
	g_conf.codecs[cod_type_G729].user_payload=18;
	g_conf.codecs[cod_type_G729].jb.jb_type=jb_type_FIXED;
	g_conf.codecs[cod_type_G729].jb.jb_loc_adpt=jb_loc_adpt_OFF;
	g_conf.codecs[cod_type_G729].jb.jb_scaling=1.4*16;
	g_conf.codecs[cod_type_G729].jb.jb_init_sz=120*8;
	g_conf.codecs[cod_type_G729].jb.jb_min_sz=10*8;
	g_conf.codecs[cod_type_G729].jb.jb_max_sz=200*8;
	
	g_conf.codecs[cod_type_G729E].type=cod_type_G729E;
	g_conf.codecs[cod_type_G729E].pkt_size=cod_pkt_size_60;
	g_conf.codecs[cod_type_G729E].bpack=bitpack_RTP;
	g_conf.codecs[cod_type_G729E].user_payload=101;
	g_conf.codecs[cod_type_G729E].jb.jb_type=jb_type_FIXED;
	g_conf.codecs[cod_type_G729E].jb.jb_loc_adpt=jb_loc_adpt_OFF;
	g_conf.codecs[cod_type_G729E].jb.jb_scaling=1.4*16;
	g_conf.codecs[cod_type_G729E].jb.jb_init_sz=120*8;
	g_conf.codecs[cod_type_G729E].jb.jb_min_sz=10*8;
	g_conf.codecs[cod_type_G729E].jb.jb_max_sz=200*8;
	
	g_conf.codecs[cod_type_ILBC_133].type=cod_type_ILBC_133;
	g_conf.codecs[cod_type_ILBC_133].pkt_size=cod_pkt_size_30;
	g_conf.codecs[cod_type_ILBC_133].bpack=bitpack_RTP;
	g_conf.codecs[cod_type_ILBC_133].user_payload=100;
	g_conf.codecs[cod_type_ILBC_133].jb.jb_type=jb_type_FIXED;
	g_conf.codecs[cod_type_ILBC_133].jb.jb_loc_adpt=jb_loc_adpt_OFF;
	g_conf.codecs[cod_type_ILBC_133].jb.jb_scaling=1.4*16;
	g_conf.codecs[cod_type_ILBC_133].jb.jb_init_sz=120*8;
	g_conf.codecs[cod_type_ILBC_133].jb.jb_min_sz=10*8;
	g_conf.codecs[cod_type_ILBC_133].jb.jb_max_sz=200*8;
	
	g_conf.codecs[cod_type_G723].type=cod_type_G723;
	g_conf.codecs[cod_type_G723].pkt_size=cod_pkt_size_60;
	g_conf.codecs[cod_type_G723].bpack=bitpack_RTP;
	g_conf.codecs[cod_type_G723].user_payload=4;
	g_conf.codecs[cod_type_G723].jb.jb_type=jb_type_FIXED;
	g_conf.codecs[cod_type_G723].jb.jb_loc_adpt=jb_loc_adpt_OFF;
	g_conf.codecs[cod_type_G723].jb.jb_scaling=1.4*16;
	g_conf.codecs[cod_type_G723].jb.jb_init_sz=120*8;
	g_conf.codecs[cod_type_G723].jb.jb_min_sz=10*8;
	g_conf.codecs[cod_type_G723].jb.jb_max_sz=200*8;
	
	g_conf.codecs[cod_type_G726_16].type=cod_type_G726_16;
	g_conf.codecs[cod_type_G726_16].pkt_size=cod_pkt_size_60;
	g_conf.codecs[cod_type_G726_16].bpack=bitpack_AAL2;
	g_conf.codecs[cod_type_G726_16].user_payload=102;
	g_conf.codecs[cod_type_G726_16].jb.jb_type=jb_type_FIXED;
	g_conf.codecs[cod_type_G726_16].jb.jb_loc_adpt=jb_loc_adpt_OFF;
	g_conf.codecs[cod_type_G726_16].jb.jb_scaling=1.4*16;
	g_conf.codecs[cod_type_G726_16].jb.jb_init_sz=120*8;
	g_conf.codecs[cod_type_G726_16].jb.jb_min_sz=10*8;
	g_conf.codecs[cod_type_G726_16].jb.jb_max_sz=200*8;

	g_conf.codecs[cod_type_G726_24].type=cod_type_G726_24;
	g_conf.codecs[cod_type_G726_24].pkt_size=cod_pkt_size_60;
	g_conf.codecs[cod_type_G726_24].bpack=bitpack_AAL2;
	g_conf.codecs[cod_type_G726_24].user_payload=103;
	g_conf.codecs[cod_type_G726_24].jb.jb_type=jb_type_FIXED;
	g_conf.codecs[cod_type_G726_24].jb.jb_loc_adpt=jb_loc_adpt_OFF;
	g_conf.codecs[cod_type_G726_24].jb.jb_scaling=1.4*16;
	g_conf.codecs[cod_type_G726_24].jb.jb_init_sz=120*8;
	g_conf.codecs[cod_type_G726_24].jb.jb_min_sz=10*8;
	g_conf.codecs[cod_type_G726_24].jb.jb_max_sz=200*8;

	g_conf.codecs[cod_type_G726_32].type=cod_type_G726_32;
	g_conf.codecs[cod_type_G726_32].pkt_size=cod_pkt_size_60;
	g_conf.codecs[cod_type_G726_32].bpack=bitpack_AAL2;
	g_conf.codecs[cod_type_G726_32].user_payload=104;
	g_conf.codecs[cod_type_G726_32].jb.jb_type=jb_type_FIXED;
	g_conf.codecs[cod_type_G726_32].jb.jb_loc_adpt=jb_loc_adpt_OFF;
	g_conf.codecs[cod_type_G726_32].jb.jb_scaling=1.4*16;
	g_conf.codecs[cod_type_G726_32].jb.jb_init_sz=120*8;
	g_conf.codecs[cod_type_G726_32].jb.jb_min_sz=10*8;
	g_conf.codecs[cod_type_G726_32].jb.jb_max_sz=200*8;

	g_conf.codecs[cod_type_G726_40].type=cod_type_G726_40;
	g_conf.codecs[cod_type_G726_40].pkt_size=cod_pkt_size_60;
	g_conf.codecs[cod_type_G726_40].bpack=bitpack_AAL2;
	g_conf.codecs[cod_type_G726_40].user_payload=105;
	g_conf.codecs[cod_type_G726_40].jb.jb_type=jb_type_FIXED;
	g_conf.codecs[cod_type_G726_40].jb.jb_loc_adpt=jb_loc_adpt_OFF;
	g_conf.codecs[cod_type_G726_40].jb.jb_scaling=1.4*16;
	g_conf.codecs[cod_type_G726_40].jb.jb_init_sz=120*8;
	g_conf.codecs[cod_type_G726_40].jb.jb_min_sz=10*8;
	g_conf.codecs[cod_type_G726_40].jb.jb_max_sz=200*8;
	
	g_conf.codecs[TELEPHONE_EVENT_CODEC].type=TELEPHONE_EVENT_CODEC;
	g_conf.codecs[TELEPHONE_EVENT_CODEC].user_payload=106;
	
	/* Override defaults from config file */
	set = config_lookup (&cfg, "codecs" );
	if( !set ){
		SU_DEBUG_0(("No codecs entries in config file: %s\n", CODECS_CONF_NAME));
	} else {
		rec_num = config_setting_length (set);

		/* init one by one */
		for (i=0; i<rec_num; i++){
			rec_set = config_setting_get_elem (set, i);
			init_codec_el(rec_set, i);
		}
	}
	
	/* CODECS FOR FAX USAGE */
	/* set type and standart payload type values */
	g_conf.fax.codec_type = cod_type_ALAW;
	g_conf.fax.internal_pt = g_conf.fax.external_pt = g_conf.codecs[cod_type_ALAW].user_payload;

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
Originally written by Vladimir Luchko.\n\
This version butchered by Luca Olivetti <%s>\n\
"
		, PACKAGE_NAME, PACKAGE_VERSION,
		__DATE__, __TIME__, PACKAGE_BUGREPORT);
}/*}}}*/

