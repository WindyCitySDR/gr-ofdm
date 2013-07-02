#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gr_block.h>
#include <gr_io_signature.h>
#include "ofdm_channel_equalizer_mimo.h"

#include <iostream>
#include <algorithm>

#include <malloc16.h>

#define DEBUG 0


ofdm_channel_equalizer_mimo ::
ofdm_channel_equalizer_mimo( int vlen )
  : gr_block(
      "channel_equalizer_mimo",
      gr_make_io_signature4(
        4, 4,
        sizeof( gr_complex ) * vlen,	// ofdm_blocks
		sizeof( gr_complex ) * vlen/2,	// h0
		sizeof( gr_complex ) * vlen/2,	// h1
		sizeof( char ) ),				// frame_start
      gr_make_io_signature(
        1, 1,
        sizeof( gr_complex ) * vlen ) )	// combined and equalized output signal

  , d_vlen( vlen )
  , d_need_input_h0( 1 )
  , d_need_input_h1( 1 )

  , d_buffer_h0( static_cast< gr_complex * >( malloc16Align( sizeof( gr_complex ) * vlen ) ) )
  , d_buffer_h1( static_cast< gr_complex * >( malloc16Align( sizeof( gr_complex ) * vlen ) ) )

{
  assert( ( vlen % 2 ) == 0 ); // alignment 16 byte
}

ofdm_channel_equalizer_mimo ::
~ofdm_channel_equalizer_mimo()
{
  if( d_buffer_h0 )
    free16Align( d_buffer_h0 );
  if( d_buffer_h1 )
    free16Align( d_buffer_h1 );
}


void
ofdm_channel_equalizer_mimo ::
forecast(
  int             noutput_items,
  gr_vector_int & ninput_items_required )
{
  ninput_items_required[0] = ninput_items_required[3] = noutput_items;
  ninput_items_required[1] = d_need_input_h0;
  ninput_items_required[2] = d_need_input_h1;
}

int
ofdm_channel_equalizer_mimo ::
general_work(
  int                         noutput_items,
  gr_vector_int             & ninput_items,
  gr_vector_const_void_star & input_items,
  gr_vector_void_star       & output_items )
{
  gr_complex const * ofdm_blocks = static_cast< gr_complex const * >( input_items[0] );

  gr_complex const * ctf_0 = static_cast< gr_complex const * >( input_items[1] );
  gr_complex const * ctf_1 = static_cast< gr_complex const * >( input_items[2] );

  char const * frame_start = static_cast< char const * >( input_items[3] );

  gr_complex * out = static_cast< gr_complex * >( output_items[0] );

  int const nin = std::min( ninput_items[0], 
				  std::min( ninput_items[3], noutput_items ) ) ;
  int n_ctf_0 = ninput_items[1];
  int n_ctf_1 = ninput_items[2];

  if( DEBUG )
    std::cout << "EQ: nin=" << nin << " nin0=" << ninput_items[0]
              << " nin2=" << ninput_items[3]
              << " n_ctf_0=" << ninput_items[1] << " n_ctf_1=" << ninput_items[2]
              << " nout=" << noutput_items
              << std::endl;

  int i = 0;

//  for( ; i < nin; ++i, ofdm_blocks += d_vlen, out += d_vlen )
  for( ; i < nin; ++i, ofdm_blocks += d_vlen, out += d_vlen )
  {
	gr_complex norm = 0.5;
	gr_complex norm_c = 1 / sqrt(2);

    // if not first frame start, advance ctf_0 ptr if enough input available
    if( frame_start[i] != 0 )
    {
      if( n_ctf_0 == 0 )
      {
        d_need_input_h0 = 1;
        if( DEBUG )
            std::cout << "d_need_input_h0" << std::endl;
        break;
      }
	  if( n_ctf_1 == 0 )
      {
        d_need_input_h1 = 1;
        if( DEBUG )
        	std::cout << "d_need_input_h1" << std::endl;
        break;
      }

      std::copy( ctf_0, ctf_0 + d_vlen/2, d_buffer_h0 );
	  std::copy( ctf_1, ctf_1 + d_vlen/2, d_buffer_h1 );

      --n_ctf_0;
      ctf_0 += d_vlen/2;
	  --n_ctf_1;
      ctf_1 += d_vlen/2;

      d_need_input_h0 = 0;
	  d_need_input_h1 = 0;
    }
    {

     // Equalization
     for( int k = 0; k < d_vlen; k++ )
	 {
    	 int l = k / 2;

    	 if ( frame_start[i] == 1 )
    	 {
    		 if ( k % 2 == 0 )
    		 {
    			 //out[k] = ofdm_blocks[k] *  d_buffer_h0[l] * norm;
    			 out[k] = ofdm_blocks[k] * norm;
    		 } else {
    			 //out[k] = ofdm_blocks[k] * d_buffer_h1[l] * norm;
    			 out[k] = ofdm_blocks[k] * norm;
    		 }

    	// if pilot
    	 } else if ( k == 24 || k == 44 || k == 64 || k == 84 || k == 124 || k == 144 || k == 164 || k == 184 )
    	 {
    		 out[k] = ofdm_blocks[k] * d_buffer_h0[l] * norm;

    	// if pair part of pilot
    	 } else if ( k == 25 || k == 45 || k == 65 || k == 85 || k == 125 || k == 145 || k == 165 || k == 185 )
    	 {
    		 out[k] = ( - std::conj( ofdm_blocks[k] ) * std::conj( d_buffer_h0[l] ) ) * norm_c;

    	 } else {
    		 if ( k % 2 == 0 )
    		 {
    			 out[k] = ( ofdm_blocks[k] * d_buffer_h0[l]  + std::conj( ofdm_blocks[k+1] ) * std::conj( d_buffer_h1[l] ) ) * norm;
    		 } else {
    			 out[k] = ( ofdm_blocks[k-1] * d_buffer_h1[l] - std::conj( ofdm_blocks[k] ) * std::conj( d_buffer_h0[l] ) ) * norm;
    		 }
    	 }
	 }
    }

  } // for-loop over input

  if( i > 0 )
  {
	consume( 0, i );
    consume( 3, i );
  }

  int const tmp_0 = ninput_items[1] - n_ctf_0;
  if( tmp_0 > 0 )
    consume( 1, tmp_0 );
  int const tmp_1 = ninput_items[2] - n_ctf_1;
  if( tmp_1 > 0 )
    consume( 2, tmp_1 );
	
  if( DEBUG )
    std::cout << "EQ: leave, consume i=" << i << " and "
              << (ninput_items[1]-n_ctf_0) << ", produce " << i
              << (ninput_items[2]-n_ctf_1) << ", produce " << i
              << std::endl;

  return i;

} // general_work


ofdm_channel_equalizer_mimo_sptr
ofdm_make_channel_equalizer_mimo( int vlen)
{
  return ofdm_channel_equalizer_mimo::create( vlen);
}


ofdm_channel_equalizer_mimo_sptr
ofdm_channel_equalizer_mimo ::
create( int vlen)
{
  try
  {
    ofdm_channel_equalizer_mimo_sptr tmp(
      new ofdm_channel_equalizer_mimo( vlen) );

    return tmp;
  }
  catch ( ... )
  {
    std::cerr << "[ERROR] Caught exception at creation of "
              << "ofdm_channel_equalizer_mimo" << std::endl;
    throw;
  } // catch ( ... )
}