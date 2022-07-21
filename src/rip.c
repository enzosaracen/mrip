#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <jerror.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <jpeglib.h>

/*
 *	current problems:
 *
 * 	- need to detect global note offset within keyscan, but this is trivial
 *
 * 	- may want to formalize black/white detection in keyscan
 *
 *	- no automatic y position detection
 *		can implement by pulling jpeg of entire vid
 *		and having keyscan() search through all scanlines until finding
 *		valid keyboard, then have video cropped and start processing
 *
 *	- newer videos have the key-on colors fade out rather than instantly
 *	return to baseline, causing longer than wanted notes and the detection of rapid
 *	hitting of the same note as only one on
 *		fixing this is messy and it can be ignored with still decent results,
 *		but can be done by detecting change in on color, recognizing that
 *		it is not back to baseline, then send off and ignore until it goes back to baseline
 *		or returns to the on color
 *		still wouldn't fix problem where there is a glow that comes from the notes
 *		and interferes with nearby keys leading to false ons - this is pretty much unfixable
 *
 *	- the detection of keys fading to black to end doesn't work for some videos where
 *	something is shown on screen before the fading occurs and covers the keys, causing
 *	random notes to be played at the end of songs
 *		this is pretty unfixable automatically, and because it doesn't affect the actual
 *		song it isn't a huge deal, also only happens on some of the newer videos, and it is easy
 *		to fix manually
 */

#define H	200	/* debug window */
#define W	1900
#define FS	(1000.0/60.0)
#define COLDIF	100	/* sum of rgb diff to signal key on */
#define KEYDIF	150	/* sum of rgb diff to signal key to key transition during keyscan */

typedef uint8_t uint8;
typedef uint32_t uint32;

FILE *fout;
int dbg = 1;
int on[88], goff, nkey;
int plum[88];
struct {
	int pos;
	uint8 col[3];
} key[88];
struct {
	uint8 *jdata;		
	unsigned x;
} jpg;
double tms;
unsigned end;

uint32 rast[H][W];
SDL_Surface *scr;

void draw(void)
{
	if(SDL_LockSurface(scr) < 0)
		exit(1);
	memcpy(scr->pixels, rast, W*H*sizeof(uint32));
	SDL_UnlockSurface(scr);
	SDL_UpdateRect(scr, 0, 0, 0, 0);
	memset(rast, 0, W*H*sizeof(uint32));
}

int abs(int n)
{
	return (n >= 0) ? n : -n;
}

double lum(uint8 *s)
{
	return 0.2126*((double)s[0]) + 0.7152*((double)s[1]) + 0.0722*((double)s[2]);
}

uint32 rgb(uint8 *s)
{
	return SDL_MapRGB(scr->format, s[0], s[1], s[2]);
}

int ccmp(uint8 *s1, uint8 *s2, int dif)
{
	return (abs(s1[0]-s2[0])+abs(s1[1]-s2[1])+abs(s1[2]-s2[2])) >= dif;
}

void send(int note, int v)
{
	on[note] = v;
	fprintf(fout, "%u %d: %d\n", (unsigned)tms, note, v);
}

void setjpg(void)
{
	FILE *fp;
	unsigned char *rowp[1];
	struct jpeg_error_mgr err;
	struct jpeg_decompress_struct info;

	fp = fopen("tmp/out.jpg", "r");
	info.err = jpeg_std_error(&err);
	jpeg_create_decompress(&info);
	jpeg_stdio_src(&info, fp);
	jpeg_read_header(&info, TRUE);
	jpeg_start_decompress(&info);
	if(info.output_width != jpg.x) {
		if(jpg.jdata)
			free(jpg.jdata);
		jpg.x = info.output_width;
		jpg.jdata = malloc(jpg.x*3);
	}
	rowp[0]	= jpg.jdata + 3*jpg.x*info.output_scanline;
	jpeg_read_scanlines(&info, rowp, 1);
	fclose(fp);
}

