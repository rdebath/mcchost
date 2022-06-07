#include <string.h>

#include "hexdump.h"

#if INTERFACE
#include <stdio.h>
#define hexdump_use_iso 0
#endif

int hexdump_address;

void
hex_logfile(int ch)
{
static char linebuf[80];
static char buf[20];
static int pos = 0;

   if (ch != EOF)
   {
      if(!pos)
         memset(linebuf, ' ', sizeof(linebuf));
      sprintf(buf, "%02x", ch&0xFF);
      memcpy(linebuf+pos*3+(pos>7), buf, 2);

      if( ( ch > ' ' && ch <= '~' ) || (hexdump_use_iso && ch > 160) )
            linebuf[50+pos] = ch;
      else  linebuf[50+pos] = '.';
      pos = ((pos+1) & 0xF);
   }

   if((ch == EOF) != (pos == 0))
   {
      if (hexdump_address != -1) {
         fprintf_logfile("%04x: %.66s", hexdump_address, linebuf);
         hexdump_address += 16;
      } else
         fprintf_logfile(": %.66s", linebuf);
      pos = 0;
   }
   if (ch == EOF) hexdump_address = 0;
}
