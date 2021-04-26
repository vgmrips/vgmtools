
#include "pcm_optimizer.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Copyright (C) 2005 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
details. You should have received a copy of the GNU Lesser General Public
License along with this module; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */

/* Basic Algorithm
1) Log PCM writes to array:

	ABCDABCDEFGHEFGHIJ

2) Find and add shared portions to common block and note offset for original writes:

	ABCDABCDEFGHEFGHIJ
	0123012345674567--  ->  ABCDEFGH

3) Optionally add long runs of unshared writes to common block (more efficient):

	ABCDABCDEFGHEFGHIJ
	012301234567456789  ->  ABCDEFGHIJ

4) Write shared block at beginning of commands, then convert original PCM write commands
into seeks and uses of shared block. */

// Minimum number of PCM bytes in a shared run. Can be larger or smaller.
int const min_match = 24;

// error codes
int const verify_failed = -1;
int const out_of_memory = 0;

typedef unsigned char byte;

// General VGM handling

struct header_t
{
	byte tag [4];
	byte eof_offset [4];
	byte version [4];
	byte psg_rate [4];
	byte ym2413_rate [4];
	byte gd3_offset [4];
	byte sample_count [4];
	byte loop_offset [4];
	byte loop_duration [4];
	byte frame_rate [4];
	byte noise_feedback [2];
	byte noise_width;
	byte unused1;
	byte ym2612_rate [4];
	byte ym2151_rate [4];
	byte data_offset [4];
	byte unused2 [8];
};

enum {
	cmd_gg_stereo       = 0x4F,
	cmd_psg             = 0x50,
	cmd_ym2513          = 0x51,
	cmd_ym2612_port0    = 0x52,
	cmd_ym2612_port1    = 0x53,
	cmd_ym2151          = 0x54,
	cmd_delay           = 0x61,
	cmd_delay_735       = 0x62,
	cmd_delay_882       = 0x63,
	cmd_byte_delay      = 0x64,
	cmd_end             = 0x66,
	cmd_data_block      = 0x67,
	cmd_short_delay     = 0x70,
	cmd_pcm_delay       = 0x80,
	cmd_pcm_seek        = 0xE0,
	
	pcm_block_type      = 0x00,
	ym2612_dac_port     = 0x2A
};

static unsigned long get_le32( void const* p )
{
	return  ((byte*) p) [3] * 0x01000000ul +
			((byte*) p) [2] * 0x00010000ul +
			((byte*) p) [1] * 0x00000100ul +
			((byte*) p) [0];
}

static void set_le32( void* p, unsigned long n )
{
	((byte*) p) [3] = n >> 24;
	((byte*) p) [2] = n >> 16;
	((byte*) p) [1] = n >> 8;
	((byte*) p) [0] = n;
}

// false if command is unrecognized 
static int check_command( int cmd )
{
	switch ( cmd )
	{
		case cmd_gg_stereo:
		case cmd_psg:
		case cmd_ym2513:
		case cmd_ym2612_port0:
		case cmd_ym2612_port1:
		case cmd_ym2151:
		case cmd_delay:
		case cmd_delay_735:
		case cmd_delay_882:
		case cmd_byte_delay:
		case cmd_end:
		case cmd_data_block:
		case cmd_pcm_seek:
			return true;
	}
	
	if ( (cmd & 0xf0) == cmd_short_delay )
		return true;
	
	if ( (cmd & 0xf0) == cmd_pcm_delay )
		return true;
	
	return false;
}

// Get number of bytes used by command
static int command_len( byte const* in )
{
	int cmd = in [0];
	
	#ifndef NDEBUG
		if ( !check_command( cmd ) )
			printf( "Undefined command 0x%02X\n", cmd );
	#endif
	
	// special cases
	switch ( cmd )
	{
		case cmd_psg:
		case cmd_byte_delay:
			return 2;
		
		case cmd_delay:
			return 3;
		
		case cmd_data_block:
			return 7 + get_le32( in + 3 );
	}
	
	// ranges
	switch ( cmd >> 4 )
	{
		case 0x03:
		case 0x04:
			return 2;
		
		case 0x05:
		case 0x0A:
		case 0x0B:
			return 3;
		
		case 0x0C:
		case 0x0D:
			return 4;
		
		case 0x0E:
		case 0x0F:
			return 5;
	}
	
	// all other commands are assumed to have no operands
	return 1;
}

// Shared PCM handling