void frame(double t)
{
	int i, j;
	char buf[128];
	uint32 col;

	sprintf(buf, "yes | ffmpeg -ss %.3f -i tmp/out.mp4 -qscale:v 4 -frames:v 1 tmp/out.jpg >/dev/null 2>&1", t/1000.0);
	system(buf);
	setjpg();
	if(dbg)
		for(i = 0; i < jpg.x && i < W; i++) {
			col = rgb(jpg.jdata+i*3);
			for(j = H/2; j < H; j++)
				rast[j][i] = col;
		}
}

int scanl(unsigned char *data)
{
	int i, x, n, px, skip, bw[88];
	uint8 *ps;

	n = px = 0;
	ps = data;
	for(x = 1; x < jpg.x; x++) {
		if(ccmp(ps, data+x*3, KEYDIF) || x == jpg.x-1) {
			if(dbg && x < W)
				for(i = 0; i < H/2; i++)
					rast[i][x] = 0xe10600;

			key[n].pos = (px+x)/2;
			for(i = 0; i < 3; i++)
				key[n].col[i] = jpg.jdata[key[n].pos*3+i];
			n++;
			skip = (x-px)/5;
			px = x;
			x += skip;	/* skip some to avoid black spaces between white keys */
		}
		ps = data+x*3;
	}
	if(n < 36 || n > 88)
		return 1;
	for(x = 0; x < n; x++) {
		ps = data+key[x].pos*3;
		px = (ps[0]+ps[1]+ps[2])/3;
		if(px >= 180)
			bw[x] = 1;
		else if(px <= 60)
			bw[x] = 0;
		else
			return 1;
	}
	nkey = n;
	/* determine global key offset here */
	return 0;
}

void keyscan(void)
{
	int i, j, col;

	do {
		if(dbg)
			draw();
		frame(tms);
		tms += FS*5;
	} while(scanl(jpg.jdata) && tms <= end);
	if(dbg) {
		printf("keyboard %d (y/n)?\n", nkey);
		draw();
		if(getchar() != 'y')
			exit(1);
	}
	/* the keyboard is still fading in at this point, and need
	 * to get the stable color values of each key center to know when
	 * they change and are thus on, so keep running frame by
	 * frame comparisons until big color change (first note played),
	 * then assume colors from the previous frame will be relatively stable */
	col = 0;
	while(!col && tms <= end) {
		tms += FS;
		printf("keys fading in %.3f\n", tms/1000.0);
		frame(tms);
		if(dbg)
			draw();
		for(i = 0; i < nkey; i++) {
			if(ccmp(key[i].col, jpg.jdata+key[i].pos*3, COLDIF)) {
				col = 1;
				send(i, 1);
			}
			if(!col)
				for(j = 0; j < 3; j++)
					key[i].col[j] = jpg.jdata[key[i].pos*3+j];
		}
	}
}

int parse(void)
{
	int i, j, t, fade;

	fade = nkey/2;
	frame(tms);
	for(i = 0; i < nkey; i++) {
		t = lum(jpg.jdata+key[i].pos*3);
		if(fade > 0 && plum[i]-t <= 0)
			fade--;
		plum[i] = t;

		t = ccmp(jpg.jdata+key[i].pos*3, key[i].col, COLDIF);
		if(dbg && t)
			for(j = 0; j < H/2; j++)
				rast[j][key[i].pos] = 0xe10600;
		if(t) {
			if(!on[i])
				send(i, 1);
		} else if(on[i])
			send(i, 0);
	}
	if(dbg)
		draw();
	return fade;
}

int main(int argc, char *argv[])
{
	if(argc != 2)
		return 1;
	end = atof(argv[1])*1000;
	fout = fopen("tmp/nout", "w");
	if(dbg) {
		if(SDL_Init(SDL_INIT_VIDEO) < 0)
			return 1;
		scr = SDL_SetVideoMode(W, H, 32, 0);
		if(scr == NULL)
			return 1;
	}
	keyscan();
	while(tms <= end) {
		if(parse()) {
			printf("notes faded\n");
			break;
		}
		printf("%.3f\n", tms/1000.0);
		tms += FS;

 		/* to check midi throughout */
		fclose(fout);
		fout = fopen("tmp/nout", "a");
	}
	fclose(fout);
	if(dbg)
		SDL_Quit();
	return 0;
}
