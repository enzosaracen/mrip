#include <stdio.h>
#include <stdint.h>

typedef uint32_t uint32;

FILE *fp;

int prev[88];

unsigned siz;

void put(int n, uint32 v)
{
	siz += n;
	while(n--)
		fputc((v >> n*8) & 0xff, fp);
}

void delta(uint32 ms)
{
	int i, start;
	unsigned char b;

	if(ms == 0) {
		put(1, 0);
		return;
	}

	start = 0;
	b = (ms >> 28) & 0xf;
	if(b != 0) {
		put(1, b | 0x80);
		start = 1;
	}
	for(i = 1; i <= 4; i++) {
		b = (ms >> (28 - 7*i)) & 0x7f;
		if(start || b != 0) {
			start = 1;
			if(i == 4)
				put(1, b);
			else
				put(1, b | 0x80);
		} else if(i == 4)
			put(1, 0);
	}
}

int main(void)
{
	int i, note, on;
	long ms1, ms2;
	fpos_t chsiz;
	FILE *fin;

	fp = fopen("out.mid", "w");
	fin = fopen("tmp/nout", "r");
	fprintf(fp, "MThd");
	put(4, 6);
	put(2, 0);
	put(2, 1);
	put(2, 500);

	fprintf(fp, "MTrk");
	fgetpos(fp, &chsiz);
	put(4, 0);

	ms1 = -1;
	siz = 0;
	put(3, 0xc001);

	for(i = 0; i < 88; i++)
		prev[i] = -1;
	while(fscanf(fin, "%ld %d: %d\n", &ms2, &note, &on) && !feof(fin)) {
		if(ms1 == -1)
			delta(0);
		else
			delta(ms2-ms1);
		if(prev[note] == on)
			printf("bad note %d\n", note);
		prev[note] = on;
		put(1, on ? 0x90 : 0x80);
		put(1, note+20);
		put(1, 127);
		ms1 = ms2;
	}
	put(4, 0xff2f00);

	fsetpos(fp, &chsiz);
	put(4, siz);
}
