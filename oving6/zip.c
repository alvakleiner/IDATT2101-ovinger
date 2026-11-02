/* BWT + MTF + RLE + Huffman compression. Usage: zip <infile> <outfile>

   Stage by stage on diverse.lyx: Huffman alone 61.0%, adding BWT and MTF 16.0%, 
   adding RLE (with BWT and MTF) 5.4%. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALPHABET 257
#define MAX_NODES (2 * ALPHABET - 1)
#define TABLE_BYTES (ALPHABET * 4)
#define HEADER_BYTES 8
#define RUNA 0
#define RUNB 1

typedef struct {
    long weight;
    int left;
    int right;
    int symbol;
} Node;

typedef struct {
    unsigned long long bits;
    int length;
} Code;

typedef struct {
    unsigned char *buffer;
    long position;
    unsigned char current;
    int bitCount;
} BitWriter;

static unsigned char *readFile(const char *name, long *length) {
    FILE *f = fopen(name, "rb");
    if (f == NULL) {
        printf("Could not open %s\n", name);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    *length = ftell(f);
    fseek(f, 0, SEEK_SET);

    unsigned char *data = malloc(*length + 1);
    if (data == NULL) {
        printf("Could not allocate %ld bytes\n", *length);
        fclose(f);
        return NULL;
    }

    if (fread(data, 1, *length, f) != (size_t)*length) {
        printf("Could not read entire file\n");
        free(data);
        fclose(f);
        return NULL;
    }

    fclose(f);
    return data;
}

static int writeFile(const char *name, const unsigned char *data, long length) {
    FILE *f = fopen(name, "wb");
    if (f == NULL) {
        printf("Could not open %s for writing\n", name);
        return 0;
    }
    if (length > 0 && fwrite(data, 1, length, f) != (size_t)length) {
        printf("Could not write entire file\n");
        fclose(f);
        return 0;
    }
    fclose(f);
    return 1;
}

static int *gRank;
static long gK, gN;

static int compareRotations(const void *pa, const void *pb) {
    long a = *(const long *)pa, b = *(const long *)pb;
    if (gRank[a] != gRank[b]) return gRank[a] < gRank[b] ? -1 : 1;
    int ra = gRank[(a + gK) % gN], rb = gRank[(b + gK) % gN];
    return ra < rb ? -1 : (ra > rb ? 1 : 0);
}

/* Burrows-Wheeler transform, rotations sorted by prefix doubling.
   Returns the index of the unrotated block in the sorted order. */
static long bwtForward(const unsigned char *in, long n, unsigned char *out) {
    long *sa = malloc(n * sizeof(long));
    int *rank = malloc(n * sizeof(int));
    int *tmp = malloc(n * sizeof(int));
    if (sa == NULL || rank == NULL || tmp == NULL) {
        free(sa);
        free(rank);
        free(tmp);
        return -1;
    }

    for (long i = 0; i < n; i++) {
        sa[i] = i;
        rank[i] = in[i];
    }
    gRank = rank;
    gN = n;

    for (long k = 1; k < n; k *= 2) {
        gK = k;
        qsort(sa, n, sizeof(long), compareRotations);
        tmp[sa[0]] = 0;
        for (long i = 1; i < n; i++)
            tmp[sa[i]] = tmp[sa[i - 1]] + (compareRotations(&sa[i - 1], &sa[i]) != 0 ? 1 : 0);
        memcpy(rank, tmp, n * sizeof(int));
        if (rank[sa[n - 1]] == n - 1) break;
    }

    long origPtr = 0;
    for (long i = 0; i < n; i++) {
        if (sa[i] == 0) origPtr = i;
        out[i] = in[(sa[i] + n - 1) % n];
    }

    free(sa);
    free(rank);
    free(tmp);
    return origPtr;
}

/* Move-to-front transform. */
static void mtfForward(const unsigned char *in, long n, unsigned char *out) {
    unsigned char list[256];
    for (int i = 0; i < 256; i++) list[i] = (unsigned char)i;

    for (long i = 0; i < n; i++) {
        int j = 0;
        while (list[j] != in[i]) j++;
        out[i] = (unsigned char)j;
        for (int k = j; k > 0; k--) list[k] = list[k - 1];
        list[0] = in[i];
    }
}

/* Run-length encoding of zero runs. A run of n zeros becomes n written in bijective base 2,
   with RUNA as digit 1 and RUNB as digit 2. Other values shift up by one to make room. */
static long rleForward(const unsigned char *in, long n, int *out) {
    long count = 0, zeros = 0;
    for (long i = 0; i <= n; i++) {
        if (i < n && in[i] == 0) { zeros++; continue; }
        while (zeros > 0) {
            if (zeros % 2 == 1) { out[count++] = RUNA; zeros = (zeros - 1) / 2; }
            else                { out[count++] = RUNB; zeros = (zeros - 2) / 2; }
        }
        if (i < n) out[count++] = in[i] + 1;
    }
    return count;
}

/* Huffman tree: repeatedly merge the two lightest nodes. Must stay identical to the copy in unzip.c. */
static int buildTree(const long *frequency, Node *node) {
    int count = 0;
    int active[ALPHABET];
    int activeCount = 0;

    for (int i = 0; i < ALPHABET; i++) {
        if (frequency[i] > 0) {
            node[count].weight = frequency[i];
            node[count].left = -1;
            node[count].right = -1;
            node[count].symbol = i;
            active[activeCount++] = count;
            count++;
        }
    }

    while (activeCount > 1) {
        int a = 0, b = 1;
        if (node[active[b]].weight < node[active[a]].weight) { int t = a; a = b; b = t; }
        for (int i = 2; i < activeCount; i++) {
            if (node[active[i]].weight < node[active[a]].weight) { b = a; a = i; }
            else if (node[active[i]].weight < node[active[b]].weight) { b = i; }
        }

        node[count].weight = node[active[a]].weight + node[active[b]].weight;
        node[count].left = active[a];
        node[count].right = active[b];
        node[count].symbol = -1;

        active[a] = count;
        active[b] = active[activeCount - 1];
        activeCount--;
        count++;
    }

    return active[0];
}

