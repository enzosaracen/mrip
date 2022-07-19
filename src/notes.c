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
#define W	1280
#define FS	(1000.0/60.0)

typedef uint32_t uint32;

FILE *fout;

int dbg = 0;
int on[88], keyoff[88], noff, kcnt;
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

int ison(int r, int g, int b)
{
	return (g - (r+b)/2 > 50) || (b - (r+g)/2 > 30);
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
	int i, j, s1[3], s2[3], diff, skip, cnt, prev, pn, sum, bw[88];

loop:
	frame(tms);
	setjpg();
	tms += FS*10;
	skip = 3;
	pn = cnt = prev = 0;
	for(i = 0; i < jpg.x; i++) {
		for(j = 0; j < 3; j++)
			s2[j] = jpg.jdata[i*3 + j];
		if(skip <= 0) {
			diff = 0;
			for(j = 0; j < 3; j++)
				diff += abs(s2[j]-s1[j]);
			if(diff >= 150) {
				keyoff[cnt++] = (prev+i)/2;
				prev = i;
				skip = 3;
			}
			if(dbg)
				putp(SDL_MapRGB(scr->format, s2[0], s2[1], s2[2]), diff >= 150, pn++);
		} else {
			if(dbg)
				putp(SDL_MapRGB(scr->format, s2[0], s2[1], s2[2]), 0, pn++);
			skip--;
			}
		for(j = 0; j < 3; j++)
			s1[j] = s2[j];
	}
	if(cnt < 30) {	/* min keys */
		printf("skipping %.3f\n", tms/1000.0);
		goto loop;
	}
	for(i = 0; i < cnt; i++) {
		for(j = 0; j < 3; j++)
			s2[j] = jpg.jdata[keyoff[i]*3 + j];
		sum = (s2[0] + s2[1] + s2[2])/3;
		if(sum >= 200)
			bw[i] = 1;
		else if(sum <= 50)
			bw[i] = 0;
		else {
			if(dbg)
				memset(rast, 0, W*H*sizeof(uint32));
			printf("skiping %.3f\n", tms/1000.0);
			goto loop;
		}
	}

	/* determine global note offset here */

	kcnt = cnt;
	if(dbg) {
		printf("keyboard %d (y/n)?\n", cnt);
		draw();
		if(getchar() != 'y')
			exit(1);
	}
}

int parse(void)
{
	int i, j, s[3], black;

	black = 1;
	frame(tms);
	setjpg();
	for(i = 0; i < kcnt; i++) {
		for(j = 0; j < 3; j++)
			s[j] = jpg.jdata[keyoff[i]*3 + j];
		if(black && (s[0]+s[1]+s[2])/3 > 50)
			black = 0;
		if(ison(s[0], s[1], s[2])) {
			if(!on[i])
				send(i, 1);
		} else if(on[i])
			send(i, 0);
	}
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
