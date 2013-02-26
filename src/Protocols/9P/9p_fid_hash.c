/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright CEA/DAM/DIF  (2011)
 * contributeur : Philippe DENIEL   philippe.deniel@cea.fr
 *                Thomas LEIBOVICI  thomas.leibovici@cea.fr
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * ---------------------------------------
 */

/**
 * \file    9p_fid_hash.c
 * \brief   9P FID Management as a hashtable
 *
 * 9p_fid_hash.c functions dedicated to Managing FIDs in a hashtable
 *
 */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "nfs_core.h"
#include "log.h"
#include "9p.h"

hash_table_t * ht_9p_fid;

/**
 * @brief Compare two 9p fid key
 *
 * @param[in] owner1 one fid key
 * @param[in] owner2 Another Fid key
 *
 * @retval 1 if they differ.
 * @retval 0 if they're identical.
 */
int compare_9p_fid_key(struct gsh_buffdesc * key1,
                       struct gsh_buffdesc * key2)
{
  _9p_fid_key_t * pk1 = (_9p_fid_key_t *)(key1->addr) ;
  _9p_fid_key_t * pk2 = (_9p_fid_key_t *)(key2->addr) ;

  return ( ( pk1->client_addr == pk2->client_addr ) &&
           ( pk1->trans_type  == pk2->trans_type  ) && 
           ( pk1->fid         == pk2->fid ) ) ? 0 : 1 ;
}

/**
 * @brief Get the hash index from a 9p fid key
 *
 * @param[in] hparam Hash parameters
 * @param[in] key The key to hash
 *
 * @return The hash index.
 */

uint32_t _9p_fid_value_hash_func(hash_parameter_t *hparam,
                                 struct gsh_buffdesc *key)
{
  return (  ((_9p_fid_key_t *)(key->addr))->client_addr + (((_9p_fid_key_t *)(key->addr))->fid << 1 ) 
          + ((_9p_fid_key_t *)(key->addr))->trans_type )  % hparam->index_size ;
}

/**
 * @brief Get the RBT hash from a 9p fid key
 *
 * @param[in] hparam Hash parameters
 * @param[in] key The key to hash
 *
 * @return The RBT hash.
 */
uint64_t _9p_fid_rbt_hash_func(hash_parameter_t *hparam,
                               struct gsh_buffdesc *key)
{
  /* Build a 64 bits number this way : first the address then the fid shited by one bit + the trans_type
   * Reasonable assumption is made here : fid is smaller than 2^31 which is about 2.1 billions. */
  uint64_t u2 = ((_9p_fid_key_t *)(key->addr))->client_addr ;
  uint64_t u1 = ((((_9p_fid_key_t *)(key->addr))->fid << 1 ) & 0x00000000FFFFFFFFLL)+ ((_9p_fid_key_t *)(key->addr))->trans_type ;
  return (  u2 << 32 | u1 ) ;
}

/**
 * @brief Display a 9p Fid Key
 *
 * @param[in]  key The 9P fid key
 * @param[out] str Output buffer
 *
 * @return Length of display string.
 */
int display_9p_fid_key(struct gsh_buffdesc *buff, char *str)
{
  return sprintf( str, "addr=0x%x==%u.%u.%u.%u,trans=%u,fid=%u",
                        ntohl(  ((_9p_fid_key_t *)(buff->addr))->client_addr ), 
                        ( ntohl(  ( ((_9p_fid_key_t *)(buff->addr))->client_addr ) ) & 0xFF000000 ) >> 24, 
                        ( ntohl(  ( ((_9p_fid_key_t *)(buff->addr))->client_addr ) ) & 0x00FF0000 ) >> 16, 
                        ( ntohl(  ( ((_9p_fid_key_t *)(buff->addr))->client_addr ) ) & 0x0000FF00 ) >>  8, 
                        ntohl(  ( ((_9p_fid_key_t *)(buff->addr))->client_addr ) )   & 0x000000FF, 
                        ((_9p_fid_key_t *)(buff->addr))->trans_type,   
                        ((_9p_fid_key_t *)(buff->addr))->fid)   ;
}

/**
 * @brief Display Fid from hash value
 *
 * @param[in]  buff Buffer pointing to Fid
 * @param[out] str  Output buffer
 *
 * @return Length of display string.
 */

int display_9p_fid_val(struct gsh_buffdesc *buff, char *str)
{
   return sprintf( str, "fid=%u,pexport=%p,pentry=%p,name=%s", 
                         ((_9p_fid_t *)(buff->addr))->fid, 
                         ((_9p_fid_t *)(buff->addr))->pexport, 
                         ((_9p_fid_t *)(buff->addr))->name) ;
}


