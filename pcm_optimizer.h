
/* Mega Drive/Sega Genesis VGM optimizer for vastly reducing PCM data size */

#ifndef PCM_OPTIMIZER_H
#define PCM_OPTIMIZER_H

#ifdef __cplusplus
	extern "C" {
#endif

/* Update VGM header to version 1.50. Returns NULL on success, otherwise error string. */
const char* update_vgm_header( void* header );

/* Optimize PCM data in Mega Drive/Sega Genesis VGM. Output buffer must have
   at least 'in_size + pcm_optimizer_extra' bytes allocated. Returns number of
   bytes written to output buffer, 0 if out of memory, or a negative value if
   VGM file had bad command or verification failed. */
enum { pcm_optimizer_extra = 1024 };
enum { optimize_shared_only = 1 << 0 }; // don't add unshared samples to data block
enum { skip_verification    = 1 << 1 }; // don't verify optimized file (not recommended)
long optimize_pcm_data( void const* in, long in_size, void* out, int flags );

#ifdef __cplusplus
	}
#endif

#endif

