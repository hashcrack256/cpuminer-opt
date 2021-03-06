#include "skunk-gate.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "algo/skein/skein-hash-4way.h"
#include "algo/gost/sph_gost.h"
#include "algo/fugue/sph_fugue.h"
#include "algo/cubehash/cubehash_sse2.h"
#include "algo/cubehash/cube-hash-2way.h"

#if defined(SKUNK_8WAY)

typedef struct {
    skein512_8way_context skein;
    cube_4way_context     cube;
    sph_fugue512_context  fugue;
    sph_gost512_context   gost;
} skunk_8way_ctx_holder;

static __thread skunk_8way_ctx_holder skunk_8way_ctx;

void skunk_8way_hash( void *output, const void *input )
{
     uint64_t vhash[8*8] __attribute__ ((aligned (128)));
     uint64_t hash0[8] __attribute__ ((aligned (64)));
     uint64_t hash1[8] __attribute__ ((aligned (64)));
     uint64_t hash2[8] __attribute__ ((aligned (64)));
     uint64_t hash3[8] __attribute__ ((aligned (64)));
     uint64_t hash4[8] __attribute__ ((aligned (64)));
     uint64_t hash5[8] __attribute__ ((aligned (64)));
     uint64_t hash6[8] __attribute__ ((aligned (64)));
     uint64_t hash7[8] __attribute__ ((aligned (64)));

     skunk_8way_ctx_holder ctx __attribute__ ((aligned (64)));
     memcpy( &ctx, &skunk_8way_ctx, sizeof(skunk_8way_ctx) );

     skein512_8way_update( &ctx.skein, input, 80 );
     skein512_8way_close( &ctx.skein, vhash );
     dintrlv_8x64( hash0, hash1, hash2, hash3, hash4, hash5, hash6,
                        hash7, vhash, 512 );
  
     intrlv_4x128_512( vhash, hash0, hash1, hash2, hash3 ); 
     cube_4way_update_close( &ctx.cube, vhash, vhash, 64 ); 
     dintrlv_4x128_512( hash0, hash1, hash2, hash3, vhash );
     intrlv_4x128_512( vhash, hash4, hash5, hash6, hash7 ); 
     cube_4way_init( &ctx.cube, 512, 16, 32 );           
     cube_4way_update_close( &ctx.cube, vhash, vhash, 64 );  
     dintrlv_4x128_512( hash4, hash5, hash6, hash7, vhash );
     
     sph_fugue512( &ctx.fugue, hash0, 64 );
     sph_fugue512_close( &ctx.fugue, hash0 );
     sph_fugue512_init( &ctx.fugue );
     sph_fugue512( &ctx.fugue, hash1, 64 );
     sph_fugue512_close( &ctx.fugue, hash1 );
     sph_fugue512_init( &ctx.fugue );
     sph_fugue512( &ctx.fugue, hash2, 64 );
     sph_fugue512_close( &ctx.fugue, hash2 );
     sph_fugue512_init( &ctx.fugue );
     sph_fugue512( &ctx.fugue, hash3, 64 );
     sph_fugue512_close( &ctx.fugue, hash3 );
     sph_fugue512( &ctx.fugue, hash4, 64 );
     sph_fugue512_close( &ctx.fugue, hash4 );
     sph_fugue512_init( &ctx.fugue );
     sph_fugue512( &ctx.fugue, hash5, 64 );
     sph_fugue512_close( &ctx.fugue, hash5 );
     sph_fugue512_init( &ctx.fugue );
     sph_fugue512( &ctx.fugue, hash6, 64 );
     sph_fugue512_close( &ctx.fugue, hash6 );
     sph_fugue512_init( &ctx.fugue );
     sph_fugue512( &ctx.fugue, hash7, 64 );
     sph_fugue512_close( &ctx.fugue, hash7 );

     sph_gost512( &ctx.gost, hash0, 64 );
     sph_gost512_close( &ctx.gost, output );
     sph_gost512_init( &ctx.gost );
     sph_gost512( &ctx.gost, hash1, 64 );
     sph_gost512_close( &ctx.gost, output+ 32 );
     sph_gost512_init( &ctx.gost );
     sph_gost512( &ctx.gost, hash2, 64 );
     sph_gost512_close( &ctx.gost, output+ 64 );
     sph_gost512_init( &ctx.gost );
     sph_gost512( &ctx.gost, hash3, 64 );
     sph_gost512_close( &ctx.gost, output+ 96 );
     sph_gost512_init( &ctx.gost );
     sph_gost512( &ctx.gost, hash4, 64 );
     sph_gost512_close( &ctx.gost, output+128 );
     sph_gost512_init( &ctx.gost );
     sph_gost512( &ctx.gost, hash5, 64 );
     sph_gost512_close( &ctx.gost, output+160 );
     sph_gost512_init( &ctx.gost );
     sph_gost512( &ctx.gost, hash6, 64 );
     sph_gost512_close( &ctx.gost, output+192 );
     sph_gost512_init( &ctx.gost );
     sph_gost512( &ctx.gost, hash7, 64 );
     sph_gost512_close( &ctx.gost, output+224 );
}

