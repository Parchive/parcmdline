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
#include <string.h>
#include "types.h"
#include "fileops.h"
#include "rs.h"
#include "util.h"

/*\
|*| Calculations over a Galois Field, GF(8)
\*/

u8 gl[0x100], ge[0x200];

/*\ Multiply: a*b = exp(log(a) + log(b)) \*/
static int
gmul(int a, int b)
{
	if ((a == 0) || (b == 0)) return 0;
	return ge[gl[a] + gl[b]];
}

/*\ Divide: a/b = exp(log(a) - log(b)) \*/
static int
gdiv(int a, int b)
{
	if ((a == 0) || (b == 0)) return 0;
	return ge[gl[a] - gl[b] + 255];
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
	for (l = 0xff; l < 0x200; l++)
		ge[l] = ge[l - 0xff];
}

/*\ Fill in a LUT \*/
static void
make_lut(u8 lut[0x100], int m)
{
	int j;
	for (j = 0x100; --j; )
		lut[j] = ge[gl[m] + gl[j]];
	lut[0] = 0;
}

#define MT(i,j)     (mt[((i) * N) + (j)])
#define IMT(i,j)   (imt[((i) * N) + (j)])
#define MULS(i,j) (muls[((i) * N) + (j)])

int
recreate(xfile_t *in, xfile_t *out)
{
	int i, j, k, l, M, N, R;
	u8 *mt, *imt, *muls;
	u8 buf[0x4000], *work;
	size_t s, size;
	size_t perc;

	ginit();

	/*\ Count number of recovery files \*/
	for (i = N = R = 0; in[i].filenr; i++) {
		if (in[i].files) {
			R++;
			/*\ Get max. matrix row size \*/
			for (k = 0; in[i].files[k]; k++) {
				if (in[i].files[k] > N)
					N = in[i].files[k];
			}
		} else {
			if (in[i].filenr > N)
				N = in[i].filenr;
		}
	}

	/*\ Count number of volumes to output \*/
	for (i = j = M = 0; out[i].filenr; i++) {
		M++;
		if (out[i].files) {
			j++;
			/*\ Get max. matrix row size \*/
			for (k = 0; out[i].files[k]; k++) {
				if (out[i].files[k] > N)
					N = out[i].files[k];
			}
		} else {
			if (out[i].filenr > N)
				N = out[i].filenr;
		}
	}
	R += j;
	N += j;

	CNEW(mt, R * N);
	CNEW(imt, R * N);

	/*\ Fill in matrix rows for recovery files \*/
	for (i = j = 0; in[i].filenr; i++) {
		if (!in[i].files)
			continue;
		for (k = 0; in[i].files[k]; k++)
			MT(j, in[i].files[k]-1) = gpow(k+1, in[i].filenr - 1);
		IMT(j, i) = 1;
		j++;
	}

	/*\ Fill in matrix rows for output recovery files \*/
	for (i = 0, l = N; out[i].filenr; i++) {
		if (!out[i].files)
			continue;
		for (k = 0; out[i].files[k]; k++)
			MT(j, out[i].files[k]-1) = gpow(k+1, out[i].filenr - 1);
		--l;
		/*\ Fudge filenr \*/
		out[i].filenr = l + 1;
		MT(j, l) = 1;
		j++;
	}

	/*\ Use (virtual) rows from data files to eliminate columns \*/
	for (i = 0; in[i].filenr; i++) {
		if (in[i].files)
			continue;
		k = in[i].filenr - 1;
		/*\ MT would have a 1 at (i, k), IMT a 1 at (i, i)
		|*| IMT(j,i) -= MT(j,k) * IMT(i,i)  (is MT(j, k))
		|*|  MT(j,k) -= MT(j,k) *  MT(i,k)  (becomes 0)
		\*/
		for (j = 0; j < R; j++) {
			IMT(j, i) ^= MT(j, k);
			MT(j, k) = 0;
		}
	}

	/*\ Eliminate columns using the remaining rows, so we get I.
	|*| The accompanying matrix will be the inverse
	\*/
	for (i = 0; i < R; i++) {
		int d, l;
		/*\ Find first non-zero entry \*/
		for (l = 0; (l < N) && !MT(i, l); l++)
			;
		d = MT(i, l);
		if (!d) continue;
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
	CNEW(muls, M * N);

	for (i = 0; out[i].filenr; i++) {
		/*\ File #x: The row IMT(j) for which MT(j,x) = 1 \*/
		for (j = 0; j < R; j++) {
			k = out[i].filenr - 1;
			if (MT(j, k) != 1)
				continue;
			/*\ All other values should be 0 \*/
			for (k = 0; !MT(j, k); k++)
				;
			if (k != out[i].filenr - 1)
				continue;
			for (k++; (k < N) && !MT(j, k); k++)
				;
			if (k != N)
				continue;
			break;
		}
		/*\ Did we find a suitable row ? \*/
		if (j == R)
			continue;
		for (k = 0; k < N; k++)
			MULS(i, k) = IMT(j, k);
	}
	free(mt);
	free(imt);

	/*\ Restore all the files at once \*/
	NEW(work, sizeof(buf) * M);

	/*\ Check for rows and columns with all-zeroes \*/
	for (i = 0; i < M; i++) {
		for (j = 0; j < N; j++)
			if (MULS(i, j))
				break;
		if (j == N)
			in[i].size = 0;
	}

	/*\ Check for rows and columns with all-zeroes \*/
	for (j = 0; j < N; j++) {
		for (i = 0; i < M; j++)
			if (MULS(i, j))
				break;
		if (i == M)
			out[j].size = 0;
	}

	/*\ Find out how much we should process in total \*/
	size = 0;
	for (i = 0; out[i].filenr; i++)
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
		for (i = 0; in[i].filenr; i++) {
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
			for (j = 0; out[j].filenr; j++) {
				u8 lut[0x100];
				if (s >= out[j].size) continue;
				if (!MULS(j, i)) continue;
				/*\ Precalc LUT \*/
				make_lut(lut, MULS(j, i));
				p = work + (j * sizeof(buf));
				/*\ XOR it in, passed through the LUTs \*/
				for (q = r; --q >= 0; )
					p[q] ^= lut[buf[q]];
			}
		}
		for (j = 0; out[j].filenr; j++) {
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
