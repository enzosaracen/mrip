#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <jerror.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <jpeglib.h>

#define H		200
#define W		1900
#define FS		(1000.0/60.0)

#define YSLICE		20
#define GRAYDIF		50
#define KEYDIF		75
#define COLDIF		100
#define BIGDIF		150

typedef uint8_t uint8;
typedef uint32_t uint32;

FILE *fout;
int dbg = 1;
int on[88], piano[88], goff, nkey;
int octave[12] = {1,0,1,0,1,1,0,1,0,1,0,1};
struct {
	int pos;
	uint8 col[3];
} key[88];
struct {
	uint8 *jdata;		
	unsigned x, y;
} jpg;
double tms;
unsigned end;
char *vid, cmdbuf[512];

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
	return n >= 0 ? n : -n;
}

uint32 rgb(uint8 *s)
{
	return SDL_MapRGB(scr->format, s[0], s[1], s[2]);
}

int ccmp(uint8 *s1, uint8 *s2, int dif)
{
	return abs(s1[0]-s2[0])+abs(s1[1]-s2[1])+abs(s1[2]-s2[2]) >= dif;
}

int isbw(uint8 *s)
{
	int sum;

	if(abs(s[0]-(s[1]+s[2])/2) > GRAYDIF || abs(s[1]-(s[0]+s[2])/2) > GRAYDIF || abs(s[2]-(s[0]+s[1])/2) > GRAYDIF) 
		return -1;
	sum = s[0]+s[1]+s[2];
	if(sum <= 200)
		return 0;
	if(sum >= 500)
		return 1;	
	return -1;
}

void send(int note, int v)
{
	on[note] = v;
	fprintf(fout, "%u %d: %d\n", (unsigned)tms, note+goff, v);
}

void frame(double t, int full)
{
	int i, j;
	double f;
	uint32 col;
	FILE *fp;
	unsigned char *rowp[1];
	struct jpeg_error_mgr err;
	struct jpeg_decompress_struct info;

	sprintf(cmdbuf, "ffmpeg -ss %.3f -i \"%s\" -qscale:v 4 -frames:v 1 -f image2 pipe:1 2>/dev/null", t/1000.0, full ? vid : "tmp/out.mp4");
	fp = popen(cmdbuf, "r");
	info.err = jpeg_std_error(&err);
	jpeg_create_decompress(&info);
	jpeg_stdio_src(&info, fp);
	jpeg_read_header(&info, TRUE);
	jpeg_start_decompress(&info);

	if(info.output_width != jpg.x || info.output_height != jpg.y) {
		if(jpg.jdata)
			free(jpg.jdata);
		jpg.x = info.output_width;
		jpg.y = info.output_height;
		if(full)
			jpg.jdata = malloc(YSLICE*jpg.x*3);
		else
			jpg.jdata = malloc(jpg.x*3);
	}
	if(!full) {
		rowp[0] = jpg.jdata;
		jpeg_read_scanlines(&info, rowp, 1);
		if(dbg) {
			for(i = 0; i < jpg.x && i < W; i++) {
				col = rgb(jpg.jdata+i*3);
				for(j = H/2; j < H; j++)
					rast[j][i] = col;
			}
		}
	} else {
		j = 0;
		for(f = 0; f < 1; f += 1.0/YSLICE) {
			i = f*(double)jpg.y;
			if(i-info.output_scanline > 0)
				jpeg_skip_scanlines(&info, i-info.output_scanline);
			rowp[0] = jpg.jdata + j*3*jpg.x;
			jpeg_read_scanlines(&info, rowp, 1);
			j++;
		}
	}
	pclose(fp);
}