int scanhash_skunk_8way( struct work *work, uint32_t max_nonce,
                         uint64_t *hashes_done, struct thr_info *mythr )
{
   uint32_t hash[8*8] __attribute__ ((aligned (128)));
   uint32_t vdata[24*8] __attribute__ ((aligned (64)));
   uint32_t *pdata = work->data;
   uint32_t *ptarget = work->target;
   const uint32_t first_nonce = pdata[19];
   uint32_t n = first_nonce;
   __m512i  *noncev = (__m512i*)vdata + 9;   // aligned
   const uint32_t Htarg = ptarget[7];
   int thr_id = mythr->id;  
   volatile uint8_t *restart = &(work_restart[thr_id].restart);

   if ( opt_benchmark )
      ((uint32_t*)ptarget)[7] = 0x0cff;

   mm512_bswap32_intrlv80_8x64( vdata, pdata );
   do
   {
      *noncev = mm512_intrlv_blend_32( mm512_bswap_32(
              _mm512_set_epi32( n+7, 0, n+6, 0, n+5, 0, n+4, 0,
                                n+3, 0, n+2, 0, n+1, 0, n  , 0 ) ), *noncev );

      skunk_8way_hash( hash, vdata );
      pdata[19] = n;

      for ( int i = 0; i < 8; i++ )
      if ( unlikely( (hash+(i<<3))[7] <= Htarg ) )
      if ( likely( fulltest( hash+(i<<3), ptarget ) && !opt_benchmark ) )
      {
         pdata[19] = n+i;
         submit_lane_solution( work, hash+(i<<3), mythr, i );
      }
      n +=8;
   } while ( likely( ( n < max_nonce-8 ) && !(*restart) ) );

   *hashes_done = n - first_nonce;
   return 0;
}

bool skunk_8way_thread_init()
{
   skein512_8way_init( &skunk_8way_ctx.skein );
   cube_4way_init( &skunk_8way_ctx.cube, 512, 16, 32 );
   sph_fugue512_init( &skunk_8way_ctx.fugue );
   sph_gost512_init( &skunk_8way_ctx.gost );
   return true;
}

#elif defined(SKUNK_4WAY)

typedef struct {
    skein512_4way_context skein;
    cubehashParam         cube;
    sph_fugue512_context  fugue;
    sph_gost512_context   gost;
} skunk_4way_ctx_holder;

static __thread skunk_4way_ctx_holder skunk_4way_ctx;

