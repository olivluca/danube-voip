/**
 * @file svd_cfg.c
 * Configuration implementation.
 * It contains startup \ref g_so and main \ref g_conf
 * 		configuration features implementation.
 */

/*Includes {{{*/
#include "svd.h"
#include "svd_cfg.h"
#include "svd_log.h"
#include "svd_led.h"

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

/** @defgroup CFG_N Config file text values.
 *  @ingroup CFG_M
 *  Text values that can be in config file.
 *  @{*/
#define CONF_CODEC_G722 "g722"
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
#define CODEC_BASE cod_type_G722_64

/**
 * Ab context for configuration functions
 */

ab_t * global_ab;

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
	if (account->display)
		free (account->display);
	if (account->outbound_proxy)
		free (account->outbound_proxy);
#ifndef DONT_BIND_TO_DEVICE
	if (account->rtp_interface)
		free (account->rtp_interface);
#endif	
	free (account);
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
 * Uci configuration: main settings
 */
struct uci_main {
	struct ucimap_section_data map;
	int log_level;
	int rtp_port_first;
	int rtp_port_last;
	int sip_tos;
	int rtp_tos;
	char *led;
};

static int
main_init(struct uci_map *map, void *section, struct uci_section *s)
{
	return 0;
}

static int
main_add(struct uci_map *map, void *section)
{
	struct uci_main *a = section;
	
	g_conf.log_level = a->log_level;
	g_conf.rtp_port_first = a->rtp_port_first;
	g_conf.rtp_port_last = a->rtp_port_last;
	g_conf.sip_tos = a->sip_tos;
	g_conf.rtp_tos = a->rtp_tos;
	if (a->led) {
		g_conf.voip_led = strdup(a->led);
		/* turn it off immediately */
		led_off(a->led);
	}
	
	return 0;
}

static struct uci_optmap main_uci_map[] =
{
	{
		UCIMAP_OPTION(struct uci_main, log_level),
		.type = UCIMAP_INT,
		.name = "log_level",
	},{
		UCIMAP_OPTION(struct uci_main, rtp_port_first),
		.type = UCIMAP_INT,
		.name = "rtp_port_first",
	},{
		UCIMAP_OPTION(struct uci_main, rtp_port_last),
		.type = UCIMAP_INT,
		.name = "rtp_port_last",
	},{
		UCIMAP_OPTION(struct uci_main, sip_tos),
		.type = UCIMAP_INT,
		.name = "sip_tos",
	},{
		UCIMAP_OPTION(struct uci_main, rtp_tos),
		.type = UCIMAP_INT,
		.name = "rtp_tos",
	},{
		UCIMAP_OPTION(struct uci_main, led),
		.type = UCIMAP_STRING,
		.name = "led",
	},
};

static struct uci_sectionmap main_sectionmap = {
	UCIMAP_SECTION(struct uci_main, map),
	.type = "main",
	.init = main_init,
	.add = main_add,
	.options = main_uci_map,
	.n_options = ARRAY_SIZE(main_uci_map),
	.options_size = sizeof(struct uci_optmap),
};

/**
 * Uci configuration: sip accounts
 */
struct uci_account {
	struct ucimap_section_data map;
	char *name;
	bool disabled;
	char *user;
	char *domain;
	char *registrar;
	char *auth_name;
	char *password;
	char *display;
	char *outbound_proxy;
#ifndef DONT_BIND_TO_DEVICE
	char *rtp_interface;
#endif
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
	
	/* required fields */
	if (!a->user || !a->domain || !a->password
#ifndef DONT_BIND_TO_DEVICE
	    || !a->rtp_interface
#endif
	    ){
		SU_DEBUG_0(("settings for account %s incomplete:\n",a->name));
		if (!a->user)
			SU_DEBUG_0(("\t missing user"));
		if (!a->domain)
			SU_DEBUG_0(("\t missing domain"));
		if (!a->password)
			SU_DEBUG_0(("\t missing password"));
		goto __exit_fail;
	}
	 
	s->name = strdup(a->name);
	free(a->name);
	asprintf(&s->user_URI, "sip:%s@%s", a->user, a->domain);
	if (a->auth_name)
		s->user_name = strdup(a->auth_name);
	else
		s->user_name = strdup(a->user);
	s->user_pass = strdup(a->password);
	if (a->registrar)
		asprintf(&s->registrar, "sip:%s", a->registrar);
	else
		asprintf(&s->registrar, "sip:%s", a->domain);
	s->sip_domain = strdup(a->domain);
	if (a->display)
		s->display = strdup(a->display);
	if (a->outbound_proxy)
		asprintf(&s->outbound_proxy, "sip:%s", a->outbound_proxy);
#ifndef DONT_BIND_TO_DEVICE
	s->rtp_interface = strdup(a->rtp_interface);
#endif
	
	k=0;
	if (a->codecs && a->codecs->n_items > 0) {
		for (i=0; i<a->codecs->n_items; i++){
			codec_type = get_codec_type(a->codecs->item[i].s);
			if (codec_type != cod_type_NONE) 
				    s->codecs[k++]=codec_type;
		}
		if (k==0) {
			SU_DEBUG_0(("no valid codecs defined for account %s\n",s->name));
			goto __exit_fail;
		}
	} else {
		/* no codecs specified: use all defined codecs */
		for (i=0; g_conf.cp[i].type != cod_type_NONE; i++)
			if (g_conf.cp[i].type != TELEPHONE_EVENT_CODEC)
				s->codecs[k++]=g_conf.cp[i].type;
	}
	  
