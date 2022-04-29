
/*
 * Reinventing the UTF-8 decoder.
 * It does not decode CESU-8 sequences.
 * The utfstate variable should be initialised to zero.
 * Negative return values are the result of illegal UTF-8 sequences and
 * should be considered to be U+FFFD equivalents. Note however, treating
 * these as bytes results in the regeneration of the original stream.
 *
 * If the UTFNIL code is returned it means more bytes are needed.
 *
 * Note:
 *  utfstate known values, 0= empty, +ve pending, -ve errored.
 *  if utfstate is errored give it an EOF as input to get next U+FFFD eqv.
 *  EOF on input will make any pending state error.
 *
 *  NB: Overlength can NOT be detected from first byte. (Needs two)
 *
 *     Table 3-7. Well-Formed UTF-8 Byte Sequences
 *  Code Points        First Byte  Second Byte Third Byte Fourth Byte
 *  U+0000..U+007F     00..7F
 *  U+0080..U+07FF     C2..DF      80..BF
 *  U+0800..U+0FFF     E0         *A0..BF*     80..BF
 *  U+1000..U+CFFF     E1..EC      80..BF      80..BF
 *  U+D000..U+D7FF     ED         *80..9F*     80..BF
 *  U+E000..U+FFFF     EE..EF      80..BF      80..BF
 *  U+10000..U+3FFFF   F0         *90..BF*     80..BF     80..BF
 *  U+40000..U+FFFFF   F1..F3      80..BF      80..BF     80..BF
 *  U+100000..U+10FFFF F4          80..8F      80..BF     80..BF
 *
 */

#include "utf8.h"

#if INTERFACE
#define UTFNIL	(-257)
#endif

int
decodeutf8(int ch, int * utfstate)
{
    static unsigned char UTFlen[] = {
	0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	/*             3, 3, 3, 4, 4, 4, 4, 5, 5, 6, 7 // consistent but illegal. */
    };

    if (ch < 0) ch = -1;   /* EOF */
    if (ch != -1 && *utfstate < 0) *utfstate = 0;

    if (*utfstate == 0) {
	int utf_size;
	if (ch < 0x80)
	    return ch;

	if (ch >= 0xC0 && (utf_size = UTFlen[ch&0x3F]) != 0) {
	    *utfstate = (((ch&0x3F)<< (6*utf_size)) + (utf_size<<27) + (1<<24));
	    return UTFNIL;
	}
    } else {
	if (ch >= 0x80 && ch < 0xC0) {
	    int need = (*utfstate >> 27) & 7;
	    int got = (*utfstate >> 24) & 7;
	    need--; got++;
	    *utfstate |= ((ch&0x3F) << (6*need));
	    *utfstate &= 0xFFFFFF;
	    *utfstate |= (need<<27) + (got<<24);
	    if (need>0) return UTFNIL;

	    // Decode and check for overlength and other errors.
	    ch = *utfstate;
	    switch( UTFlen[(ch>>(6*(got-1))) & 0x3F] )
	    {
	    default: ch = -1; break;
	    case 1:
		ch &= 0x7FF;
		if (ch < 0x80) ch = -1;
		break;
	    case 2:
		ch &= 0xFFFF;
		if (ch < 0x800) ch = -1;
		if (ch >= 0xD800 && ch < 0xE000) ch = -1;   /* CESU-8 */
		break;
	    case 3:
		ch &= 0x1FFFFF;
		if (ch < 0x10000 || ch > 0x10FFFF) ch = -1;
		break;
	    }
	    if (ch != -1) {
		*utfstate = 0; return ch;
	    }
	}
    }

    /* ERROR, need to reconstruct the original byte sequence. */
    if (*utfstate == 0) return (signed char)ch;
    else {
	unsigned char unpacked_chars[8];
	int unpacked_count = 0;

	if (*utfstate > 0) {
	    int need = (*utfstate >> 27) & 7;
	    int got = (*utfstate >> 24) & 7;

	    /* Convert to unpacked form */
	    got--;
	    unpacked_chars[unpacked_count++] =
		    0xC0 + ((*utfstate >> (6*(need+got))) & 0x3F);

	    while(got>0) {
		got--;
		unpacked_chars[unpacked_count++] =
		    0x80 + ((*utfstate >> (6*(need+got))) & 0x3F);
	    }

	    /* Add ch to end if not -1 */
	    if (ch != -1)
		unpacked_chars[unpacked_count++] = ch;
	} else {
	    /* Unpack negative form */
	    int got = (*utfstate >> 24) & 7;

	    while(got>0) {
		got--;
		unpacked_chars[unpacked_count] =
		    ((*utfstate >> (8*unpacked_count)) & 0xFF);
		unpacked_count++;
	    }
	}

	*utfstate = 0;
	if (unpacked_count==1 && unpacked_chars[0] >= 0xC0 && ch != -1)
	    return decodeutf8(unpacked_chars[0], utfstate);

	if (unpacked_count>0) ch = (signed char)unpacked_chars[0]; else ch = UTFNIL;
	if (unpacked_count>1) {
	    /* Repack into error form */
	    int i;
	    for(i=1; i<unpacked_count; i++)
		*utfstate |= (unpacked_chars[i] << 8*(i-1) );

	    *utfstate |= ((unpacked_count-1) << 24);
	    *utfstate |= (-1 & ~0xFFFFFFF);
	}
	return ch;
    }
}
