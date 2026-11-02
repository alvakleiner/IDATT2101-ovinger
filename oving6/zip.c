/* Huffman compression. Usage: zip <infile> <outfile> */

#include <stdio.h>
#include <stdlib.h>

#define MAX_NODES 511
#define TABLE_BYTES 1024

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

/* Huffman tree construction: repeatedly merge the two lightest nodes. Returns the index of the root.
   Must stay byte-for-byte identical to the copy in unzip.c, or the decoder rebuilds a different tree. */
static int buildTree(const long *frequency, Node *node) {
    int count = 0;
    int active[256];
    int activeCount = 0;

    for (int i = 0; i < 256; i++) {
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

/* Walk the tree and record the bit code for each byte value. Left branch appends 0, right appends 1. */
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

/* Pad the final partial byte with zeros. The decoder stops after the expected number of symbols, 
   so the padding is never decoded. */
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

    /* Step 1: byte frequencies */
    long frequency[256] = {0};
    for (long i = 0; i < length; i++) frequency[data[i]]++;

    /* An empty file has no tree and no codes, so write the all-zero table and stop. */
    if (length == 0) {
        unsigned char empty[TABLE_BYTES] = {0};
        int ok = writeFile(argv[2], empty, TABLE_BYTES);
        free(data);
        if (!ok) return 1;
        printf("%s: 0 bytes  ->  %s: %d bytes\n", argv[1], argv[2], TABLE_BYTES);
        return 0;
    }

    long sum = 0;
    for (int i = 0; i < 256; i++) sum += frequency[i];
    printf("Frequency sum: %ld (length: %ld)  %s\n", sum, length, (sum == length) ? "OK" : "FAIL");

    /* Step 2: build the Huffman tree */
    Node node[MAX_NODES];
    int root = buildTree(frequency, node);
    printf("Root weight: %ld (length: %ld)  %s\n",
           node[root].weight, length, (node[root].weight == length) ? "OK" : "FAIL");

    /* Step 3: derive the code table */
    Code code[256] = {0};
    buildCodes(node, root, 0, 0, code);

    long bits = 0;
    for (int i = 0; i < 256; i++) bits += frequency[i] * code[i].length;
    long outLength = TABLE_BYTES + (bits + 7) / 8;

    unsigned char *out = malloc(outLength);
    if (out == NULL) {
        printf("Could not allocate output buffer\n");
        free(data);
        return 1;
    }

    /* Step 4a: write the frequency table, four bytes per entry, least significant byte first. 
       The decoder rebuilds the same tree from this, and the sum of the frequencies gives the length. */
    long pos = 0;
    for (int i = 0; i < 256; i++) {
        unsigned int fr = (unsigned int)frequency[i];
        out[pos++] = (unsigned char)(fr & 0xFF);
        out[pos++] = (unsigned char)((fr >> 8) & 0xFF);
        out[pos++] = (unsigned char)((fr >> 16) & 0xFF);
        out[pos++] = (unsigned char)((fr >> 24) & 0xFF);
    }

    /* Step 4b: write the codes as packed bits */
    BitWriter w = { out + TABLE_BYTES, 0, 0, 0 };
    for (long i = 0; i < length; i++)
        writeBits(&w, code[data[i]].bits, code[data[i]].length);
    flushBits(&w);

    if (TABLE_BYTES + w.position != outLength) {
        printf("Size mismatch: wrote %ld, expected %ld\n", TABLE_BYTES + w.position, outLength);
        free(out);
        free(data);
        return 1;
    }

    if (!writeFile(argv[2], out, outLength)) {
        free(out);
        free(data);
        return 1;
    }

    printf("%s: %ld bytes  ->  %s: %ld bytes  (%.1f%%)\n",
           argv[1], length, argv[2], outLength, 100.0 * outLength / length);

    free(out);
    free(data);
    return 0;
}