/* Left branch appends 0, right appends 1. */
static void buildCodes(const Node *node, int i, unsigned long long bits, int depth, Code *code) {
    if (node[i].left == -1) {
        code[node[i].symbol].bits = bits;
        code[node[i].symbol].length = depth;
        return;
    }
    buildCodes(node, node[i].left,  bits << 1,       depth + 1, code);
    buildCodes(node, node[i].right, (bits << 1) | 1, depth + 1, code);
}

static void writeBits(BitWriter *w, unsigned long long bits, int length) {
    for (int i = length - 1; i >= 0; i--) {
        w->current = (unsigned char)((w->current << 1) | ((bits >> i) & 1));
        if (++w->bitCount == 8) {
            w->buffer[w->position++] = w->current;
            w->current = 0;
            w->bitCount = 0;
        }
    }
}

static void flushBits(BitWriter *w) {
    if (w->bitCount > 0) {
        w->current = (unsigned char)(w->current << (8 - w->bitCount));
        w->buffer[w->position++] = w->current;
        w->current = 0;
        w->bitCount = 0;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <infile> <outfile>\n", argv[0]);
        return 1;
    }

    long length;
    unsigned char *data = readFile(argv[1], &length);
    if (data == NULL) return 1;

    if (length == 0) {
        unsigned char empty[HEADER_BYTES + TABLE_BYTES] = {0};
        int ok = writeFile(argv[2], empty, HEADER_BYTES + TABLE_BYTES);
        free(data);
        if (!ok) return 1;
        printf("%s: 0 bytes  ->  %s: %d bytes\n", argv[1], argv[2], HEADER_BYTES + TABLE_BYTES);
        return 0;
    }

    unsigned char *stage = malloc(length);
    unsigned char *mtf = malloc(length);
    if (stage == NULL || mtf == NULL) {
        printf("Out of memory\n");
        free(stage);
        free(mtf);
        free(data);
        return 1;
    }

    /* BWT */
    long origPtr = bwtForward(data, length, stage);
    if (origPtr < 0) {
        printf("BWT failed, out of memory\n");
        free(stage);
        free(mtf);
        free(data);
        return 1;
    }

    /* MTF */
    mtfForward(stage, length, mtf);

    /* RLE */
    int *rle = malloc((2 * length + 2) * sizeof(int));
    if (rle == NULL) {
        printf("Out of memory\n");
        free(stage);
        free(mtf);
        free(data);
        return 1;
    }
    long rleCount = rleForward(mtf, length, rle);

    /* Huffman */
    long frequency[ALPHABET] = {0};
    for (long i = 0; i < rleCount; i++) frequency[rle[i]]++;

    Node node[MAX_NODES];
    int root = buildTree(frequency, node);

    Code code[ALPHABET] = {0};
    buildCodes(node, root, 0, 0, code);

    long bits = 0;
    for (int i = 0; i < ALPHABET; i++) bits += frequency[i] * code[i].length;
    long outLength = HEADER_BYTES + TABLE_BYTES + (bits + 7) / 8;

    unsigned char *out = malloc(outLength + 1);
    if (out == NULL) {
        printf("Could not allocate output buffer\n");
        free(stage);
        free(mtf);
        free(data);
        return 1;
    }

    /* Header: rotation index, block length, then the frequencies, four bytes each,
       least significant byte first. */
    long pos = 0;
    out[pos++] = (unsigned char)(origPtr & 0xFF);
    out[pos++] = (unsigned char)((origPtr >> 8) & 0xFF);
    out[pos++] = (unsigned char)((origPtr >> 16) & 0xFF);
    out[pos++] = (unsigned char)((origPtr >> 24) & 0xFF);
    out[pos++] = (unsigned char)(length & 0xFF);
    out[pos++] = (unsigned char)((length >> 8) & 0xFF);
    out[pos++] = (unsigned char)((length >> 16) & 0xFF);
    out[pos++] = (unsigned char)((length >> 24) & 0xFF);
    for (int i = 0; i < ALPHABET; i++) {
        unsigned int fr = (unsigned int)frequency[i];
        out[pos++] = (unsigned char)(fr & 0xFF);
        out[pos++] = (unsigned char)((fr >> 8) & 0xFF);
        out[pos++] = (unsigned char)((fr >> 16) & 0xFF);
        out[pos++] = (unsigned char)((fr >> 24) & 0xFF);
    }

    BitWriter w = { out + HEADER_BYTES + TABLE_BYTES, 0, 0, 0 };
    for (long i = 0; i < rleCount; i++)
        writeBits(&w, code[rle[i]].bits, code[rle[i]].length);
    flushBits(&w);

    if (HEADER_BYTES + TABLE_BYTES + w.position != outLength) {
        printf("Size mismatch: wrote %ld, expected %ld\n",
               HEADER_BYTES + TABLE_BYTES + w.position, outLength);
        free(out);
        free(stage);
        free(mtf);
        free(data);
        return 1;
    }

    if (!writeFile(argv[2], out, outLength)) {
        free(out);
        free(rle);
        free(stage);
        free(mtf);
        free(data);
        return 1;
    }

    printf("%s: %ld bytes  ->  %s: %ld bytes  (%.1f%%)\n",
           argv[1], length, argv[2], outLength, 100.0 * outLength / length);

    free(out);
    free(rle);
    free(stage);
    free(mtf);
    free(data);
    return 0;
}