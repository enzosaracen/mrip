#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <jerror.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <jpeglib.h>

#define H	100
#define W	1500
#define FS	(1000.0/60.0)
#define COLDIF	80
#define KEYDIF	150

typedef uint32_t uint32;

FILE *fout;

int dbg = 1;
int on[88], goff, nkey;
struct {
	int pos;
	int col[3];
} key[88];
double tms;
unsigned vend;

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
		for(i = 0; i < H/2; i++)
			rast[i][n] = 0xe10600;
	}
	for(i = H/2; i < H; i++)
		rast[i][n] = col;
}

void frame(double t)
{
	char buf[128];

	sprintf(buf, "yes | ffmpeg -ss %.3f -i tmp/out.mp4 -qscale:v 4 -frames:v 1 tmp/out.jpg >/dev/null 2>&1", t/1000.0);
	system(buf);
}

int abs(int n)
{
	return (n >= 0) ? n : -n;
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

void keyscan(void)
{
	int i, j, k, s1[3], s2[3], diff, skip, n, prev, sum, bw[88];

loop:
	frame(tms);
	setjpg();
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
	if(n < 30) {
		printf("skipping %.3f (keys=%d < 30)\n", tms/1000.0, n);
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
			printf("skipping %.3f (color %d,%d,%d)\n", tms/1000.0, s2[0], s2[1], s2[2]);
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
	 * then assume previous frame off colors will be relatively stable */
	while(tms <= vend) {
		tms += FS;
		printf("keys fading %.3f\n", tms/1000.0);
		frame(tms);
		setjpg();
		for(i = 0; i < nkey; i++) {
			for(j = 0; j < 3; j++)
				s1[j] = jpg.jdata[key[i].pos*3 + j];
			if(ccmp(key[i].col, s1)) {
				printf("%d: %d,%d,%d   %d,%d,%d", i, s1[0], s1[1], s1[2], key[i].col[0], key[i].col[1], key[i].col[2]);
				return;
			}
			for(j = 0; j < 3; j++)
				key[i].col[j] = s1[j];
		}
	}
}

int parse(void)
{
	int i, j, k, t, s[3], black;

	black = 1;
	frame(tms);
	setjpg();
	for(i = 0; i < nkey; i++) {
		for(j = 0; j < 3; j++)
			s[j] = jpg.jdata[key[i].pos*3 + j];

		/* to stop after keys fade out */
		if(black && (s[0]+s[1]+s[2])/3 > 50)
			black = 0;

		t = ccmp(s, key[i].col);
		if(dbg)
			for(k = 0; k < 5; k++)
				putp(rgb(s), t, key[i].pos+k);
		if(t) {
			if(!on[i])
				send(i, 1);
		} else if(on[i])
			send(i, 0);
	}
	if(dbg)
		draw();
	return black;
}

int main(int argc, char *argv[])
{
	if(argc != 2)
		return 1;
	vend = atof(argv[1])*1000;
	fout = fopen("tmp/nout", "w");
	if(dbg) {
		if(SDL_Init(SDL_INIT_VIDEO) < 0)
			return 1;
		scr = SDL_SetVideoMode(W, H, 32, 0);
		if(scr == NULL)
			return 1;
	}
	keyscan();
	while(tms <= vend) {
		if(parse())
			break;
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
