#include <stdio.h>      // printf
#include <stdlib.h>     // malloc, free
#include <time.h>       // clock_gettime, struct timespec

// --- FELLES GODKJENNINGSKRAV ---
// Sjekksum
static long long sjekksum(const int *t, int n) {
    long long sum = 0;
    for (int i = 0; i < n; i++) sum += t[i];
    return sum;
}

// Rekkefølgetest
static int erSortert(const int *t, int n) {
    for (int i = 0; i < n - 1; i++)
        // snur t[i+1] >= t[i] for å finne moteksempel
        if (t[i + 1] < t[i]) {
            printf("  FEIL: t[%d]=%d < t[%d]=%d\n", i + 1, t[i + 1], i, t[i]);
            return 0;
        }
    return 1;
}


// --- ALTERNATIV 1 - SAMMENLIGNE ULIKE TYPER QUICKSORT ---

// HJELPEFUNKSJONER
// Tidtaking
static double tidNaa(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

// Bytte to elementer i tabell
static void bytt(int *t, int i, int j) {
    int temp = t[i];
    t[i] = t[j];
    t[j] = temp;
}

// Tilfeldig tallgenerator (Xorshift32)
static unsigned int rngTilstand = 123456789u;

static unsigned int neste(void) {
    rngTilstand ^= rngTilstand << 13;
    rngTilstand ^= rngTilstand >> 17;
    rngTilstand ^= rngTilstand << 5;
    return rngTilstand;
}


// TABELLER
// Tabell med tilfeldige tall
static void fyllTilfeldig(int *t, int n) {
    for (int i = 0; i < n; i++) t[i] = (int)(neste() % 1000000);
}

// Tabell med mange duplikater
static void fyllDuplikater(int *t, int n) {
    for (int i = 0; i < n; i++)
        t[i] = (i % 2 == 0) ? 42 : (int)(neste() % 1000000);
}

// Tabell som er sortert fra før
static void fyllSortert(int *t, int n) {
    for (int i = 0; i < n; i++) t[i] = i;
}

// Tabell som er baklengs sortert
static void fyllBaklengs(int *t, int n) {
    for (int i = 0; i < n; i++) t[i] = n - i;
}


// SINGLE-PIVOT QUICKSORT (fra læreboka)
static int median3sort(int *t, int v, int h) {
    int m = (v + h) / 2;
    if (t[v] > t[m]) bytt(t, v, m);
    if (t[m] > t[h]) {
        bytt(t, m, h);
        if (t[v] > t[m]) bytt(t, v, m);
    }
    return m;
}

static int splitt(int *t, int v, int h) {
    int iv, ih;
    int m = median3sort(t, v, h);
    int dv = t[m];
    bytt(t, m, h - 1);

    for (iv = v, ih = h - 1;;) {
        while (t[++iv] < dv);
        while (t[--ih] > dv);
        if (iv >= ih) break;
        bytt(t, iv, ih);
    }
    bytt(t, iv, h - 1);
    return iv;
}

static void quicksort(int *t, int v, int h) {
    if (h - v > 2) {
        int delepos = splitt(t, v, h);
        quicksort(t, v, delepos - 1);
        quicksort(t, delepos + 1, h);
    } else median3sort(t, v, h);
}


// DUAL-PIVOT QUICKSORT (fra geeksforgeeks + to endringer)

static int dpSplitt(int *t, int v, int h, int *vp) {

    // FIKS 1: originalen velger t[v] og t[h] direkte som pivoter. På en sortert tabell blir de da 
    // det minste og det største tallet, alle andre havner i midtintervallet, og delingen blir maksimalt 
    // skjev -- O(n^2). Ved å hente pivotene en tredjedel inn fra hver ende unngår vi det.
    int tredjedel = (h - v) / 3;
    bytt(t, v, v + tredjedel);
    bytt(t, h, h - tredjedel);

    if (t[v] > t[h]) bytt(t, v, h);

    int p = t[v], q = t[h];
    int j = v + 1;
    int k = v + 1;
    int g = h - 1;

    while (k <= g) {
        if (t[k] < p) {
            bytt(t, k, j);
            j++;
        } else if (t[k] >= q) {
            while (t[g] > q && k < g) g--;
            bytt(t, k, g);
            g--;
            if (t[k] < p) {
                bytt(t, k, j);
                j++;
            }
        }
        k++;
    }
    j--;
    g++;

    bytt(t, v, j);
    bytt(t, h, g);

    *vp = j;
    return g;
}

static void dualPivotQuicksort(int *t, int v, int h) {
    if (v < h) {
        int vp, hp;
        hp = dpSplitt(t, v, h, &vp);

        dualPivotQuicksort(t, v, vp - 1);

        // FIKS 2: er de to pivotene like, må alt i midtintervallet også være lik dem, og den 
        // delen er dermed ferdig sortert. Uten denne testen sorterer originalen et intervall 
        // som allerede er ferdig, gang på gang.
        if (t[vp] != t[hp])
            dualPivotQuicksort(t, vp + 1, hp - 1);

        dualPivotQuicksort(t, hp + 1, h);
    }
}


// TIDSMÅLINGER

// Kjører én sortering på ett datasett og kontrollerer resultatet. Tabellen fylles på nytt for hver 
// måling: etter en sortering er den jo sortert, og en ny måling på samme data ville målt noe helt annet.
static void maal(const char *navn, const char *datasett, void (*sorter)(int *, int, int),
                 void (*fyll)(int *, int), int *t, int n) {

    fyll(t, n);
    long long foer = sjekksum(t, n);

    double start = tidNaa();
    sorter(t, 0, n - 1);
    double tid = tidNaa() - start;

    long long etter = sjekksum(t, n);
    int sortert = erSortert(t, n);

    printf("%-14s %-12s %8.3f s   sjekksum: %-4s  rekkefølge: %s\n",
           navn, datasett, tid,
           (foer == etter) ? "OK" : "FEIL",
           sortert ? "OK" : "FEIL");
}

int main(void) {
    int n = 50000000;

    int *t = malloc((size_t)n * sizeof(int));
    if (t == NULL) {
        printf("Fikk ikke allokert minne til %d tall\n", n);
        return 1;
    }

    printf("Antall tall: %d\n\n", n);

    maal("single-pivot", "tilfeldig",  quicksort, fyllTilfeldig, t, n);
    maal("single-pivot", "duplikater", quicksort, fyllDuplikater, t, n);
    maal("single-pivot", "sortert",    quicksort, fyllSortert,   t, n);
    maal("single-pivot", "baklengs",   quicksort, fyllBaklengs,  t, n);
    printf("\n");
    maal("dual-pivot",   "tilfeldig",  dualPivotQuicksort, fyllTilfeldig, t, n);
    maal("dual-pivot",   "duplikater", dualPivotQuicksort, fyllDuplikater, t, n);
    maal("dual-pivot",   "sortert",    dualPivotQuicksort, fyllSortert,   t, n);
    maal("dual-pivot",   "baklengs",   dualPivotQuicksort, fyllBaklengs,  t, n);

    free(t);
    return 0;
}