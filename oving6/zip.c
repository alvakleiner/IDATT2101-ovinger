/* Huffman compression. Usage: zip <infile> <outfile> */

#include <stdio.h>
#include <stdlib.h>

#define MAX_NODES 511

typedef struct {
    long weight;
    int left;
    int right;
    int symbol;
} Node;

static unsigned char *readFile(const char *name, long *length) {
    FILE *f = fopen(name, "rb");
    if (f == NULL) {
        printf("Could not open %s\n", name);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    *length = ftell(f);
    fseek(f, 0, SEEK_SET);

    unsigned char *data = malloc(*length);
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
    if (fwrite(data, 1, length, f) != (size_t)length) {
        printf("Could not write entire file\n");
        fclose(f);
        return 0;
    }
    fclose(f);
    return 1;
}

/* Huffman tree construction: repeatedly merge the two lightest nodes.
   Returns the index of the root. */
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

    long sum = 0;
    int distinct = 0;
    for (int i = 0; i < 256; i++) {
        sum += frequency[i];
        if (frequency[i] > 0) distinct++;
    }
    printf("Frequency sum: %ld (length: %ld)  %s\n",
           sum, length, (sum == length) ? "OK" : "FAIL");
    printf("Distinct byte values: %d of 256\n", distinct);

    /* Step 2: build the Huffman tree */
    Node node[MAX_NODES];
    int root = buildTree(frequency, node);

    printf("Root weight: %ld (length: %ld)  %s\n",
           node[root].weight, length,
           (node[root].weight == length) ? "OK" : "FAIL");

    if (!writeFile(argv[2], data, length)) {
        free(data);
        return 1;
    }

    printf("%s: %ld bytes  ->  %s: %ld bytes\n",
           argv[1], length, argv[2], length);

    free(data);
    return 0;
}