void skunk_4way_hash( void *output, const void *input )
{
     uint64_t hash0[8] __attribute__ ((aligned (64)));
     uint64_t hash1[8] __attribute__ ((aligned (64)));
     uint64_t hash2[8] __attribute__ ((aligned (64)));
     uint64_t hash3[8] __attribute__ ((aligned (64)));
     uint64_t vhash[8*4] __attribute__ ((aligned (64)));

     skunk_4way_ctx_holder ctx __attribute__ ((aligned (64)));
     memcpy( &ctx, &skunk_4way_ctx, sizeof(skunk_4way_ctx) );

     skein512_4way_update( &ctx.skein, input, 80 );
     skein512_4way_close( &ctx.skein, vhash );
     dintrlv_4x64( hash0, hash1, hash2, hash3, vhash, 512 );

     cubehashUpdateDigest( &ctx.cube, (byte*) hash0, (const byte*)hash0, 64 );
     memcpy( &ctx.cube, &skunk_4way_ctx.cube, sizeof(cubehashParam) );
     cubehashUpdateDigest( &ctx.cube, (byte*)hash1, (const byte*) hash1, 64 );
     memcpy( &ctx.cube, &skunk_4way_ctx.cube, sizeof(cubehashParam) );
     cubehashUpdateDigest( &ctx.cube, (byte*)hash2, (const byte*) hash2, 64 );
     memcpy( &ctx.cube, &skunk_4way_ctx.cube, sizeof(cubehashParam) );
     cubehashUpdateDigest( &ctx.cube, (byte*)hash3, (const byte*) hash3, 64 );

     sph_fugue512( &ctx.fugue, hash0, 64 );
     sph_fugue512_close( &ctx.fugue, hash0 );
     sph_fugue512_init( &ctx.fugue );
     sph_fugue512( &ctx.fugue, hash1, 64 );
     sph_fugue512_close( &ctx.fugue, hash1 );
     sph_fugue512_init( &ctx.fugue );
     sph_fugue512( &ctx.fugue, hash2, 64 );
     sph_fugue512_close( &ctx.fugue, hash2 );
     sph_fugue512_init( &ctx.fugue );
     sph_fugue512( &ctx.fugue, hash3, 64 );
     sph_fugue512_close( &ctx.fugue, hash3 );

     sph_gost512( &ctx.gost, hash0, 64 );
     sph_gost512_close( &ctx.gost, hash0 );
     sph_gost512_init( &ctx.gost );
     sph_gost512( &ctx.gost, hash1, 64 );
     sph_gost512_close( &ctx.gost, hash1 );
     sph_gost512_init( &ctx.gost );
     sph_gost512( &ctx.gost, hash2, 64 );
     sph_gost512_close( &ctx.gost, hash2 );
     sph_gost512_init( &ctx.gost );
     sph_gost512( &ctx.gost, hash3, 64 );
     sph_gost512_close( &ctx.gost, hash3 );

     memcpy( output,    hash0, 32 );
     memcpy( output+32, hash1, 32 );
     memcpy( output+64, hash2, 32 );
     memcpy( output+96, hash3, 32 );
}

int scanhash_skunk_4way( struct work *work, uint32_t max_nonce,
                    uint64_t *hashes_done, struct thr_info *mythr )
{
   uint32_t hash[4*8] __attribute__ ((aligned (64)));
   uint32_t vdata[24*4] __attribute__ ((aligned (64)));
   uint32_t *pdata = work->data;
   uint32_t *ptarget = work->target;
   const uint32_t first_nonce = pdata[19];
   uint32_t n = first_nonce;
   __m256i  *noncev = (__m256i*)vdata + 9;   // aligned
   const uint32_t Htarg = ptarget[7];
   int thr_id = mythr->id;  // thr_id arg is deprecated
   volatile uint8_t *restart = &(work_restart[thr_id].restart);

   if ( opt_benchmark )
      ((uint32_t*)ptarget)[7] = 0x0cff;

   mm256_bswap32_intrlv80_4x64( vdata, pdata );
   do
   {
      *noncev = mm256_intrlv_blend_32( mm256_bswap_32(
                 _mm256_set_epi32( n+3, 0, n+2, 0, n+1, 0, n, 0 ) ), *noncev );

      skunk_4way_hash( hash, vdata );
      pdata[19] = n;

      for ( int i = 0; i < 4; i++ )
      if ( (hash+(i<<3))[7] <= Htarg )
      if ( fulltest( hash+(i<<3), ptarget ) && !opt_benchmark )
      {
         pdata[19] = n+i;
         submit_lane_solution( work, hash+(i<<3), mythr, i );
      }
      n +=4;
   } while ( ( n < max_nonce ) && !(*restart) );

   *hashes_done = n - first_nonce + 1;
   return 0;
}

bool skunk_4way_thread_init()
{
   skein512_4way_init( &skunk_4way_ctx.skein );
   cubehashInit( &skunk_4way_ctx.cube, 512, 16, 32 );
   sph_fugue512_init( &skunk_4way_ctx.fugue );
   sph_gost512_init( &skunk_4way_ctx.gost );
   return true;
}

#endif
