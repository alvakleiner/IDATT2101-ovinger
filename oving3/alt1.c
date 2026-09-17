// FELLES GODKJENNINGSKRAV
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

