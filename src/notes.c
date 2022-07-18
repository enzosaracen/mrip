#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <jerror.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <jpeglib.h>

#define H 100
#define W 1280

typedef uint32_t uint32;

FILE *fout;

int dbg = 0, init = 1;
int on[88];
int keyoff[88];
double tms;

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

void putp(uint32 col, int t, int n)
{
	int i;

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

void parse(void)
{
	FILE *fp;
	int i, j, x, s1[3], s2[3], diff, skip, cnt, prev, pn;
	unsigned char *jdata, *rowp[1];
	struct jpeg_error_mgr err;
	struct jpeg_decompress_struct info;

	fp = fopen("tmp/out.jpg", "r");
	info.err = jpeg_std_error(&err);
	jpeg_create_decompress(&info);
	jpeg_stdio_src(&info, fp);
	jpeg_read_header(&info, TRUE);
	jpeg_start_decompress(&info);
	x = info.output_width;
	jdata = malloc(x*3);
	rowp[0] = jdata + 3*x*info.output_scanline;
	jpeg_read_scanlines(&info, rowp, 1);

	if(init) {
		pn = 0;
		cnt = 0;
		prev = 0;
		skip = 3;
		for(i = 0; i < x; i++) {
			for(j = 0; j < 3; j++)
				s2[j] = jdata[i*3 + j];
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
		if(cnt != 88) {
			if(dbg) {
				printf("keyboard %d (y/n)?\n", cnt);
				draw();
				if(getchar() != 'y')
					exit(1);
			}
		}
		init = 0;
		free(jdata);
		fclose(fp);
		return;
	}

	for(i = 0; i < 88; i++) {
		for(j = 0; j < 3; j++)
			s1[j] = jdata[keyoff[i]*3 + j];
		if(ison(s1[0], s1[1], s1[2])) {
			if(!on[i])
				send(i, 1);
		} else if(on[i])
			send(i, 0);
	}
	free(jdata);
	fclose(fp);
}

int main(int argc, char *argv[])
{
	int i, j, start, end;

	if(argc != 3)
		return 1;
	fout = fopen("tmp/nout", "w");
	if(dbg) {
		if(SDL_Init(SDL_INIT_VIDEO) < 0)
			return 1;
		scr = SDL_SetVideoMode(W, H, 32, 0);
		if(scr == NULL)
			return 1;
	}

	start = atoi(argv[1]);
	end = atoi(argv[2]);
	tms = start*1000;

	frame((start+end)/2.0*1000);
	parse();
	for(i = start; i <= end; i++) {
		for(j = 0; j < 60; j++) {
			frame(tms);
			parse();
			tms += (1000.0/60.0);
			printf("%.3f\n", tms/1000.0);
		}
		/* to check midi throughout */
		fclose(fout);
		fout = fopen("tmp/nout", "a");
	}

	fclose(fout);
	if(dbg)
		SDL_Quit();
	return 0;
}
