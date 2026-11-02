/* Øving 6 - utpakking
   Foreløpig gjør den ingen utpakking, bare kopierer fila.
   Hensikten er å få fil-inn og fil-ut til å fungere feilfritt først. */

#include <stdio.h>
#include <stdlib.h>

/* Leser hele fila inn i en nyallokert buffer.
   Returnerer bufferet, og legger antall byte i *lengde.
   Returnerer NULL ved feil.                                          */
static unsigned char *lesFil(const char *navn, long *lengde) {
    FILE *f = fopen(navn, "rb");
    if (f == NULL) {
        printf("Fikk ikke aapnet %s\n", navn);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    *lengde = ftell(f);
    fseek(f, 0, SEEK_SET);

    unsigned char *data = malloc(*lengde);
    if (data == NULL) {
        printf("Fikk ikke allokert %ld byte\n", *lengde);
        fclose(f);
        return NULL;
    }

    if (fread(data, 1, *lengde, f) != (size_t)*lengde) {
        printf("Fikk ikke lest hele fila\n");
        free(data);
        fclose(f);
        return NULL;
    }

    fclose(f);
    return data;
}

/* Skriver lengde byte fra data til fil. Returnerer 1 ved suksess. */
static int skrivFil(const char *navn, const unsigned char *data, long lengde) {
    FILE *f = fopen(navn, "wb");
    if (f == NULL) {
        printf("Fikk ikke aapnet %s for skriving\n", navn);
        return 0;
    }
    if (fwrite(data, 1, lengde, f) != (size_t)lengde) {
        printf("Fikk ikke skrevet hele fila\n");
        fclose(f);
        return 0;
    }
    fclose(f);
    return 1;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Bruk: %s <komprimert fil> <utfil>\n", argv[0]);
        return 1;
    }

    long lengde;
    unsigned char *data = lesFil(argv[1], &lengde);
    if (data == NULL) return 1;

    /* ---- Her kommer utpakkingen senere ---- */

    if (!skrivFil(argv[2], data, lengde)) {
        free(data);
        return 1;
    }

    printf("%s: %ld byte  ->  %s: %ld byte\n",
           argv[1], lengde, argv[2], lengde);

    free(data);
    return 0;
}