int scanl(unsigned char *data)
{
	int i, j, x, n, px, skip, bw[88], off[8], noff, bpos[88];
	uint8 *ps, bcol[88][3];
	uint32 col;

	if(dbg)
		for(i = 0; i < jpg.x && i < W; i++) {
			col = rgb(data+i*3);
			for(j = H/2; j < H; j++)
				rast[j][i] = col;
		}

	n = px = 0;
	ps = data+(jpg.x/100-1)*3;
	for(x = jpg.x/100; x < jpg.x; x++) {
		if(ccmp(ps, data+x*3, KEYDIF) || x == jpg.x-1) {
			if(dbg && x < W)
				for(i = 0; i < H/2; i++)
					rast[i][x] = 0xe10600;
			if(n >= 88)
				return 1;
			bpos[n] = (px+x)/2;
			for(i = 0; i < 3; i++)
				bcol[n][i] = jpg.jdata[bpos[n]*3+i];
			n++;
			skip = (x-px)/2;
			px = x;
			x += skip;
		}
		ps = data+x*3;
	}

	if(n < 36)
		return 1;
	for(i = 0; i < n; i++)
		if((bw[i] = isbw(data+bpos[i]*3)) < 0)
			return 1;
	if(n < 88) {
		noff = 0;
		for(i = 0; i < 88-n; i++)
			for(j = 0; j < n; j++) {
				if(bw[j] != piano[j+i])
					break;
				if(j == n-1)
					off[noff++] = i;
			}
		if(noff == 0)
			return 1;
		i = 0;
		if(noff > 1) {
			printf("ambiguous %d key layout (%d possible offsets):  ", n, noff);
			for(j = 0; j < noff; j++)
				printf("%d %c", off[j], j == noff-1 ? '\n' : ' ');
			printf("enter choice (0-%d):\n", noff-1);
			i = getchar()-'0';
			if(i < 0 || i >= noff)
				exit(1);
			while((j = getchar()) != '\n' && j != EOF);
		}
		goff = off[i];
		printf("note offset: %d\n", goff);
	}
	for(i = 0; i < n; i++) {
		key[i].pos = bpos[i];
		for(j = 0; j < 3; j++)
			key[i].col[j] = bcol[i][j];
	}
	nkey = n;
	return 0;
}

void keyscan(void)
{
	int i, j, e;

	e = 0;
	while(!e) {
		if(tms >= end) {
			printf("no keyboard found\n");
			exit(1);
		}
		frame(tms, 1);
		printf("skipping %.3f\n", tms/1000.0);
		tms += FS*5;
		for(i = 0; i < YSLICE; i++) {
			if(dbg)
				memset(rast, 0, W*H*sizeof(uint32));
			if(scanl(jpg.jdata+i*3*jpg.x) == 0) {
				e = 1;
				j = i;
				draw();
			}
		}
	}
	if(dbg) {
		printf("keyboard %d (y/n)?\n", nkey);
		if(getchar() != 'y')
			exit(1);
	}
	sprintf(cmdbuf, "yes | ffmpeg -i \"%s\" -filter:v \"crop=iw:2:0:%d\" tmp/out.mp4", vid, (int)(((double)j)/YSLICE*jpg.y));
	system(cmdbuf);
	e = 0;
	while(!e && tms <= end) {
		tms += FS;
		printf("waiting for note %.3f\n", tms/1000.0);
		frame(tms, 0);
		if(dbg)
			draw();
		for(i = 0; i < nkey; i++) {
			if(ccmp(key[i].col, jpg.jdata+key[i].pos*3, BIGDIF) && isbw(jpg.jdata+key[i].pos*3) < 0) {
				e = 1;
				send(i, 1);
			}
			if(!e)
				for(j = 0; j < 3; j++)
					key[i].col[j] = jpg.jdata[key[i].pos*3+j];
		}
	}
}

int parse(void)
{
	int i, j, t, sbuf[88], oncnt;

	frame(tms, 0);
	oncnt = 0;
	for(i = 0; i < nkey; i++) {
		sbuf[i] = -1;
		t = ccmp(jpg.jdata+key[i].pos*3, key[i].col, COLDIF);
		if(t) {
			if(dbg)
				for(j = 0; j < H/2; j++)
					rast[j][key[i].pos] = 0xe10600;
			oncnt++;
			if(!on[i])
				sbuf[i] = 1;
		} else if(on[i])
			sbuf[i] = 0;
	}
	if(oncnt > nkey/3)
		return 1;
	for(i = 0; i < nkey; i++)
		if(sbuf[i] != -1)
			send(i, sbuf[i]);
	if(dbg)
		draw();
	return 0;
}

int main(int argc, char *argv[])
{
	int i, j;

	if(argc != 3)
		return 1;
	vid = argv[1];
	end = atof(argv[2])*1000;
	fout = fopen("tmp/nout", "w");
	if(dbg) {
		if(SDL_Init(SDL_INIT_VIDEO) < 0)
			return 1;
		scr = SDL_SetVideoMode(W, H, 32, 0);
		if(scr == NULL)
			return 1;
	}

	piano[0] = 1;
	piano[1] = 0;
	piano[2] = 1;
	for(i = 0; i < 7; i++)
		for(j = 0; j < 12; j++)
			piano[i*12+j+3] = octave[j];
	piano[87] = 1;
	
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
