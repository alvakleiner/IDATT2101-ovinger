/* BWT + MTF + RLE + Huffman decompression. Usage: unzip <infile> <outfile> */

#include <stdio.h>
#include <stdlib.h>

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
    const unsigned char *buffer;
    long position;
    int bitCount;
} BitReader;

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

/* Huffman tree: repeatedly merge the two lightest nodes. Must stay identical to the copy in zip.c. */
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

static int readBit(BitReader *r) {
    int bit = (r->buffer[r->position] >> (7 - r->bitCount)) & 1;
    if (++r->bitCount == 8) {
        r->bitCount = 0;
        r->position++;
    }
    return bit;
}

/* Expands zero runs written in bijective base 2 back into zeros. */
static long rleInverse(const int *in, long n, unsigned char *out) {
    long count = 0;
    for (long i = 0; i < n; i++) {
        if (in[i] == RUNA || in[i] == RUNB) {
            long length = 0, mult = 1;
            while (i < n && (in[i] == RUNA || in[i] == RUNB)) {
                length += (in[i] == RUNA ? 1 : 2) * mult;
                mult *= 2;
                i++;
            }
            i--;
            while (length-- > 0) out[count++] = 0;
        } else {
            out[count++] = (unsigned char)(in[i] - 1);
        }
    }
    return count;
}

/* Inverse move-to-front. */
static void mtfInverse(const unsigned char *in, long n, unsigned char *out) {
    unsigned char list[256];
    for (int i = 0; i < 256; i++) list[i] = (unsigned char)i;

    for (long i = 0; i < n; i++) {
        int j = in[i];
        unsigned char c = list[j];
        out[i] = c;
        for (int k = j; k > 0; k--) list[k] = list[k - 1];
        list[0] = c;
    }
}

/* Inverse Burrows-Wheeler transform. */
static void bwtInverse(const unsigned char *in, long n, long origPtr, unsigned char *out) {
    long count[257] = {0};
    for (long i = 0; i < n; i++) count[in[i] + 1]++;
    for (int i = 1; i < 257; i++) count[i] += count[i - 1];

    long *next = malloc(n * sizeof(long));
    if (next == NULL) return;
    for (long i = 0; i < n; i++) next[count[in[i]]++] = i;

    long p = next[origPtr];
    for (long i = 0; i < n; i++) {
        out[i] = in[p];
        p = next[p];
    }
    free(next);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <infile> <outfile>\n", argv[0]);
        return 1;
    }

    long length;
    unsigned char *data = readFile(argv[1], &length);
    if (data == NULL) return 1;

    if (length < HEADER_BYTES + TABLE_BYTES) {
        printf("File is too short to contain a header\n");
        free(data);
        return 1;
    }

    long origPtr = (long)data[0] | ((long)data[1] << 8)
                 | ((long)data[2] << 16) | ((long)data[3] << 24);
    long blockLength = (long)data[4] | ((long)data[5] << 8)
                     | ((long)data[6] << 16) | ((long)data[7] << 24);

    long frequency[ALPHABET] = {0};
    long sum = 0;
    long pos = HEADER_BYTES;
    for (int i = 0; i < ALPHABET; i++) {
        unsigned int fr = (unsigned int)data[pos]
                        | ((unsigned int)data[pos + 1] << 8)
                        | ((unsigned int)data[pos + 2] << 16)
                        | ((unsigned int)data[pos + 3] << 24);
        pos += 4;
        frequency[i] = fr;
        sum += fr;
    }

    if (sum == 0) {
        int ok = writeFile(argv[2], data, 0);
        free(data);
        if (!ok) return 1;
        printf("%s: %ld bytes  ->  %s: 0 bytes\n", argv[1], length, argv[2]);
        return 0;
    }

    if (origPtr < 0 || origPtr >= blockLength) {
        printf("Corrupt header: rotation index out of range\n");
        free(data);
        return 1;
    }

    Node node[MAX_NODES];
    int root = buildTree(frequency, node);

    int *rle = malloc(sum * sizeof(int));
    unsigned char *mtf = malloc(blockLength);
    unsigned char *stage = malloc(blockLength);
    unsigned char *out = malloc(blockLength);
    if (rle == NULL || mtf == NULL || stage == NULL || out == NULL) {
        printf("Out of memory\n");
        free(rle);
        free(mtf);
        free(stage);
        free(out);
        free(data);
        return 1;
    }

    /* Huffman */
    BitReader r = { data + HEADER_BYTES + TABLE_BYTES, 0, 0 };
    for (long i = 0; i < sum; i++) {
        int n = root;
        while (node[n].left != -1)
            n = readBit(&r) ? node[n].right : node[n].left;
        rle[i] = node[n].symbol;
    }

    /* RLE */
    long mtfCount = rleInverse(rle, sum, mtf);
    if (mtfCount != blockLength) {
        printf("Length mismatch: expanded %ld, expected %ld\n", mtfCount, blockLength);
        free(rle);
        free(mtf);
        free(stage);
        free(out);
        free(data);
        return 1;
    }

    /* MTF */
    mtfInverse(mtf, blockLength, stage);

    /* BWT */
    bwtInverse(stage, blockLength, origPtr, out);

    if (!writeFile(argv[2], out, blockLength)) {
        free(rle);
        free(mtf);
        free(stage);
        free(out);
        free(data);
        return 1;
    }

    printf("%s: %ld bytes  ->  %s: %ld bytes\n", argv[1], length, argv[2], blockLength);

    free(rle);
    free(mtf);
    free(stage);
    free(out);
    free(data);
    return 0;
}