# Rapport Øving 3 IDATT2101

Navn: Alva K. Leiner, Ida Holt-Francati, Dara C. Langved, Nivisa Ratheekanthan

## Alternativ 1: sammenligne ulike typer quicksort

Oppgaven går ut på å sammenligne vanlig quicksort, som bruker ett delingstall, med dual pivot quicksort, som bruker to. Begge er implementert som funksjoner i samme testprogram, og det er gjort tidsmålinger på fire ulike datamønstre med 50 millioner tall.

## Korrekthetstesting

Hver sortering kontrolleres med to tester. Begge ligger utenfor tidtakingen, siden de i seg selv er fulle gjennomløp av tabellen.

Sjekksummen beregnes før og etter sortering. Sortering skal bare flytte tall, aldri lage eller ødelegge en verdi, og summen må derfor være uendret. Endrer den seg, har koden en feil hvor tabellposisjoner overskrives og tall går tapt. Summen lagres i `long long`; 50 millioner `int`-er gir en sum i størrelsesorden $5 \cdot 10^{13}$, mens en 32-bits `int` stopper på omtrent $2{,}1 \cdot 10^9$.

Rekkefølgetesten kontrollerer at `t[i+1] >= t[i]` for alle i fra 0 til n-2. Det holder å sammenligne nabopar fordi mindre-enn er transitiv: står hvert element riktig i forhold til det neste, følger resten.

De to testene fanger forskjellige feil og er utilstrekkelige hver for seg. En sortering med en feil som fyller hele tabellen med samme verdi ville bestått rekkefølgetesten, men strøket på sjekksummen. En tabell som ikke er rørt består sjekksummen, men ikke rekkefølgetesten.

## Single-pivot quicksort

Implementasjonen er hentet fra læreboka, med `median3sort`, `splitt` og `quicksort`.

Kjøretiden avhenger av hvor jevnt tabellen deles. Ved perfekt deling gjøres $\Theta(n)$ arbeid i splitt, og problemet deles i to like store deler:

$$
T(n) = 2T(n/2) + \Theta(n)
$$

Dette gir $T(n) = \Theta(n \log n)$. Ved maksimalt skjev deling, altså der delingstallet er det minste eller største tallet, blir den ene delen tom:

$$
T(n) = T(n-1) + \Theta(n) = \Theta(n^2)
$$

To detaljer i lærebokas kode er avgjørende for at det verste tilfellet unngås i praksis.

Valget av median blant tre tall som delingsverdi gjør at en sortert tabell deles nøyaktig på midten. Med `t[v]` som delingstall ville en sortert tabell gitt det minste tallet ved hvert kall, altså $\Theta(n^2)$.

De indre løkkene bruker `<` og `>` fremfor `<=` og `>=`. Løkkene stopper dermed på tall som er like delingsverdien. Dette koster noen unødvendige ombyttinger av like tall, men gjør at en deltabell hvor alle tall er like deles perfekt på midten. Med `<=` og `>=` ville begge løkkene gått til enden av deltabellen, og gitt maksimalt skjev deling på data med mange duplikater.

Kildekode fra splitt:
```c
    while (t[++iv] < dv);
    while (t[--ih] > dv);
```

## Dual pivot quicksort

Implementasjonen er basert på https://www.geeksforgeeks.org/dsa/dual-pivot-quicksort/. Algoritmen deler tabellen i tre deler i stedet for to, som gir rekurrensen

$$
T(n) = 3T(n/3) + \Theta(n)
$$

Dette er også $\Theta(n \log n)$, men med grunnere rekursjon: dybden blir $\log_3 n$ fremfor $\log_2 n$.

Algoritmen fra kilden har to svakheter som måtte rettes.

Den velger første og siste arrayposisjon som venstre og høyre delingstall. Er tabellen sortert fra før, er dette det minste og det største tallet i tabellen, og samtlige øvrige tall havner i det midterste intervallet. Delingen blir da $n \rightarrow (0, n-2, 0)$, altså maksimalt skjev, og kjøretiden $\Theta(n^2)$. Kilden oppgir selv at dette er tilfellet. Svakheten rettes ved å hente delingstallene en tredjedel inn fra hver ende før de velges.

Kildekode fra fiks 1:
```c
    int tredjedel = (h - v) / 3;
    bytt(t, v, v + tredjedel);
    bytt(t, h, h - tredjedel);
```

Den andre svakheten gjelder tabeller med mange duplikater. Midtintervallet inneholder per definisjon tallene mellom de to delingstallene. Er delingstallene like, finnes det ingen verdi som ligger strengt mellom dem, og alt i midtintervallet må være lik dem. Den delen er dermed ferdig sortert, og det rekursive kallet på midtintervallet kan utelates.

Kildekode fra fiks 2:
```c
    if (t[vp] != t[hp])
        dualPivotQuicksort(t, vp + 1, hp - 1);
```

## Tidsmålingene

Målingene er gjort på tabeller med 50 millioner `int`, kompilert med `gcc -O2`. Tidtakingen bruker `clock_gettime` med `CLOCK_MONOTONIC`.

Hver måling gjøres på en tabell som fylles på nytt. Repetisjoner kan ikke brukes her, slik det ble gjort i forrige øving, fordi tabellen er sortert etter første sortering, og en gjentatt måling ville målt et vesentlig lettere problem. Datamengden er til gjengjeld stor nok til at én enkelt sortering gir tilstrekkelig måletid.

De tilfeldige tallene genereres med xorshift32 fremfor `rand()`. Xorshift er raskere, noe som merkes når 50 millioner posisjoner skal fylles, og den er deterministisk, slik at målingene kan gjentas med samme datagrunnlag.

| Datasett | Single-pivot (s) | Dual pivot (s) |
|---|---:|---:|
| Tilfeldig | 3,279 | 3,151 |
| Duplikater | 1,939 | 1,685 |
| Sortert | 0,429 | 0,396 |
| Baklengs sortert | 0,652 | 0,408 |

Alle målingene besto sjekksumtesten og rekkefølgetesten.

Dual pivot quicksort er raskest på alle fire datasettene, men forskjellen varierer betydelig mellom datamønstrene. På tilfeldige data er de to nesten like raske, og forskjellen er så liten at den ligger nær variasjonen mellom kjøringer. Resultatet for dette datasettet bør derfor tillegges mindre vekt enn de øvrige. På baklengs sorterte data er forskjellen klart størst.

Den største forskjellen mellom de to algoritmene er hvordan de håndterer baklengs sorterte data. Dual pivot bruker omtrent like lang tid på baklengs sortert som på sortert, 0,408 mot 0,396 sekunder, mens single-pivot bruker halvannen gang så lang tid på baklengs som på sortert, 0,652 mot 0,429 sekunder.

For begge algoritmene er sorterte data det raskeste tilfellet, ikke det tregeste. Det bekrefter at valget av delingstall fungerer som tiltenkt i begge implementasjonene. Uten median av tre, henholdsvis uten fiks 1, ville dette datasettet gitt kvadratisk kjøretid.

Begge algoritmene sorterer data med mange duplikater raskere enn tilfeldige data, som forventet når antallet ulike verdier er lavere. Dual pivot har noe større utbytte av dette enn single-pivot.

At dual pivot vinner samlet, stemmer med at tredeling gir grunnere rekursjon enn todeling. Forskjellen på tilfeldige data er likevel beskjeden, noe som tyder på at gevinsten et stykke på vei motvirkes av at hvert nivå koster mer: to delingstall gir flere sammenligninger per element.