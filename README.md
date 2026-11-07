# random-lottery-numbers

Programs to filter lottery numbers from Brazil.

* Dupla-sena - `duplasena/ds_filter.c`
* Lotofacil  - `lotofacil/lf_filter.c`

## Lotofacil (`lf_filter`)

Generates random Lotofacil combinations (15 numbers out of 1..25), or filters an
existing list of combinations, using statistical criteria (even/odd, sum, repeated
numbers from the previous draw, primes, line/column distribution, etc).

Random combinations are generated with `arc4random_uniform()` (Fisher-Yates shuffle),
so there is no seed and no modulo bias.

### Requirements

* gcc and make
* libconfig (development files)
* glibc >= 2.36 (for `arc4random_uniform()`)

```
# Debian / Ubuntu
sudo apt install build-essential libconfig-dev

# Fedora / RHEL
sudo dnf install gcc make libconfig-devel

# Arch Linux
sudo pacman -S base-devel libconfig
```

### Build

```
cd lotofacil
make            # normal build (-O2)
make release    # stripped binary
make debug      # AddressSanitizer + UBSan, for testing
make clean
```

Without make:

```
gcc -Wall -Wextra -O2 lf_filter.c -o lf_filter -lconfig
```

### Configuration file

With `-n`, the `-a` and `-A` options compare the combinations with the previous draw, which is
read from `lf_filter.conf` in the current directory (the file is read only in this case):

```
ultimo_concurso = {
  numero  = 1780;
  dezenas = [3,4,5,6,7,9,10,11,13,14,15,16,17,22,23];
};
```

### Usage

```
./lf_filter [FILTERS] [-v] [-g] [-t] -n<number of random combinations>
./lf_filter [FILTERS] [-v] [-g] [-t] <matrix file>
```

With `-n`, random combinations are generated (the matrix file is ignored). Without
it, the combinations are read from the matrix file: one combination per line, 15 distinct
numbers (1..25) separated by spaces. The filters are applied to every combination
read from the file.

In a matrix file, the "previous draw" of a line is the line before it (the first
line has 0 repeated numbers), so the order of the lines matters. This is used by
`-a`, `-A` and by the `Rep` field of `-v`; `lf_filter.conf` is not read in this mode.
The previous line is always the previous record of the file, even if it was
discarded by some filter. When `-a` or `-A` is used, the first line is always
discarded (there is no previous draw to compare it with).

Example `matrix.txt`:

```
01 02 03 04 05 06 07 08 09 10 11 12 13 14 15
03 04 05 06 07 09 10 11 13 14 15 16 17 22 23
01 02 04 05 07 08 10 11 13 14 16 17 19 20 22
02 03 05 06 08 09 11 12 14 15 17 18 20 21 23
01 02 03 05 07 09 11 13 15 17 19 21 23 24 25
```

Numbers may have one or two digits (`1` or `01`) and be separated by spaces and/or
tabs; leading zeros are accepted, empty lines are ignored (a line can have up to
255 characters), and the order of the numbers in a line does not matter. A line with a wrong count, a repeated
number or a number outside 1..25 aborts the program with an error.

| Option | Description | Values |
|--------|-------------|--------|
| `-n`   | number of random combinations | 1 ~ 3268760 |
| `-v`   | show the statistics of each combination (no value) | |
| `-g`   | grid output: every number always in the same column | (no value) | 
| `-t`   | totals: summary with averages and percentages at the end | (no value) | 
| `-p`   | minimum number of even numbers | 2 ~ 12 |
| `-P`   | maximum number of even numbers (`-p7 -P7` = exactly 7) | 2 ~ 12 |
| `-a`   | minimum number of numbers repeated from the previous draw (with `-n`: from `lf_filter.conf`; with a matrix file: from the previous line) | 4 ~ 14 |
| `-A`   | maximum number of numbers repeated from the previous draw (`-a9 -A9` = exactly 9) | 4 ~ 14 |
| `-i`   | minimum sum | 120 ~ 270 |
| `-x`   | maximum sum | 120 ~ 270 |
| `-f`   | fixed number (repeatable) | 1 ~ 25, up to 14 |
| `-d`   | discarded number (repeatable) | 1 ~ 25, up to 9 |
| `-l`   | distribution by rows | 43332, 44322, 44331, 53322, 54321 |
| `-c`   | distribution by columns | 43332, 44322, 44331, 53322, 54321 |
| `-s`   | minimum sequence of drawn numbers | 2 ~ 15 |
| `-e`   | minimum sequence of non-drawn numbers | 2 ~ 10 |
| `-r`   | minimum number of primes | 1 ~ 9 |
| `-m`   | maximum number of primes (`-m0` = no primes) | 0 ~ 9 |

If several values are given for `-l` or `-c`, a combination is kept
if it matches **any** of them. All the other filters must always match. `-r`
and `-m` can be used at most once each, and `-r` cannot be greater than `-m`. The same
holds for `-p`/`-P` and `-a`/`-A`. Note that two draws always repeat at least 5 numbers, so
`-A4` can never match.
`-f` and `-d` are checked against `-p`/`-P` (for example, more fixed even numbers than `-P`
allows is an error).

