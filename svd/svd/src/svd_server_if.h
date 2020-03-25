/**
 * @file svd_server_if.h
 * Start server part definition.
 * It containes main svd function to start server part.
 */
#ifndef __SVD_SERVER_IF_H__
#define __SVD_SERVER_IF_H__

/** @addgtoroup IF_SRV Interface server part.
 *  @{*/
int svd_create_interface( svd_t * svd );
void svd_destroy_interface(svd_t * svd);

/** @}*/

#endif /* __SVD_SERVER_IF_H__ */