static int count_matching( long const* offsets, byte const* in_begin, long block_begin,
		long begin, int limit )
{
	int count = 0;
	while ( count < limit )
	{
		if ( offsets [begin + count] >= 0 )
			break;
		
		if ( in_begin [begin + count] != in_begin [block_begin + count] )
			break;
		
		count++;
	}
	return count;
}

static long find_shared( byte const* in_begin, long in_size, long const* offsets, long block_begin )
{
	// find end of unshared run
	long block_end = block_begin;
	while ( block_end < in_size && offsets [block_end] < 0 )
		block_end++;
	if ( block_end - block_begin < min_match )
		return block_end - block_begin;
	
	// find last copy of beginning of first 'min_match' bytes of block
	int first_byte = in_begin [block_begin];
	long begin = in_size - min_match;
	while ( begin > block_begin )
	{
		if ( in_begin [begin] == first_byte &&
				0 == memcmp( in_begin + begin, in_begin + block_begin, min_match ) &&
				count_matching( offsets, in_begin, block_begin, begin, min_match ) == min_match )
			break;
		
		begin--;
	}
	if ( begin < block_begin + min_match )
		return min_match - 1;
	
	// search backwards for all matches, keeping track of the smallest
	long match_size = in_size - begin;
	while ( begin > block_begin )
	{
		if ( in_begin [begin] != first_byte )
		{
			begin--;
			continue;
		}
		
		long limit = begin - block_begin;
		if ( limit > match_size )
			limit = match_size;
		
		long count = count_matching( offsets, in_begin, block_begin, begin, limit );
		if ( count < min_match )
		{
			begin--;
		}
		else
		{
			match_size = count;
			begin -= count;
		}
	}
	
	return match_size;
}

static long find_samples( byte* in_begin, long in_size, long* offsets, byte* shared )
{
	long shared_size = 0;
	
	// find and add shared data
	long block_begin = 0;
	while ( block_begin < in_size )
	{
		// skip to next unshared pcm
		while ( block_begin < in_size && offsets [block_begin] >= 0 )
			block_begin++;
		
		long const match_size = find_shared( in_begin, in_size, offsets, block_begin );
		if ( match_size >= min_match )
		{
			memcpy( shared + shared_size, in_begin + block_begin, match_size );
			
			int use_count = 0;
			int first_byte = in_begin [block_begin];
			long pos = block_begin;
			while ( pos <= in_size - match_size )
			{
				if ( in_begin [pos] != first_byte ||
						0 != memcmp( in_begin + pos, shared + shared_size, match_size ) )
				{
					pos++;
					continue;
				}
				
				long i = 0;
				while ( i < match_size && offsets [pos + i] < 0 )
					i++;
				
				if ( i == match_size )
				{
					for ( i = 0; i < match_size; i++ )
						offsets [pos + i] = shared_size + i;
					
					// clear data to reduce false hits (optimization only)
					memset( in_begin + pos, 0xFE, match_size );
					use_count++;
				}
				
				pos += match_size;
			}
			//printf( "shared offset: %ld, size: %ld, use count: %d\n",
			//      block_begin, match_size, use_count );
			assert( use_count > 1 );
			
			shared_size += match_size;
		}
		block_begin += match_size;
	}
	return shared_size;
}

static long add_unshared( byte* in_begin, long in_size, long* offsets, byte* shared,
		long shared_size )
{
	long begin = 0;
	long scan_end = in_size - min_match;
	while ( begin < scan_end )
	{
		if ( offsets [begin] >= 0 )
		{
			begin++;
			continue;
		}
		
		long end = begin;
		while ( end < scan_end && offsets [end] < 0 )
			end++;
		
		if ( end - begin >= min_match )
		{
			while ( begin < end )
			{
				offsets [begin] = shared_size;
				shared [shared_size++] = in_begin [begin++];
			}
		}
		begin = end;
	}
	return shared_size;
}

