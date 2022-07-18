#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <jerror.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <jpeglib.h>

#define H 100
#define W 1000

typedef uint32_t uint32;

FILE *fout, *fmsg;

int on[88];
uint32 ms;
double flms;
uint32 rast[H][W];
SDL_Surface *scr;

int abs(int n)
{
	return (n >= 0) ? n : -n;
}

void putp(uint32 col, int t, int n)
{
	int i;

	if(t) {
		for(i = 0; i < H/2; i++)
			rast[i][n] = col;
	}
	for(i = H/2; i < H; i++)
		rast[i][n] = col;
}

int ison(int r, int g, int b)
{
	return (g - (r+b)/2 > 50) || (b - (r+g)/2 > 30);
}

void send(int note, int v)
{
	on[note] = v;
	fprintf(fout, "%u %d: %d\n", ms, note, v);
	//printf("\t %u %d: %d\n", ms, note, v);
}

void parse(FILE *fp)
{
	int i, j, x, s1[3], s2[3], diff, skip, cnt, edge, pn;
	unsigned char *jdata, *rowp[1];
	struct jpeg_error_mgr err;
	struct jpeg_decompress_struct info;

	info.err = jpeg_std_error(&err);
	jpeg_create_decompress(&info);
	jpeg_stdio_src(&info, fp);
	jpeg_read_header(&info, TRUE);
	jpeg_start_decompress(&info);
	x = info.output_width;
	jdata = malloc(x*3);
	rowp[0] = jdata + 3*x*info.output_scanline;
	jpeg_read_scanlines(&info, rowp, 1);

	cnt = pn = 0;
	edge = skip = 3;
	for(i = 0; i < x; i++) {
		for(j = 0; j < 3; j++)
			s2[j] = jdata[i*3 + j];
		if(skip <= 0) {
			diff = 0;
			for(j = 0; j < 3; j++)
				diff += abs(s2[j]-s1[j]);
			if(diff >= 150) {
				cnt++;
				edge = skip = 3;
			}
			//putp(SDL_MapRGB(scr->format, s2[0], s2[1], s2[2]), diff >= 150, pn++);
		} else {
			if(edge == 1) {
				if(ison(s2[0], s2[1], s2[2])) {
				//	printf("%d on\n", cnt);
					if(!on[cnt])
						send(cnt, 1);
				} else if(on[cnt]) {
					send(cnt, 0);
				}
				edge = 0;	
			} else if(edge > 0)
				edge--;
			skip--;
		}
		for(j = 0; j < 3; j++)
			s1[j] = s2[j];
	}
	free(jdata);
	fclose(fp);
}

int main(void)
{
	int i;
	char buf[16];
	FILE *ftime;

	fout = fopen("tmp/nout", "a");
	fmsg = fopen("tmp/nmsg", "r");
	ftime = fopen("tmp/time", "r");

	for(i = 0; i < 88; i++) {
		fscanf(fmsg, "%d\n", &on[i]);
		if(feof(fmsg))
			break;
	}

	/*if(SDL_Init(SDL_INIT_VIDEO) < 0)
		return 1;
	scr = SDL_SetVideoMode(W, H, 32, 0);
	if(scr == NULL)
		return 1;*/

	fscanf(ftime, "%u", &ms);
	flms = ms;
	for(i = 1; i <= 60; i++) {
		sprintf(buf, "tmp/out%d.jpg", i);
		parse(fopen(buf, "r"));

		flms += 16.6666;	/* QUESTIONABLE */
		ms = round(flms);

		/*if(SDL_LockSurface(scr) < 0)
			return 1;
		memcpy(scr->pixels, rast, W*H*sizeof(uint32));
		SDL_UnlockSurface(scr);
		SDL_UpdateRect(scr, 0, 0, 0, 0);
		memset(rast, 0, W*H*sizeof(uint32));
		getchar();*/
	}
	fclose(fmsg);
	fmsg = fopen("tmp/nmsg", "w");
	for(i = 0; i < 88; i++)
		fprintf(fmsg, "%d\n", on[i]);

	fclose(fout);
	fclose(fmsg);
	SDL_Quit();
	return 0;
}