	if (a->dtmf) {
		s->dtmf=dtmf_off;
		if (!strcasecmp(a->dtmf,"rfc2883")) {
			s->dtmf=dtmf_2883;
		} else if (!strcasecmp(a->dtmf,"info")) {
			s->dtmf=dtmf_info;
		}
	} else {
		/* no dtmf mode specified, use rfc2883 */
		s->dtmf=dtmf_2883;
	}
	if (s->dtmf == dtmf_2883)
		s->codecs[k++] = TELEPHONE_EVENT_CODEC;

	if (a->outgoing_priority && a->outgoing_priority->n_items > 0) {
		for (i=0; i<a->outgoing_priority->n_items && i<g_conf.channels; i++)
			s->outgoing_priority[i] = a->outgoing_priority->item[i].i;
	} else {
		/* no outgoing priority specified, use this account for all channels,
		 * give more priority to accounts defined first
		 */
		int index = su_vector_len(g_conf.sip_account) + 1;
		for (i=0; i<g_conf.channels; i++)
			s->outgoing_priority[i] = index;;
	}
	if (a->ring_incoming && a->ring_incoming->n_items > 0) {
		for (i=0; i<a->ring_incoming->n_items && i<g_conf.channels; i++)
			s->ring_incoming[i] = a->ring_incoming->item[i].b;
	} else {
		/* no ring incoming specified, ring all channels */
		for (i=0; i<g_conf.channels; i++)
			s->ring_incoming[i] = 1;
	}
	s->enabled = 1;
	/* no way to check for absence of an option, so use option disabled instead of enabled */
	if (a->disabled)
		s->enabled = 0;
	su_vector_append(g_conf.sip_account, s);
	return 0;

__exit_fail:	
	if (s)
		sip_free(s);
	return 0;
}