static byte* add_pcm_refs( byte const* in, byte const* in_loop, long const* offsets,
		byte const* samples, long samples_size, byte* out, byte** out_loop )
{
	long pcm_offset = -1;
	while ( *in != cmd_end )
	{
		// write pcm data after any data blocks, before any other commands
		if ( samples_size && *in != cmd_data_block )
		{
			*out++ = cmd_data_block;
			*out++ = cmd_end;
			*out++ = pcm_block_type;
			set_le32( out, samples_size );
			out += 4;
			memcpy( out, samples, samples_size );
			out += samples_size;
			samples_size = 0;
		}
		
		if ( in == in_loop )
		{
			*out_loop = out;
			pcm_offset = -1; // force re-seek into pcm data
		}
		
		if ( *in == cmd_ym2612_port0 && in [1] == ym2612_dac_port && *offsets++ >= 0 )
		{
			long offset = offsets [-1];
			if ( pcm_offset != offset )
			{
				// need to seek
				pcm_offset = offset;
				*out++ = cmd_pcm_seek;
				set_le32( out, offset );
				out += 4;
			}
			
			*out++ = cmd_pcm_delay;
			in += 3;
			pcm_offset++;
			
			// optimize if next command is short delay
			if ( *in == cmd_delay && in [2] == 0 && in [1] < 0x10 )
			{
				out [-1] = cmd_pcm_delay + in [1];
				in += 3;
			}
			else if ( (*in & 0xf0) == cmd_short_delay && (*in & 0x0f) < 0x0f )
			{
				out [-1] = cmd_pcm_delay + (*in & 0x0f) + 1;
				in++;
			}
		}
		// optimize delay
		else if ( *in == cmd_delay && in [2] == 0 && in [1] <= 0x10 && in [1] )
		{
			*out++ = cmd_short_delay | (in [1] - 1);
			in += 3;
		}
		else
		{
			for ( int n = command_len( in ); n--; )
				*out++ = *in++;
		}
	}
	*out++ = cmd_end;
	
	return out;
}

// VGM file conversion

static long copy_header( byte const* in, byte* out, byte const** loop_begin )
{
	memcpy( out, in, sizeof (header_t) );
	header_t const* h = (header_t*) out;
	
	long loop_offset = get_le32( h->loop_offset );
	if ( loop_offset )
		*loop_begin = in + loop_offset + offsetof (header_t,loop_offset);
	
	long header_size = sizeof (header_t);
	if ( get_le32( h->version ) >= 0x150 )
	{
		header_size = get_le32( h->data_offset ) + offsetof (header_t,data_offset);
		assert( header_size >= sizeof (header_t) );
		memcpy( out, in, header_size );
	}
	
	return header_size;
}   

static long copy_trailer( byte const* in_begin, byte const* in, long in_size,
		byte* out_begin, byte* out, byte* out_loop )
{
	if ( out_loop )
		set_le32( ((header_t*) out_begin)->loop_offset,
				out_loop - out_begin - offsetof (header_t,loop_offset) );
	
	// copy any trailing data (gd3 tag)
	long remain = in_size - (in - in_begin);
	if ( (unsigned) remain > 0x200 )
		printf( "Lots of data past end of commands: %ld\n", remain );
	memcpy( out, in, remain );
	out += remain;
	
	// adjust offsets in header
	long new_size = out - out_begin;
	long offset = in_size - new_size;
	header_t* h = (header_t*) out_begin;
	set_le32( h->eof_offset, get_le32( h->eof_offset ) - offset );
	set_le32( h->gd3_offset, get_le32( h->gd3_offset ) - offset );
	
	return new_size;
}

static long unoptimize_pcm_data( byte const* in_begin, long in_size, byte* out_begin,
		int delays_optimized )
{
	byte const* loop_begin = NULL;
	long header_size = copy_header( in_begin, out_begin, &loop_begin );
	
	byte* out_loop = NULL;
	byte const* in = in_begin + header_size;
	byte* out = out_begin + header_size;
	
	byte const* samples = NULL;
	long samples_size = 0;
	long samples_pos = -1;
	long pcm_end = 0;
	while ( *in != cmd_end )
	{
		if ( in - in_begin >= in_size )
			return verify_failed;
		
		// recalculate loop
		if ( in == loop_begin )
		{
			out_loop = out;
			samples_pos = -1;
		}
		
		// sample data
		if ( *in == cmd_data_block && in [2] == pcm_block_type )
		{
			if ( in [1] != cmd_end )
				return verify_failed;
			if ( samples )
				return verify_failed;
			samples_size = get_le32( in + 3 );
			in += 7;
			samples = in;
			in += samples_size;
		}
		// seek
		else if ( *in == cmd_pcm_seek )
		{
			samples_pos = get_le32( in + 1 );
			in += 5;
		}
		// pcm delay
		else if ( (*in & 0xf0) == cmd_pcm_delay )
		{
			if ( (unsigned long) samples_pos >= samples_size )
				return verify_failed;
			
			*out++ = cmd_ym2612_port0;
			*out++ = ym2612_dac_port;
			*out++ = samples [samples_pos++];
			if ( pcm_end < samples_pos )
				pcm_end = samples_pos;
			
			int delay = *in++ & 0x0f;
			if ( delay > 0 )
			{
				*out++ = cmd_short_delay + delay - 1;
				if ( !delays_optimized )
				{
					out [-1] = cmd_delay;
					*out++ = delay;
					*out++ = 0;
				}
			}
		}
		// short delay
		else if ( (*in & 0xf0) == cmd_short_delay && !delays_optimized )
		{
			*out++ = cmd_delay;
			*out++ = (*in++ & 0x0f) + 1;
			*out++ = 0;
		}
		else
		{
			for ( int n = command_len( in ); n--; )
				*out++ = *in++;
		}
	}
	*out++ = *in++;
	
	return copy_trailer( in_begin, in, in_size, out_begin, out, out_loop );
}

