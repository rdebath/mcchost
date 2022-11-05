
#include "cmdmeasure.h"

/*HELP measure,ms H_CMD
&T/measure [block] [block2]..
Measures all the blocks between two points
Shows information such as dimensions, most frequent blocks
  <blocks> optionally indicates which blocks to only count
Aliases: /ms
*/

#if INTERFACE
#define CMD_MEASURE \
    {N"measure", &cmd_measure}, {N"ms", &cmd_measure, .dup=1}
#endif

void
cmd_measure(char * cmd, char * arg)
{
    if (!marks[0].valid || !marks[1].valid) {
	if (!marks[0].valid) {
	    if (!extn_heldblock || !extn_messagetypes)
		printf_chat("&SPlace or break two blocks to determine edges.");
	}
	player_mark_mode = 2;
	if (marks[0].valid) player_mark_mode--;
	request_pending_marks("Selecting region for Measure", cmd_measure, cmd, arg);
	return;
    }

    block_t bllist[256];
    int blcount = 0;
    for(char * ar = arg?strtok(arg, " "):0;ar;ar=strtok(0," ")) {
	block_t b = block_id(ar);
	if (b == BLOCKNIL) {
	    printf_chat("&WUnknown block '%s'", ar?ar:"");
	    return;
	}
	bllist[blcount++] = b;
    }

    measure_blocks(bllist, blcount,
	marks[0].x, marks[0].y, marks[0].z,
	marks[1].x, marks[1].y, marks[1].z);

    clear_pending_marks();
}

void
measure_blocks(block_t*bllist, int blcount, int x0, int y0, int z0, int x1, int y1, int z1)
{
    int args[7] = {0, x0, y0, z0, x1, y1, z1};

    if (clamp_cuboid(args) == 0)
	return;

    int nocount = 5;
    if (blcount != 0) nocount = blcount;
    int * countlist = calloc(nocount, sizeof(int));

    if (blcount == 1) {
	// Single block, simple and might be faster.
	int x, y, z, count = 0;
	block_t b = bllist[0];
	for(y=args[2]; y<=args[5]; y++)
	    for(x=args[1]; x<=args[4]; x++)
		for(z=args[3]; z<=args[6]; z++)
		{
		    uintptr_t index = World_Pack(x, y, z);
		    if (level_blocks[index] == b)
			count++;
		}
	countlist[0] = count;
    } else {
	int counts[BLOCKMAX] = {0};

	int x, y, z;
	for(y=args[2]; y<=args[5]; y++)
	    for(x=args[1]; x<=args[4]; x++)
		for(z=args[3]; z<=args[6]; z++)
		{
		    uintptr_t index = World_Pack(x, y, z);
		    block_t b = level_blocks[index];
		    if (b < BLOCKMAX) counts[b] ++;
		}

	if (blcount) {
	    for(int i = 0; i<blcount; i++)
		countlist[i] = counts[bllist[i]];
	} else {
	    for(int i=0; i<BLOCKMAX; i++) {
		if (counts[i] < countlist[nocount-1]) continue;
		bllist[nocount-1] = i;
		countlist[nocount-1] = counts[i];
		for(int j=nocount-1; j>0; j--) {
		    if (countlist[j] > countlist[j-1]) {
			int t = countlist[j];
			countlist[j] = countlist[j-1];
			countlist[j-1] = t;
			t = bllist[j];
			bllist[j] = bllist[j-1];
			bllist[j-1] = t;
		    }
		}
	    }
	}
    }

    printf_chat("Measuring from &T(%d, %d, %d)&S to &T(%d, %d, %d)",
	args[1], args[2], args[3], args[4], args[5], args[6]);

    int totcells = (args[4]-args[1]+1)*(args[5]-args[2]+1)*(args[6]-args[3]+1);
    printf_chat("  &b%d&S wide, &b%d&S high, &b%d&S long, &b%d&S blocks",
	args[4]-args[1]+1, args[5]-args[2]+1, args[6]-args[3]+1, totcells);

    char bbuf[8192];
    sprintf(bbuf, "Block types:");
    for(int i = 0; i<nocount; i++) {
	if (countlist[i] == 0) continue;
	int l = strlen(bbuf) + 10;
	l += sizeof(int)*3+3;
	char * bn = block_name(bllist[i]);
	l += strlen(bn);
	if (l>sizeof(bbuf)) break;

	sprintf(bbuf+strlen(bbuf), "%s %s: %d (%d%%)",
	    i?",":"", bn, countlist[i],
	    (int)(100.0*countlist[i]/totcells));
    }
    printf_chat("%s", bbuf);
    free(countlist);
}
