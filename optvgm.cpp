
// Optimizes Mega Drive/Sega Genesis VGMs. Always gzip compresses output.
//
// optvgm [-s] file.vgm/vgz [out.vgz]
//
// Specifying only input path optimizes in place. Optimized data is always verified
// *before* being written.
//
// By default, unshared samples are also added to PCM data, reducing file size slightly
// while slightly increasing memory usage in a player. Specify -s to add shared data only.

#include "pcm_optimizer.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <zlib.h>

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

static void throw_error( const char* str )
{
	if ( str )
	{
		fprintf( stderr, "Error: %s\n", str );
		exit( EXIT_FAILURE );
	}
}

// gzip lameness

static const char* get_eof( FILE* file, long* eof )
{
	unsigned char buf [4];
	if ( !fread( buf, 2, 1, file ) )
		return "Couldn't read from file";
	
	if ( buf [0] == 0x1F && buf [1] == 0x8B )
	{
		if ( fseek( file, -4, SEEK_END ) )
			return "Couldn't seek in file";
		
		if ( !fread( buf, 4, 1, file ) )
			return "Couldn't read from file";
		
		*eof = buf [3] * 0x1000000L + buf [2] * 0x10000L + buf [1] * 0x100L + buf [0];
	}
	else
	{
		if ( fseek( file, 0, SEEK_END ) )
			return "Couldn't seek in file";
		
		*eof = ftell( file );
	}
	
	return NULL;
}

static const char* get_eof( const char* path, long* eof )
{
	FILE* file = fopen( path, "rb" );
	if ( !file )
		return "Couldn't open file";
	const char* error = get_eof( file, eof );
	fclose( file );
	return error;
}

int main( int argc, char** argv )
{
	// supply defaults
	if ( !argc || !*argv )
	{
		static char* args [] = { "optvgm", "in.vgz", "out.vgz", NULL };
		argc = sizeof args / sizeof *args - 1;
		argv = args;
	}
	
	int opt_flags = 0;
	
	// parse options
	int arg = 1;
	while ( arg < argc && argv [arg] [0] == '-' )
	{
		switch ( toupper( argv [arg++] [1] ) )
		{
			case 'S':
				opt_flags |= optimize_shared_only;
				break;
			
			default:
				printf( "Invalid option\n" );
				arg = argc; // cause help to be printed
				break;
		}
	}
	
	// print help
	if ( arg >= argc )
	{
		printf( "%s [-s] file.vgm/vgz [out.vgz] # Optimize Mega Drive/Sega Genesis VGM/VGZ\n",
				argv [0] );
		printf( "-s # Optimized shared samples only\n" );
		return 0;
	}
	
	// allocate memory
	long in_size = 0;
	throw_error( get_eof( argv [arg], &in_size ) );
	char* in_data = (char*) malloc( in_size );
	char* out_data = (char*) malloc( in_size + pcm_optimizer_extra );
	if ( !in_data || !out_data )
	{
		free( in_data );
		free( out_data );
		throw_error( "Out of memory" );
	}
	
	// read input
	gzFile in_file = gzopen( argv [arg], "rb" );
	if ( !in_file )
		throw_error( "Couldn't open input file" );
	if ( gzread( in_file, in_data, in_size ) < in_size )
		throw_error( "Error reading input" );
	gzclose( in_file );
	if ( 0 != memcmp( in_data, "Vgm ", 4 ) )
		throw_error( "Not a VGM/VGZ file\n" );
	
	// check and update header
	throw_error( update_vgm_header( in_data ) );
	
	// optimize
	long out_size = optimize_pcm_data( in_data, in_size, out_data, opt_flags );
	if ( !out_size )
		throw_error( "Out of memory" );
	if ( out_size < 0 )
		throw_error( "Verify failed" );
	if ( out_size == in_size )
		printf( "No optimization achieved.\n" );
	free( in_data );
	
	// write output
	if ( arg + 1 < argc )
		arg++;
	gzFile out_file = gzopen( argv [arg], "wb9" ); // max compression
	if ( !out_file )
		throw_error( "Couldn't open output file" );
	if ( gzwrite( out_file, out_data, out_size ) < out_size )
		throw_error( "Error writing output" );
	gzclose( out_file );
	free( out_data );
	
	return 0;
}

