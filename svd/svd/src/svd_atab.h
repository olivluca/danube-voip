/**
 * @file svd_atab.h
 * Analog Telephone Adaptor interface.
 * It contains ATA actions in routine.
 */

#ifndef __SVD_ATAB_H__
#define __SVD_ATAB_H__

#include "svd.h"

/** @defgroup ATA_B ATA board functions.
 *  Manipulate with ATA board elements.
 *  @{*/
/** Create the ab struct and attach the callbacks to root.*/
int svd_atab_create (svd_t * const svd);
/** Destroy the ab struct in given svd.*/
void svd_atab_delete (svd_t * svd);
/** Clears call params that has been set during dial process.*/
void svd_clear_call(svd_t * const svd, ab_chan_t * const chan);
/** Found first free fxs channel.*/
int get_FF_FXS_idx ( ab_t const * const ab, char const self_chan_idx );
/** @}*/


/** @defgroup MEDIA Media activities.
 *  Manipulate with media on channel.
 *  @{*/
/** Attach the appropriate callbacks to RTP file on given channel.*/
int svd_media_register (svd_t * const svd, ab_chan_t * const chan);
/** Close RTP-socket and destroy the callback timers in root.*/
void svd_media_unregister (svd_t * const svd, ab_chan_t * const chan);
/** Re-SO_BINDTODEVICE on rtp-socket of the channel.*/
int svd_media_tapi_rtp_sock_rebinddev (svd_chan_t * const ctx);
/** Start encoding / decoding on given channel.*/
int ab_chan_media_activate ( ab_chan_t * const chan );
/** Stop encoding / decoding on given channel.*/
int ab_chan_media_deactivate ( ab_chan_t * const chan );
/** Get parameters of given codec type or name.*/
cod_prms_t const * svd_cod_prms_get(enum cod_type_e ct ,char const * const cn);
/** @}*/

/** Wait for seconds after ring on fxo before sent CANCEL to hotlined FXS. */
#define RING_WAIT_DROP 5

#endif /* __SVD_ATAB_H__ */

