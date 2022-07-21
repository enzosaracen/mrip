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
 *		additional pro:	it would remove most of the need to have a shell script
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
#define W	1500

#define FS	(1000.0/60.0)
#define COLDIF	100	/* sum of rgb diffs between base key color (key[i].col) and
			   current color at key[i].pos to register note on */
#define KEYDIF	150	/* sum of diff to signal key to key transition during keyscan */

typedef uint32_t uint32;

FILE *fout;

int dbg = 1;
int on[88], goff, nkey;
int plum[88];		/* prev frame col luminosities to check if all keys fade to
			   black (including notes left on) */
struct {
	int pos;
	int col[3];
} key[88];
double tms;
unsigned end;

uint32 rast[H][W];
SDL_Surface *scr;

struct {
	unsigned char *jdata;		
	unsigned x;
} jpg;

void draw(void)
{
	if(SDL_LockSurface(scr) < 0)
		exit(1);
	memcpy(scr->pixels, rast, W*H*sizeof(uint32));
	SDL_UnlockSurface(scr);
	SDL_UpdateRect(scr, 0, 0, 0, 0);
	memset(rast, 0, W*H*sizeof(uint32));
}

void putp(uint32 col, int t, int n)
{
	int i;

	if(n >= W)
		return;
	if(t) {
		for(i = 0; i < H/4; i++)
			rast[i][n] = 0xe10600;
	}
	for(i = H/4; i < H/2; i++)
		rast[i][n] = col;
}

int abs(int n)
{
	return (n >= 0) ? n : -n;
}

double lum(double f1, double f2, double f3)
{
	return 0.2126*f1 + 0.7152*f2 + 0.0722*f3;
}

uint32 rgb(int *s)
{
	return SDL_MapRGB(scr->format, s[0], s[1], s[2]);
}

int ccmp(int *s1, int *s2)
{
	return (abs(s1[0]-s2[0])+abs(s1[1]-s2[1])+abs(s1[2]-s2[2])) >= COLDIF;
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
	int i, j, k, s[3];
	char buf[128];

	sprintf(buf, "yes | ffmpeg -ss %.3f -i tmp/out.mp4 -qscale:v 4 -frames:v 1 tmp/out.jpg >/dev/null 2>&1", t/1000.0);
	system(buf);
	setjpg();
	for(j = 0; j < jpg.x; j++) {
		for(k = 0; k < 3; k++)
			s[k] = jpg.jdata[j*3 + k];
		for(i = H/2; i < H; i++)
			rast[i][j] = rgb(s);
	}
}

void keyscan(void)
{
	int i, j, k, s1[3], s2[3], col, diff, skip, n, prev, sum, bw[88];

loop:
	frame(tms);
	tms += FS*5;
	skip = 3;
	n = prev = 0;
	for(i = 0; i < jpg.x; i++) {
		for(j = 0; j < 3; j++)
			s2[j] = jpg.jdata[i*3 + j];
		if(skip <= 0) {
			diff = 0;
			for(j = 0; j < 3; j++)
				diff += abs(s2[j]-s1[j]);
			if(diff >= KEYDIF || i == jpg.x-1) {
				key[n].pos = (prev+i)/2;
				for(k = 0; k < 3; k++)
					key[n].col[k] = jpg.jdata[key[n].pos*3 + k];
				n++;
				prev = i;
				skip = 3;
			}
			if(dbg)
				putp(rgb(s2), skip > 0, i);
		} else {
			if(dbg)
				putp(rgb(s2), 0, i);
			skip--;
			}
		for(j = 0; j < 3; j++)
			s1[j] = s2[j];
	}
	if(n < 50 || n > 88) {
		printf("skipping %.3f (keys=%d)\n", tms/1000.0, n);
		goto loop;
	}
	for(i = 0; i < n; i++) {
		for(j = 0; j < 3; j++)
			s2[j] = jpg.jdata[key[i].pos*3 + j];
		sum = (s2[0] + s2[1] + s2[2])/3;
		if(sum >= 180)
			bw[i] = 1;
		else if(sum <= 60)
			bw[i] = 0;
		else {
			if(dbg)
				memset(rast, 0, W*H*sizeof(uint32));
			printf("skipping %.3f (bad color)\n", tms/1000.0);
			goto loop;
		}
	}

	/* determine global note offset here */

	nkey = n;
	if(dbg) {
		printf("keyboard %d (y/n)?\n", n);
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
		for(i = 0; i < nkey; i++) {
			for(j = 0; j < 3; j++)
				s1[j] = jpg.jdata[key[i].pos*3 + j];
			if(ccmp(key[i].col, s1)) {
				printf("first note(s): %d,%d,%d   %d,%d,%d\n", key[i].col[0], key[i].col[1], key[i].col[2], s1[0], s1[1], s1[2]);
				col = 1;
				send(i, 1);
			}
			if(!col)
				for(j = 0; j < 3; j++)
					key[i].col[j] = s1[j];
		}
	}
}

int parse(void)
{
	int i, j, t, s[3], fade;

	fade = 1;
	frame(tms);
	for(i = 0; i < nkey; i++) {
		for(j = 0; j < 3; j++)
			s[j] = jpg.jdata[key[i].pos*3 + j];

		t = lum(s[0], s[1], s[2]);
		if(plum[i]-t <= 0)
			fade = 0;
		plum[i] = t;

		t = ccmp(s, key[i].col);
		if(dbg)
			for(j = 0; j < 5; j++)
				putp(rgb(s), t, key[i].pos+j);
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