long optimize_pcm_data( void const* in_beginv, long in_size, void* out_beginv, int flags )
{
	byte const* const in_begin = (byte*) in_beginv;
	if ( memcmp( in_begin, "Vgm ", 4 ) )
		return -1;
	if ( get_le32( ((header_t*) in_begin)->version ) < 0x150 )
		return -1; // header must be updated to 1.50 first
	
	byte* const out_begin = (byte*) out_beginv;
	byte const* loop_begin = NULL;
	long header_size = copy_header( in_begin, out_begin, &loop_begin );
	
	// extract all pcm writes
	byte* raw_pcm = (byte*) malloc( in_size / 3 );
	if ( !raw_pcm )
		return out_of_memory;
	long raw_pcm_size = 0;
	int delays_optimized = false;
	byte const* in = in_begin + header_size;
	while ( *in != cmd_end )
	{
		if ( (in [0] == cmd_data_block && in [2] == pcm_block_type) ||
				in [0] == cmd_pcm_seek )
		{
			free( raw_pcm );
			memcpy( out_begin, in_begin, in_size );
			return in_size;
		}
		
		if ( in [0] == cmd_ym2612_port0 && in [1] == ym2612_dac_port )
			raw_pcm [raw_pcm_size++] = in [2];
		
		if ( (in [0] & 0xf0) == cmd_short_delay )
			delays_optimized = true;
		
		if ( !check_command( *in ) )
			return -1; // unrecognized command
		
		in += command_len( in );
	}
	in++;
	
	// determine shared samples
	byte* samples = (byte*) malloc( raw_pcm_size + 1 );
	long* pcm_offsets = (long*) malloc( raw_pcm_size * sizeof (long) + 1 );
	if ( !pcm_offsets || !samples )
	{
		free( pcm_offsets );
		free( samples );
		free( raw_pcm );
		return out_of_memory;
	}
	for ( long i = 0; i < raw_pcm_size; i++ )
		pcm_offsets [i] = -1;
	long samples_size = find_samples( raw_pcm, raw_pcm_size, pcm_offsets, samples );
	
	if ( !(flags & optimize_shared_only) )
		samples_size = add_unshared( raw_pcm, raw_pcm_size, pcm_offsets, samples, samples_size );
	
	//printf( "samples_size: %ld\n", (long) samples_size );
	
	// convert pcm writes into references to shared data
	byte* out_loop = NULL;
	byte* out = add_pcm_refs( in_begin + header_size, loop_begin, pcm_offsets,
			samples, samples_size, out_begin + header_size, &out_loop );
	
	free( pcm_offsets );
	free( samples );
	free( raw_pcm );
	
	long out_size = copy_trailer( in_begin, in, in_size, out_begin, out, out_loop );
	
	// optionally verify
	if ( !(flags & skip_verification) )
	{
		byte* verify = (byte*) malloc( in_size * 4 / 3 );
		if ( !verify )
			return out_of_memory;
		long result = unoptimize_pcm_data( out_begin, out_size, verify, delays_optimized );
		if ( result >= 0 && (result != in_size || memcmp( verify, in_begin, in_size )) )
			result = -1;
		free( verify );
		if ( result < 0 )
			return result;
	}
	
	return out_size;
}

const char* update_vgm_header( void* header )
{
	header_t* h = (header_t*) header;
	if ( memcmp( h->tag, "Vgm ", 4 ) )
		return "Not a VGM file";
	
	long version = get_le32( h->version );
	if ( version >= 0x200 )
		return "VGM file has newer format than supported";
	
	if ( version < 0x110 )
	{
		h->noise_feedback [0] = 9;
		h->noise_feedback [1] = 0;
		h->noise_width = 16;
		set_le32( h->ym2612_rate, get_le32( h->ym2413_rate ) );
		set_le32( h->ym2413_rate, 0 );
	}
	if ( version < 0x150 )
	{
		set_le32( h->data_offset, sizeof (header_t) - offsetof (header_t,data_offset) );
	}
	set_le32( h->version, 0x150 );
	
	return NULL;
}

