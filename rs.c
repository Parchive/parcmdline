/*\
|*|  Parity Archive - A way to restore missing files in a set.
|*|
|*|  Copyright (C) 2001  Willem Monsuwe (willem@stack.nl)
|*|
|*|  File format by Stefan Wehlus -
|*|   initial idea by Tobias Rieper, further suggestions by Kilroy Balore 
|*|
|*|  Reed-Solomon coding
\*/

#include <stdio.h>
#include "types.h"
#include "fileops.h"
#include "rs.h"
#include "util.h"

/*\
|*| Calculations over a Galois Field, GF(8)
\*/

u8 gl[0x100], ge[0x100];

/*\ Multiply: a*b = exp(log(a) + log(b)) \*/
static int
gmul(int a, int b)
{
	int i;
	if ((a == 0) || (b == 0)) return 0;
	i = gl[a] + gl[b];
	if (i > 255) i -= 255;
	return ge[i];
}

/*\ Divide: a/b = exp(log(a) - log(b)) \*/
static int
gdiv(int a, int b)
{
	int i;
	if ((a == 0) || (b == 0)) return 0;
	i = gl[a] - gl[b];
	if (i < 0) i += 255;
	return ge[i];
}

/*\ Power: a^b = exp(log(a) * b) \*/
static int
gpow(int a, int b)
{
	if (a == 0) return 0;
	return ge[(gl[a] * b) % 255];
}

/*\ Initialise log and exp tables using generator x^8 + x^4 + x^3 + x^2 + 1 \*/
void
ginit(void)
{
	unsigned int b, l;

	b = 1;
	for (l = 0; l < 0xff; l++) {
		gl[b] = l;
		ge[l] = b;
		b += b;
		if (b & 0x100) b ^= 0x11d;
	}
	ge[0xff] = ge[0];
}

/*\ Fill in a LUT \*/
static void
make_lut(u8 lut[0x100], int m)
{
	int j;
	for (j = 0; j < 0x100; j++)
		lut[j] = gmul(j, m);
}

#define MT(i,j)     (mt[((i) * N) + (j)])
#define IMT(i,j)   (imt[((i) * N) + (j)])
#define MULS(i,j) (muls[((i) * N) + (j)])

int
recreate(xfile_t *in, int N, xfile_t *out, int M)
{
	int i, j, k, R;
	u8 *mt, *imt, *muls;
	u8 buf[0x4000], *work;
	size_t s, size;
	size_t perc;

	ginit();

	/*\ Count number of recovery files \*/
	for (i = R = 0; i < N; i++)
		if (in[i].volnr)
			R++;

	NEW(mt, R * N);
	CNEW(imt, R * N);

	/*\ Fill in matrix rows for recovery files \*/
	for (i = j = 0; j < R; j++) {
		while (!in[i].volnr)
			i++;
		for (k = 0; k < N; k++)
			MT(j, k) = gpow(k + 1, in[i].volnr - 1);
		IMT(j, i) = 1;
		i++;
	}

	/*\ Use (virtual) rows from data files to eliminate columns \*/
	for (i = 0; i < N; i++) {
		if (in[i].volnr)
			continue;
		k = in[i].filenr - 1;
		/*\ Both mt and imt only have a 1 at (k,k), so subtracting
		|*| the row means subtracting mt(k,j) from imt(k,j)
		|*| and then setting mt(k,j) to zero.  \*/
		for (j = 0; j < R; j++) {
			IMT(j, k) ^= MT(j, k);
			MT(j, k) = 0;
		}
	}

	/*\ Eliminate columns using the remaining rows, so we get I.
	|*| The accompanying matrix will be the inverse
	\*/
	for (i = 0; i < R; i++) {
		int d, l;
		/*\ Find first non-zero entry \*/
		for (l = 0; !MT(i, l); l++)
			;
		d = MT(i, l);
		/*\ Scale the matrix so MT(i, l) becomes 1 \*/
		for (j = 0; j < N; j++) {
			MT(i, j) = gdiv(MT(i, j), d);
			IMT(i, j) = gdiv(IMT(i, j), d);
		}
		/*\ Eliminate the column in the other matrices \*/
		for (k = 0; k < R; k++) {
			if (k == i) continue;
			d = MT(k, l);
			for (j = 0; j < N; j++) {
				MT(k, j) ^= gmul(MT(i, j), d);
				IMT(k, j) ^= gmul(IMT(i, j), d);
			}
		}
	}

	/*\ Make the multiplication tables \*/
	NEW(muls, M * N);

	for (i = 0; i < M; i++) {
		if (out[i].volnr) {
			int d;
			/*\ Checksum #x: F(x), ... \*/
			for (k = 0; k < N; k++)
				MULS(i, k) = gpow(k + 1, out[i].volnr - 1);
			/*\ But every missing file is
			|*| replaced by IMT(m) * F(x,m) \*/
			for (j = 0; j < R; j++) {
				for (k = 0; !MT(j,k); k++)
					;
				d = gpow(k + 1, out[i].volnr - 1);
				MULS(i, k) ^= d;
				for (k = 0; k < N; k++)
					MULS(i, k) ^= gmul(d, IMT(j, k));
			}
		} else {
			/*\ File #x: The row IMT(j) for which MT(j,x) = 1 \*/
			for (j = 0; !MT(j, out[i].filenr - 1); j++)
				;
			for (k = 0; k < N; k++)
				MULS(i, k) = IMT(j, k);
		}
	}
	free(mt);
	free(imt);

	/*\ Restore all the files at once \*/
	NEW(work, sizeof(buf) * M);

	/*\ Find out how much we should process in total \*/
	size = 0;
	for (i = 0; i < M; i++)
		if (size < out[i].size)
			size = out[i].size;

	perc = 0;
	fprintf(stderr, "0%%"); fflush(stderr);
	/*\ Process all files \*/
	for (s = 0; s < size; ) {
		ssize_t tr, r, q;
		u8 *p;

		/*\ Display progress \*/
		while (((s * 50) / size) > perc) {
			perc++;
			if (perc % 5) fprintf(stderr, ".");
			else fprintf(stderr, "%d%%", (perc / 5) * 10);
			fflush(stderr);
		}

		/*\ See how much we should read \*/
		memset(work, 0, sizeof(buf) * M);
		for (i = 0; i < N; i++) {
			tr = sizeof(buf);
			if (tr > (in[i].size - s))
				tr = in[i].size - s;
			if (tr <= 0)
				continue;
			r = file_read(in[i].f, buf, tr);
			if (r < tr) {
				fprintf(stderr, "READ ERROR!\n");
				free(muls);
				free(work);
				return 0;
			}
			for (j = 0; j < M; j++) {
				u8 lut[0x100];
				if (s >= out[j].size) continue;
				/*\ Precalc LUT \*/
				make_lut(lut, MULS(j, i));
				p = work + (j * sizeof(buf));
				/*\ XOR it in, passed through the LUTs \*/
				for (q = 0; q < r; q++)
					p[q] ^= lut[buf[q]];
			}
		}
		for (j = 0; j < M; j++) {
			if (s >= out[j].size) continue;
			tr = sizeof(buf);
			if (tr > (out[j].size - s))
				tr = out[j].size - s;
			r = file_write(out[j].f, work + (j * sizeof(buf)), tr);
			if (r < tr) {
				fprintf(stderr, "WRITE ERROR!\n");
				free(muls);
				free(work);
				return 0;
			}
		}
		s += sizeof(buf);
	}
	fprintf(stderr, "100%%\n"); fflush(stderr);
	free(muls);
	free(work);
	return 1;
}
