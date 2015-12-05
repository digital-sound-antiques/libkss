#ifndef _MMAP_H_
#define _MMAP_H_
#include "../ksstypes.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { BANK_16K=0, BANK_8K=1 } ;
enum { BANK_READABLE=1, BANK_WRITEABLE=2 } ;

typedef struct tagBANK
{
  k_uint32 refc ; /* Reference counter */
  k_uint32 type ; /* KSS_8/16K_BANK */
  k_uint32 attr ; 
  k_uint8 *body ;
} BANK ;

typedef struct tagMMAP
{
  BANK *bank[16][256] ;

  k_uint8 *readmap[8] ;  /* 0x2000 * 8 Bank = 64K */
  k_uint8 *writemap[8] ; /* 0x2000 * 8 Bank = 64K */

  k_uint32 current_slot[8] ; /* Current slot number */
  k_uint32 current_bank[8] ; /* Current bank number */

} MMAP ;

/* Create a MMAP object */
MMAP *MMAP_new() ;

/* Delete a MMAP object */
void MMAP_delete(MMAP *mmap) ;

/* Set a bank data to a bank.
  slot : slot number (0 to 15)
  bank : bank number (0 to 255)
  type : KSS_16K_BANK or KSS_8K_BANK
  data : Pointer to bank data(16K or 8K)
*/
void MMAP_set_bank_data(MMAP *mmap, k_uint32 slot, k_uint32 bank, k_uint32 type, k_uint8 *data) ;

/* Unset a bank.
  slot : slot number (0 to 15)
  bank : bank number (0 to 255)
*/
void MMAP_unset_bank(MMAP *mmap, k_uint32 slot, k_uint32 bank) ;

/* Set the attribute of a bank assigned with a page
  page : page number (0 to 8)
  attr : attribute 
         0                            : Both read and write disable.
         BANK_WRITEABLE               : write only
         BANK_READABLE                : read only
         BANK_WRITEABLE|BANK_READABLE : Both read and write enable.
*/
void MMAP_set_page_attr(MMAP *mmap, k_uint32 page, k_uint32 attr) ;

/* Set the attribute of a bank
   slot : slot number (0 to 15)
   bank : bank number (0 to 255)
   attr : attribute (see above)
*/
void MMAP_set_bank_attr(MMAP *mmap, k_uint32 slot, k_uint32 bank, k_uint32 attr) ;

/* Mirror a bank to an another bank
   src_slot : slot number of the source bank (0 to 15)
   src_bank : bank number of the source bank (0 to 255)
   dst_slot : slot number of the bank refers the source bank (0 to 15)
   dst_bank : slot number of the bank refers the source bank (0 to 255)
*/
void MMAP_mirror_bank(MMAP *mmap, k_uint32 src_slot, k_uint32 src_bank, k_uint32 dst_slot, k_uint32 dst_bank) ;

/* Select the bank number of a page.
   page : page number (0 to 7). Bit1 is ignored if the bank type is BANK_16K
   slot : slot number of the bank
   bank : bank number of the bank 
*/
void MMAP_select_bank(MMAP *mmap, k_uint32 page, k_uint32 slot, k_uint32 bank) ;

/* Read 1byte from the memory (64K linered).
   adr : address (0 to 0xffff) 
*/
k_uint32 MMAP_read_memory(MMAP *mmap, k_uint32 adr) ;

/* Write 1byte to the memory (64K linered).
   adr : address (0 to 0xffff) 
   val : value (0 to 0xff)
*/
void MMAP_write_memory(MMAP *mmap, k_uint32 adr, k_uint32 val) ;

#ifdef __cplusplus
}
#endif

#endif