Run without arguments to see the full help.

### Examples

```
# 10 random combinations: exactly 7 even numbers, previous-draw repeats = 9, sum 184..199
./lf_filter -p7 -P7 -a9 -A9 -i184 -x199 -n10

# 5 combinations that contain 1 and 25 and never contain 13
./lf_filter -f1 -f25 -d13 -n5

# between 4 and 6 primes
./lf_filter -r4 -m6 -n10

# filter an existing list
./lf_filter -p7 -P7 -r5 matrix.txt

# same, showing the statistics of each combination
./lf_filter -v -p7 -P7 -r5 matrix.txt

# only the statistics, without filtering anything
./lf_filter -v matrix.txt

# only normalize a matrix file to the grid layout
./lf_filter -g matrix.txt > grid.txt

# statistics of a matrix file (list with statistics + summary at the end)
./lf_filter -v -t matrix.txt
```

### Output

One combination per line, in ascending order:

```
02 03 04 06 07 10 11 13 16 17 18 19 21 24 25
```

With `-g`, each number always has its own column (`NN` plus one space, 25 slots),
and numbers that were not drawn are left blank. Without `-v` each line ends at its
last number; with `-v` the lines are padded to the same length (74 characters) so
the statistics stay aligned. The output of `-g` can be read back as a matrix file.

```
01          05 06 07 08 09    11 12 13 14 15 16                22    24 25
   02 03    05 06             11 12 13 14 15 16             21 22 23 24 25
01          05 06       09 10    12    14       17 18 19 20 21 22 23    25
```

With `-t`, a summary of the combinations that were printed is shown after the list
(here with 40 combinations):

```
Combinacoes = 40  Soma media = 193.40
Par        7= 35.00%(14)      8= 30.00%(12)      6= 12.50%( 5)      9= 12.50%( 5)      5=  7.50%( 3)      4=  2.50%( 1)
Impar      8= 35.00%(14)      7= 30.00%(12)      6= 12.50%( 5)      9= 12.50%( 5)     10=  7.50%( 3)     11=  2.50%( 1)
Primo      5= 40.00%(16)      6= 30.00%(12)      4= 10.00%( 4)      7= 10.00%( 4)      8=  7.50%( 3)      3=  2.50%( 1)
Rep        9= 32.50%(13)      8= 22.50%( 9)     10= 17.50%( 7)     11= 15.00%( 6)      7=  7.50%( 3)      0=  2.50%( 1)      6=  2.50%( 1)
L      43332= 27.50%(11)  44322= 27.50%(11)  44331= 15.00%( 6)  54321= 10.00%( 4)  44421=  5.00%( 2)  53322=  5.00%( 2)
C      43332= 25.00%(10)  54321= 20.00%( 8)  44322= 17.50%( 7)  44331= 12.50%( 5)  53322=  7.50%( 3)  53331=  7.50%( 3)
SeqS       4= 27.50%(11)      5= 20.00%( 8)      3= 15.00%( 6)      6= 12.50%( 5)      8= 12.50%( 5)      7= 10.00%( 4)     10=  2.50%( 1)
SeqNS      3= 42.50%(17)      2= 35.00%(14)      4= 17.50%( 7)      5=  5.00%( 2)
```

There is one line per group, and the values of each line are sorted by percentage, in
descending order (ties are listed by value). Each value shows `value=percentage(count)`,
and the columns are aligned from one line to the next. The groups are: even numbers
(`Par`), odd numbers (`Impar`), primes (`Primo`), repeated from the previous draw (`Rep`, same rule as in
`-v`), row patterns (`L`), column patterns (`C`), longest sequence of drawn numbers
(`SeqS`) and of non-drawn numbers (`SeqNS`). Only the values that occurred are listed,
and `L` and `C` show only the 6 most frequent patterns. The first line also has the
average sum. `-t` does not change the list, so it can be combined with `-v` and `-g`.

With `-v`, the statistics of each combination are appended:

```
01 02 03 04 06 07 11 15 16 17 18 19 22 24 25   Som=190  Par=7   Imp=8   Pri=6   Rep=9   L=44322  C=44322  SeqS=5   SeqNS=3
```

| Field | Meaning |
|-------|---------|
| `Som` | sum of the 15 numbers |
| `Par` / `Imp` | even / odd numbers |
| `Pri` | prime numbers |
| `Rep` | numbers repeated from the previous draw: the previous line with a matrix file (0 on the first line), or `lf_filter.conf` with `-n -a`/`-A` (not shown with `-n` alone) |
| `L` / `C` | distribution by rows / columns, in descending order (any of the 18 possible patterns, even those not accepted by `-l`/`-c`) |
| `SeqS` | longest sequence of drawn numbers |
| `SeqNS` | longest sequence of non-drawn numbers |

### Notes

* Very restrictive filters can take a long time (or never finish) in random
  mode, since combinations are generated and then discarded until `-n` valid ones
  are found. Impossible filters (for example `-m1 -f2 -f3`) are not detected.
* Random mode keeps every combination in memory to avoid duplicates, so very
  large values of `-n` get slower as the list grows.
* Lotteries are random: these filters do not increase the odds of winning.