static struct uci_optmap account_uci_map[] =
{
	{
		UCIMAP_OPTION(struct uci_account, disabled),
		.type = UCIMAP_BOOL,
		.name = "disabled",
	},{
		UCIMAP_OPTION(struct uci_account, user),
		.type = UCIMAP_STRING,
		.name = "user",
	},{
		UCIMAP_OPTION(struct uci_account, domain),
		.type = UCIMAP_STRING,
		.name = "domain",
	},{
		UCIMAP_OPTION(struct uci_account, registrar),
		.type = UCIMAP_STRING,
		.name = "registrar",
	},{
		UCIMAP_OPTION(struct uci_account, auth_name),
		.type = UCIMAP_STRING,
		.name = "auth_name",
	},{
		UCIMAP_OPTION(struct uci_account, password),
		.type = UCIMAP_STRING,
		.name = "password",
	},{
		UCIMAP_OPTION(struct uci_account, display),
		.type = UCIMAP_STRING,
		.name = "display",
	},{
		UCIMAP_OPTION(struct uci_account, outbound_proxy),
		.type = UCIMAP_STRING,
		.name = "outbound_proxy",
#ifndef DONT_BIND_TO_DEVICE
	},{
		UCIMAP_OPTION(struct uci_account, rtp_interface),
		.type = UCIMAP_STRING,
		.name = "rtp_interface",
#endif
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

/**
 * Uci configuration: dialplan
 */
struct uci_dialplan {
	struct ucimap_section_data map;
	char *prefix;
	char *replace;
	bool remove_prefix;
	char *account;
};

static int
dialplan_init(struct uci_map *map, void *section, struct uci_section *s)
{
	return 0;
}

static int
dialplan_add(struct uci_map *map, void *section)
{
	struct uci_dialplan *a = section;
	struct dplan_record_s *d;
	int account_index = -1;
	int i;
	
	if (!a->prefix || strlen(a->prefix) == 0) { 
		SU_DEBUG_0(("dialplan: missing prefix\n"));
		goto __exit_fail;
	}
	
	if (!a->account) { 
		SU_DEBUG_0(("dialplan: missing account\n"));
		goto __exit_fail;
	}
	
	for (i=0; i<su_vector_len(g_conf.sip_account); i++) {
		sip_account_t *account = su_vector_item(g_conf.sip_account, i);
		if (!strcasecmp(a->account, account->name)) {
			account_index = i;
			break;
		}
	}
		  
	if (account_index<0) { 
		SU_DEBUG_0(("dialplan: account %s not found\n", a->account));
		goto __exit_fail;
	}
	
	d = malloc(sizeof(*d));
	if( !d ){
		SU_DEBUG_0((LOG_FNC_A(LOG_NOMEM)));
		goto __exit_fail;
	}
	memset(d, 0, sizeof(*d));
	d->prefix = strdup(a->prefix);
	d->prefixlen = strlen(d->prefix);
	
	if (a->replace)
		d->replace = strdup(a->replace);
	else {
		if (a->remove_prefix)
			d->replace = strdup("");
		else
			d->replace = strdup(a->prefix);
	}
	d->account = account_index;
	su_vector_append(g_conf.dial_plan, d);
	return 0;
__exit_fail:	
	if (d)
		dialplan_free(d);
	return 0;
}

static struct uci_optmap dialplan_uci_map[] =
{
	{
		UCIMAP_OPTION(struct uci_dialplan, prefix),
		.type = UCIMAP_STRING,
		.name = "prefix",
	},{
		UCIMAP_OPTION(struct uci_dialplan, replace),
		.type = UCIMAP_STRING,
		.name = "replace",
	},{
		UCIMAP_OPTION(struct uci_dialplan, remove_prefix),
		.type = UCIMAP_BOOL,
		.name = "remove_prefix",
	},{
		UCIMAP_OPTION(struct uci_dialplan, account),
		.type = UCIMAP_STRING,
		.name = "account",
	},
};

static struct uci_sectionmap dialplan_sectionmap = {
	UCIMAP_SECTION(struct uci_dialplan, map),
	.type = "dialplan",
	.init = dialplan_init,
	.add = dialplan_add,
	.options = dialplan_uci_map,
	.n_options = ARRAY_SIZE(dialplan_uci_map),
	.options_size = sizeof(struct uci_optmap),
};

/**
 * Uci configuration: codecs overrides
 */
struct uci_codec {
	struct ucimap_section_data map;
	int  codec_type;
	char *packet_size;
	int payload;
	char *bitpack;
	char *jb_type;
	bool local_adaptation;
	char *scaling;
	int jb_init_sz;
	int jb_min_sz;
	int jb_max_sz;
};

static int
codec_init(struct uci_map *map, void *section, struct uci_section *s)
{
	struct uci_codec *a = section;

	a->codec_type = get_codec_type(s->e.name);
	if (a->codec_type == cod_type_NONE)
		SU_DEBUG_0(("Wrong codec name \"%s\"\n",
				s->e.name));
	
	return 0;
}

static int
codec_add(struct uci_map *map, void *section)
{
	struct uci_codec *a = section;
	codec_t * cod;
	float scal;
	
	if (a->codec_type == cod_type_NONE) {
		goto __exit_fail;		
	}
	
	cod=&g_conf.codecs[a->codec_type];
	
	if (a->packet_size) {
		if       ( !strcmp(a->packet_size,"2.5")){
			cod->pkt_size = cod_pkt_size_2_5;
		} else if( !strcmp(a->packet_size, "5")){
			cod->pkt_size = cod_pkt_size_5;
		} else if( !strcmp(a->packet_size, "5.5")){
			cod->pkt_size = cod_pkt_size_5_5;
		} else if( !strcmp(a->packet_size, "10")){
			cod->pkt_size = cod_pkt_size_10;
		} else if( !strcmp(a->packet_size, "11")){
			cod->pkt_size = cod_pkt_size_11;
		} else if( !strcmp(a->packet_size, "20")){
			cod->pkt_size = cod_pkt_size_20;
		} else if( !strcmp(a->packet_size, "30")){
			cod->pkt_size = cod_pkt_size_30;
		} else if( !strcmp(a->packet_size, "40")){
			cod->pkt_size = cod_pkt_size_40;
		} else if( !strcmp(a->packet_size, "50")){
			cod->pkt_size = cod_pkt_size_50;
		} else if( !strcmp(a->packet_size, "60")){
			cod->pkt_size = cod_pkt_size_60;
		}
	}
	
	if (a->payload)
		cod->user_payload = a->payload;
	
	if (a->bitpack) {
		if       ( !strcasecmp(a->bitpack, CONF_CODEC_BITPACK_RTP)){
			cod->bpack = bitpack_RTP;
		} else if( !strcasecmp(a->bitpack, CONF_CODEC_BITPACK_AAL2)){
			cod->bpack = bitpack_AAL2;
		}
	}
	
	if (a->jb_type) {
		if       ( !strcasecmp(a->jb_type, CONF_JB_TYPE_FIXED)){
			cod->jb.jb_type = jb_type_FIXED;
		} else if( !strcasecmp(a->jb_type, CONF_JB_TYPE_ADAPTIVE)){
			cod->jb.jb_type = jb_type_ADAPTIVE;
		}
	}

	if (a->local_adaptation)
		cod->jb.jb_loc_adpt = jb_loc_adpt_ON;
	else
		cod->jb.jb_loc_adpt = jb_loc_adpt_OFF;
	
	if (a->scaling) {
	      scal=atof(a->scaling);
	      if ((int)scal != 0)
		cod->jb.jb_scaling = scal * 16;
	}
	
	if (a->jb_init_sz)
		cod->jb.jb_init_sz = a->jb_init_sz;
	if (a->jb_min_sz)
		cod->jb.jb_min_sz = a->jb_min_sz;
	if (a->jb_max_sz)
		cod->jb.jb_max_sz = a->jb_max_sz;

__exit_fail:	
	return 0;
}

static struct uci_optmap codec_uci_map[] =
{
	{
		UCIMAP_OPTION(struct uci_codec, packet_size),
		.type = UCIMAP_STRING,
		.name = "packet_size",
	},{
		UCIMAP_OPTION(struct uci_codec, payload),
		.type = UCIMAP_INT,
		.name = "payload",
	},{
		UCIMAP_OPTION(struct uci_codec, bitpack),
		.type = UCIMAP_STRING,
		.name = "bitpack",
	},{
		UCIMAP_OPTION(struct uci_codec, jb_type),
		.type = UCIMAP_STRING,
		.name = "jb_type",
	},{
		UCIMAP_OPTION(struct uci_codec, local_adaptation),
		.type = UCIMAP_BOOL,
		.name = "local_adaptation",
	},{
		UCIMAP_OPTION(struct uci_codec, scaling),
		.type = UCIMAP_STRING,
		.name = "scaling",
	},{
		UCIMAP_OPTION(struct uci_codec, jb_init_sz),
		.type = UCIMAP_INT,
		.name = "jb_init_sz",
	},{
		UCIMAP_OPTION(struct uci_codec, jb_min_sz),
		.type = UCIMAP_INT,
		.name = "jb_min_sz",
	},{
		UCIMAP_OPTION(struct uci_codec, jb_max_sz),
		.type = UCIMAP_INT,
		.name = "jb_max_sz",
	},
};

static struct uci_sectionmap codec_sectionmap = {
	UCIMAP_SECTION(struct uci_codec, map),
	.type = "codec",
	.init = codec_init,
	.add = codec_add,
	.options = codec_uci_map,
	.n_options = ARRAY_SIZE(codec_uci_map),
	.options_size = sizeof(struct uci_optmap),
};

/**
 * Uci configuration: channel settings
 */
struct uci_channel {
	struct ucimap_section_data map;
	int  channel;
	int enc_db;
	int dec_db;
	char *vad;
	bool hpf;
	char *wlec_type;
	bool wlec_nlp;
	int wlec_ne_nb;
	int wlec_fe_nb;
	char *cid;
	char *led;
};

static int
channel_init(struct uci_map *map, void *section, struct uci_section *s)
{
	struct uci_channel *a = section;

	errno = 0;
	a->channel = strtol(s->e.name, NULL, 0) - 1;
	if (errno != 0 || a->channel<0 || a->channel >= g_conf.channels) {
		SU_DEBUG_0(("Wrong channel number \"%s\"\n",
				s->e.name));
		a->channel = -1;
	}

	return 0;
}

static int
channel_add(struct uci_map *map, void *section)
{
	struct uci_channel *a = section;
	struct rtp_session_prms_s * c;
	struct wlec_s * w;
	
	if (a->channel < 0 || a->channel >= g_conf.channels)
		return -1;
	
	/* rtp audio parameters */
	c = &g_conf.audio_prms[a->channel];
	
	if (a->enc_db)
		c->enc_dB = a->enc_db;
	if (a->dec_db)
		c->dec_dB = a->dec_db;
	if (a->vad) {
		if( !strcasecmp(a->vad, CONF_VAD_NOVAD)){
		c->VAD_cfg = vad_cfg_OFF;
		} else if( !strcasecmp(a->vad, CONF_VAD_ON)){
			c->VAD_cfg = vad_cfg_ON;
		} else if( !strcasecmp(a->vad, CONF_VAD_G711)){
			c->VAD_cfg = vad_cfg_G711;
		} else if( !strcasecmp(a->vad, CONF_VAD_CNG_ONLY)){
			c->VAD_cfg = vad_cfg_CNG_only;
		} else if( !strcasecmp(a->vad, CONF_VAD_SC_ONLY)){
			c->VAD_cfg = vad_cfg_SC_only;
		}
	}
	
	c->HPF_is_ON = a->hpf;
	
	/* wlec parameters */
	w = &g_conf.wlec_prms[a->channel];
	if (a->wlec_type ) {
		if       ( !strcasecmp(a->wlec_type, CONF_WLEC_TYPE_OFF)){
			w->mode = wlec_mode_OFF;
		} else if( !strcasecmp(a->wlec_type, CONF_WLEC_TYPE_NE)){
			w->mode = wlec_mode_NE;
		} else if( !strcasecmp(a->wlec_type, CONF_WLEC_TYPE_NFE)){
			w->mode = wlec_mode_NFE;
		}
	  
	}
	
	if (a->wlec_nlp)
		w->nlp = wlec_nlp_ON;
	else
		w->nlp = wlec_nlp_OFF;

	if (a->wlec_ne_nb)
		w->ne_nb = a->wlec_ne_nb;
	if (a->wlec_fe_nb)
		w->fe_nb = a->wlec_fe_nb;
		
	if(w->mode == wlec_mode_NFE){
		if(w->ne_nb == 16){
			w->ne_nb = 8;
		}
		if(w->fe_nb == 16){
			w->fe_nb = 8;
		}
	}
	
	/* caller id standard */
	if (a->cid) {
		SU_DEBUG_0(("Setting caller id standard for channel %d to %s\n", a->channel+1, a->cid));
		if (svd_set_cid(&global_ab->chans[a->channel], a->cid)) {
			SU_DEBUG_0(("Invalid caller id %s\n",a->cid));
		}
	}
	
	/* led */
	if (a->led) {
		g_conf.chan_led[a->channel] = strdup(a->led);
		/* turn it off immediately */
		led_off(a->led);
	}
		
	return 0;
}

static struct uci_optmap channel_uci_map[] =
{
	{
		UCIMAP_OPTION(struct uci_channel, enc_db),
		.type = UCIMAP_INT,
		.name = "enc_db",
	},{
		UCIMAP_OPTION(struct uci_channel, dec_db),
		.type = UCIMAP_INT,
		.name = "dec_db",
	},{
		UCIMAP_OPTION(struct uci_channel, vad),
		.type = UCIMAP_STRING,
		.name = "vad",
	},{
		UCIMAP_OPTION(struct uci_channel, hpf),
		.type = UCIMAP_BOOL,
		.name = "hpf",
	},{
		UCIMAP_OPTION(struct uci_channel, wlec_type),
		.type = UCIMAP_STRING,
		.name = "wlec_type",
	},{
		UCIMAP_OPTION(struct uci_channel, wlec_nlp),
		.type = UCIMAP_BOOL,
		.name = "wlec_nlp",
	},{
		UCIMAP_OPTION(struct uci_channel, wlec_ne_nb),
		.type = UCIMAP_INT,
		.name = "wlec_ne_nb",
	},{
		UCIMAP_OPTION(struct uci_channel, wlec_fe_nb),
		.type = UCIMAP_INT,
		.name = "wlec_fe_nb",
	},{
		UCIMAP_OPTION(struct uci_channel, cid),
		.type = UCIMAP_STRING,
		.name = "cid",
	},{
		UCIMAP_OPTION(struct uci_channel, led),
		.type = UCIMAP_STRING,
		.name = "led",
	},
};

static struct uci_sectionmap channel_sectionmap = {
	UCIMAP_SECTION(struct uci_channel, map),
	.type = "channel",
	.init = channel_init,
	.add = channel_add,
	.options = channel_uci_map,
	.n_options = ARRAY_SIZE(channel_uci_map),
	.options_size = sizeof(struct uci_optmap),
};

/**
 * Uci configuration: load everything
 */
static struct uci_sectionmap *svd_smap[] = {
	&main_sectionmap,
	&account_sectionmap,
	&dialplan_sectionmap,
	&codec_sectionmap,
	&channel_sectionmap,
};

static struct uci_map svd_map = {
	.sections = svd_smap,
	.n_sections = ARRAY_SIZE(svd_smap),
};

int
uci_config_load(void)
{
	int ret;
	struct uci_context *ctx = uci_alloc_context();
	struct uci_package *pkg;

	ucimap_init(&svd_map);
	ret = uci_load(ctx, "svd", &pkg);
	if (ret) {
		char *errmsg;
		uci_get_errorstr(ctx, &errmsg, NULL);
		SU_DEBUG_0(("Error loading configuration file: %s\n", errmsg));
		free(errmsg);
	}
	ucimap_parse(&svd_map, pkg);
	ucimap_cleanup(&svd_map);
	uci_free_context(ctx);
	return ret;
}


/** @}*/

/** @defgroup CFG_IF Config internal functions.
 *  @ingroup CFG_M
 *  This functions using while reading config file.
 *  @{*/
/** Init AUDIO parameters configuration.*/
static void audio_defaults (void);
/** Init WLEC defaults.*/
static void wlec_defaults (ab_t const * const ab);
/** Init codecs definitions.*/
static void codec_defaults (void);
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
	int err = -1;
	/* default presets */
	memset (&g_conf, 0, sizeof(g_conf));

	global_ab = (ab_t *)ab;
	g_conf.channels = ab->chans_per_dev;

	g_conf.sip_account = su_vector_create(home,sip_free);
	if( !g_conf.sip_account ){
		SU_DEBUG_0((LOG_FNC_A(LOG_NOMEM)));
		goto __exit;
	}
	
	g_conf.dial_plan = su_vector_create(home, dialplan_free);
	if( !g_conf.dial_plan ){
		SU_DEBUG_0((LOG_FNC_A(LOG_NOMEM)));
		goto __exit;
	}

	codec_defaults();
	audio_defaults();
	wlec_defaults(ab);
	
	if(uci_config_load()){
		goto __exit;
	}
	
	bool at_least_one_account = false;
	int i;
	for (i=0; i<su_vector_len(g_conf.sip_account); i++) {
		sip_account_t * account = su_vector_item(g_conf.sip_account, i);
		if (account->enabled) {
			  at_least_one_account = true;
			  break;
		}
	}

	if (g_conf.rtp_port_first && g_conf.rtp_port_last && g_conf.sip_tos &&
	    g_conf.rtp_tos && at_least_one_account) {
		err = 0;
		goto __exit;
	}
	
	if (!g_conf.rtp_port_first)
		SU_DEBUG_0(("Missing/invalid \"option rtp_port_first\" in \"config main\"\n"));
	if (!g_conf.rtp_port_last)
		SU_DEBUG_0(("Missing/invalid \"option rtp_port_last\" in \"config main\"\n"));
	if (!g_conf.sip_tos)
		SU_DEBUG_0(("Missing/invalid \"option sip_tos\" in \"config main\"\n"));
	if (!g_conf.rtp_tos)
		SU_DEBUG_0(("Missing/invalid \"option rtp_tos\" in \"config main\"\n"));
	if (!at_least_one_account)
		SU_DEBUG_0(("No accounts defined/enabled\n"));

__exit:
	conf_show();
	return err;
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
	SU_DEBUG_3((" led[%s] : ", g_conf.voip_led));
	SU_DEBUG_3(("ports[%d:%d]\n",
			g_conf.rtp_port_first,
			g_conf.rtp_port_last));

	for (i=i; i<COD_MAS_SIZE; i++) if (g_conf.codecs[i].type!=cod_type_NONE) {
		SU_DEBUG_3(("t:%s/bp%d/sz%d/pt:0x%X__[%d:%d]::[%d:%d:%d:%d]\n",
				g_conf.cp[g_conf.codecs[i].type-CODEC_BASE].sdp_name,
				g_conf.codecs[i].bpack,
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
		SU_DEBUG_3(("SIP net %d : %s, enabled: %d\n", i, curr_sip_rec->name, curr_sip_rec->enabled));
		SU_DEBUG_3((	"\tCodecs:\t"));
		for (j=0; curr_sip_rec->codecs[j] != cod_type_NONE; j++){
		      SU_DEBUG_3(("%s ",
				g_conf.cp[curr_sip_rec->codecs[j]-CODEC_BASE].sdp_name
				));
		}
		SU_DEBUG_3(("\n"));
		SU_DEBUG_3((	"\tRegistrar     : '%s'\n"
				"\tOutbound proxy: '%s'\n"
				"\tUser/Pass     : '%s/%s'\n"
				"\tUser_URI      : '%s'\n"
				"\tDisplay name  : '%s'\n",
				curr_sip_rec->registrar,
				curr_sip_rec->outbound_proxy ? curr_sip_rec->outbound_proxy : "(none)",
				curr_sip_rec->user_name,
				curr_sip_rec->user_pass,
				curr_sip_rec->user_URI,
				curr_sip_rec->display ? curr_sip_rec->display : "(none)"));
				
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

	/* rtp audio and wlec parameters */
	for (i=0; i<g_conf.channels; i++) {
		struct rtp_session_prms_s * c = &g_conf.audio_prms[i];
		struct wlec_s * w = &g_conf.wlec_prms[i];
		SU_DEBUG_3(("chan %d led % s enc_dB %d dec_db %d vad %d hpf %d wlec_mode %d wlec_nlp %d wlec_ne_nb %d wlec_fe_nb %d\n",
			    i, g_conf.chan_led[i], c->enc_dB, c->dec_dB, c->VAD_cfg, c->HPF_is_ON,
			    w->mode, w->nlp, w->ne_nb, w->fe_nb));
	}

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
	int i;
	
	if (g_conf.dial_plan)
	  su_vector_destroy(g_conf.dial_plan);
	
	if (g_conf.sip_account)
	  su_vector_destroy(g_conf.sip_account);
	
	if (g_conf.voip_led) {
	  led_off(g_conf.voip_led);
	  free(g_conf.voip_led);
	}
	
	for (i=0; i<g_conf.channels; i++) {
	  if (g_conf.chan_led[i]) {
	    led_off(g_conf.chan_led[i]);
	    free(g_conf.chan_led[i]);
	  }
	}
	
	memset(&g_conf, 0, sizeof(g_conf));
}/*}}}*/

/**
 * Init codecs names and default parameters
 */
static void
codec_defaults( void )
{/*{{{*/
	static char * empty = "";
	int i;

	memset(g_conf.cp, 0, sizeof(g_conf.cp));

	for (i=0; i<COD_MAS_SIZE; i++){
		g_conf.cp[i].type = cod_type_NONE;
	}

	/* Init names and basic parameters */
	/* G722_64 parameters. */
	i=cod_type_G722_64-CODEC_BASE;
	g_conf.cp[i].type = cod_type_G722_64;
	g_conf.cp[i].sdp_name=strdup("G722");
	g_conf.cp[i].fmtp_str=empty;
	g_conf.cp[i].rate = 8000; /* per RFC3551, 16000Hz in reality */
	
	/* G711 ALAW parameters. */
	i=cod_type_ALAW-CODEC_BASE;
	g_conf.cp[i].type = cod_type_ALAW;
	g_conf.cp[i].sdp_name=strdup("PCMA");
	g_conf.cp[i].fmtp_str=empty;
	g_conf.cp[i].rate = 8000;

	/* G729 parameters. */
	i=cod_type_G729-CODEC_BASE;
	g_conf.cp[i].type = cod_type_G729;
	g_conf.cp[i].sdp_name=strdup("G729");
	g_conf.cp[i].fmtp_str=empty;
	g_conf.cp[i].rate = 8000;

	/* G729E parameters. */
	i=cod_type_G729E-CODEC_BASE;
	g_conf.cp[i].type = cod_type_G729E;
	g_conf.cp[i].sdp_name=strdup("G729E");
	g_conf.cp[i].fmtp_str=empty;
	g_conf.cp[i].rate = 8000;

	/* G723 parameters. */
	i=cod_type_G723-CODEC_BASE;
	g_conf.cp[i].type = cod_type_G723;
	g_conf.cp[i].sdp_name=strdup("G723");
	g_conf.cp[i].fmtp_str=empty;
	g_conf.cp[i].rate = 8000;

	/* iLBC_133 parameters. */
	i=cod_type_ILBC_133-CODEC_BASE;
	g_conf.cp[i].type = cod_type_ILBC_133;
	g_conf.cp[i].sdp_name=strdup("iLBC");
	g_conf.cp[i].fmtp_str=strdup("mode=30");
	g_conf.cp[i].rate = 8000;

	/* iLBC_152 parameters.
	g_conf.cp[i].type = cod_type_ILBC_152;
	if(     strlen("iLBC") >= COD_NAME_LEN ||
			strlen("mode=20") >= FMTP_STR_LEN){
		goto __exit_fail;
	}
	strcpy(g_conf.cp[i].sdp_name, "iLBC");
	strcpy(g_conf.cp[i].fmtp_str, "mode=20");
	g_conf.cp[i].rate = 8000;
	i++;
	*/

	/* G726_16 parameters. */
	i=cod_type_G726_16-CODEC_BASE;
	g_conf.cp[i].type = cod_type_G726_16;
	g_conf.cp[i].sdp_name=strdup("G726-16");
	g_conf.cp[i].fmtp_str=empty;
	g_conf.cp[i].rate = 8000;

	/* G726_ parameters. */
	i=cod_type_G726_24-CODEC_BASE;
	g_conf.cp[i].type = cod_type_G726_24;
	g_conf.cp[i].sdp_name=strdup("G726-24");
	g_conf.cp[i].fmtp_str=empty;
	g_conf.cp[i].rate = 8000;

	/* G726_ parameters. */
	i=cod_type_G726_32-CODEC_BASE;
	g_conf.cp[i].type = cod_type_G726_32;
	g_conf.cp[i].sdp_name=strdup("G726-32");
	g_conf.cp[i].fmtp_str=empty;
	g_conf.cp[i].rate = 8000;

	/* G726_ parameters. */
	i=cod_type_G726_40-CODEC_BASE;
	g_conf.cp[i].type = cod_type_G726_40;
	g_conf.cp[i].sdp_name=strdup("G726-40");
	g_conf.cp[i].fmtp_str=empty;
	g_conf.cp[i].rate = 8000;
	
	/* telephone event parameters. */
	i=TELEPHONE_EVENT_CODEC-CODEC_BASE;
	g_conf.cp[i].type = TELEPHONE_EVENT_CODEC;
	g_conf.cp[i].sdp_name=strdup("telephone-event");
	g_conf.cp[i].fmtp_str=strdup("0-16");
	g_conf.cp[i].rate = 8000;

	/* set all to NONE */
	memset(g_conf.codecs, 0, sizeof(g_conf.codecs));
	for (i=0; i<COD_MAS_SIZE; i++){
		g_conf.codecs[i].type = cod_type_NONE;
	}

	/* Default values for the codecs */
	g_conf.codecs[cod_type_G722_64].type=cod_type_G722_64;
	g_conf.codecs[cod_type_G722_64].pkt_size=cod_pkt_size_20;
	g_conf.codecs[cod_type_G722_64].bpack=bitpack_RTP;
	g_conf.codecs[cod_type_G722_64].user_payload=9;
	g_conf.codecs[cod_type_G722_64].jb.jb_type=jb_type_FIXED;
	g_conf.codecs[cod_type_G722_64].jb.jb_loc_adpt=jb_loc_adpt_OFF;
	g_conf.codecs[cod_type_G722_64].jb.jb_scaling=1.4*16;
	g_conf.codecs[cod_type_G722_64].jb.jb_init_sz=120*8;
	g_conf.codecs[cod_type_G722_64].jb.jb_min_sz=10*8;
	g_conf.codecs[cod_type_G722_64].jb.jb_max_sz=200*8;
	
	g_conf.codecs[cod_type_ALAW].type=cod_type_ALAW;
	g_conf.codecs[cod_type_ALAW].pkt_size=cod_pkt_size_20;
	g_conf.codecs[cod_type_ALAW].bpack=bitpack_RTP;
	g_conf.codecs[cod_type_ALAW].user_payload=8;
	g_conf.codecs[cod_type_ALAW].jb.jb_type=jb_type_FIXED;
	g_conf.codecs[cod_type_ALAW].jb.jb_loc_adpt=jb_loc_adpt_OFF;
	g_conf.codecs[cod_type_ALAW].jb.jb_scaling=1.4*16;
	g_conf.codecs[cod_type_ALAW].jb.jb_init_sz=120*8;
	g_conf.codecs[cod_type_ALAW].jb.jb_min_sz=10*8;
	g_conf.codecs[cod_type_ALAW].jb.jb_max_sz=200*8;
	
	g_conf.codecs[cod_type_G729].type=cod_type_G729;
	g_conf.codecs[cod_type_G729].pkt_size=cod_pkt_size_10;
	g_conf.codecs[cod_type_G729].bpack=bitpack_RTP;
	g_conf.codecs[cod_type_G729].user_payload=18;
	g_conf.codecs[cod_type_G729].jb.jb_type=jb_type_FIXED;
	g_conf.codecs[cod_type_G729].jb.jb_loc_adpt=jb_loc_adpt_OFF;
	g_conf.codecs[cod_type_G729].jb.jb_scaling=1.4*16;
	g_conf.codecs[cod_type_G729].jb.jb_init_sz=120*8;
	g_conf.codecs[cod_type_G729].jb.jb_min_sz=10*8;
	g_conf.codecs[cod_type_G729].jb.jb_max_sz=200*8;
	
	g_conf.codecs[cod_type_G729E].type=cod_type_G729E;
	g_conf.codecs[cod_type_G729E].pkt_size=cod_pkt_size_10;
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
	g_conf.codecs[cod_type_G723].pkt_size=cod_pkt_size_30;
	g_conf.codecs[cod_type_G723].bpack=bitpack_RTP;
	g_conf.codecs[cod_type_G723].user_payload=4;
	g_conf.codecs[cod_type_G723].jb.jb_type=jb_type_FIXED;
	g_conf.codecs[cod_type_G723].jb.jb_loc_adpt=jb_loc_adpt_OFF;
	g_conf.codecs[cod_type_G723].jb.jb_scaling=1.4*16;
	g_conf.codecs[cod_type_G723].jb.jb_init_sz=120*8;
	g_conf.codecs[cod_type_G723].jb.jb_min_sz=10*8;
	g_conf.codecs[cod_type_G723].jb.jb_max_sz=200*8;
	
	g_conf.codecs[cod_type_G726_16].type=cod_type_G726_16;
	g_conf.codecs[cod_type_G726_16].pkt_size=cod_pkt_size_10;
	g_conf.codecs[cod_type_G726_16].bpack=bitpack_AAL2;
	g_conf.codecs[cod_type_G726_16].user_payload=102;
	g_conf.codecs[cod_type_G726_16].jb.jb_type=jb_type_FIXED;
	g_conf.codecs[cod_type_G726_16].jb.jb_loc_adpt=jb_loc_adpt_OFF;
	g_conf.codecs[cod_type_G726_16].jb.jb_scaling=1.4*16;
	g_conf.codecs[cod_type_G726_16].jb.jb_init_sz=120*8;
	g_conf.codecs[cod_type_G726_16].jb.jb_min_sz=10*8;
	g_conf.codecs[cod_type_G726_16].jb.jb_max_sz=200*8;

	g_conf.codecs[cod_type_G726_24].type=cod_type_G726_24;
	g_conf.codecs[cod_type_G726_24].pkt_size=cod_pkt_size_10;
	g_conf.codecs[cod_type_G726_24].bpack=bitpack_AAL2;
	g_conf.codecs[cod_type_G726_24].user_payload=103;
	g_conf.codecs[cod_type_G726_24].jb.jb_type=jb_type_FIXED;
	g_conf.codecs[cod_type_G726_24].jb.jb_loc_adpt=jb_loc_adpt_OFF;
	g_conf.codecs[cod_type_G726_24].jb.jb_scaling=1.4*16;
	g_conf.codecs[cod_type_G726_24].jb.jb_init_sz=120*8;
	g_conf.codecs[cod_type_G726_24].jb.jb_min_sz=10*8;
	g_conf.codecs[cod_type_G726_24].jb.jb_max_sz=200*8;

	g_conf.codecs[cod_type_G726_32].type=cod_type_G726_32;
	g_conf.codecs[cod_type_G726_32].pkt_size=cod_pkt_size_10;
	g_conf.codecs[cod_type_G726_32].bpack=bitpack_AAL2;
	g_conf.codecs[cod_type_G726_32].user_payload=104;
	g_conf.codecs[cod_type_G726_32].jb.jb_type=jb_type_FIXED;
	g_conf.codecs[cod_type_G726_32].jb.jb_loc_adpt=jb_loc_adpt_OFF;
	g_conf.codecs[cod_type_G726_32].jb.jb_scaling=1.4*16;
	g_conf.codecs[cod_type_G726_32].jb.jb_init_sz=120*8;
	g_conf.codecs[cod_type_G726_32].jb.jb_min_sz=10*8;
	g_conf.codecs[cod_type_G726_32].jb.jb_max_sz=200*8;

	g_conf.codecs[cod_type_G726_40].type=cod_type_G726_40;
	g_conf.codecs[cod_type_G726_40].pkt_size=cod_pkt_size_10;
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
	
	/* CODECS FOR FAX USAGE */
	/* set type and standart payload type values */
	g_conf.fax.codec_type = cod_type_ALAW;
	g_conf.fax.internal_pt = g_conf.fax.external_pt = g_conf.codecs[cod_type_ALAW].user_payload;

}/*}}}*/

/**
 * Init`s AUDIO parameters in main routine configuration \ref g_conf structure.
 *
 * \retval 0 success.
 * \retval -1 fail.
 */
static void
audio_defaults( void )
{/*{{{*/
	struct rtp_session_prms_s * curr_rec;
	int i;

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
}/*}}}*/

/**
 * Init`s WLEC default parameters
 */
static void
wlec_defaults( ab_t const * const ab )
{/*{{{*/
	struct wlec_s * curr_rec;
	ab_chan_t * cc; /* current channel */
	int i;
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

