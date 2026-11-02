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
* glibc >= 2.36 (for `arc4random_uniform`)

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

The `-a` option compares the combinations with the previous draw, which is read from
`lf_filter.conf` in the current directory:

```
ultimo_concurso = {
  numero  = 1780;
  dezenas = [3,4,5,6,7,9,10,11,13,14,15,16,17,22,23];
};
```

### Usage

```
./lf_filter [FILTERS] [-v] -n<number of random combinations>
./lf_filter [FILTERS] [-v] <matrix file>
```

With `-n`, random combinations are generated (the matrix file is ignored). Without
it, the combinations are read from the matrix file: one combination per line, 15 distinct
numbers (1..25) separated by spaces. The filters are applied to every combination
read from the file.

Example `matrix.txt`:

```
01 02 03 04 05 06 07 08 09 10 11 12 13 14 15
03 04 05 06 07 09 10 11 13 14 15 16 17 22 23
01 02 04 05 07 08 10 11 13 14 16 17 19 20 22
02 03 05 06 08 09 11 12 14 15 17 18 20 21 23
01 02 03 05 07 09 11 13 15 17 19 21 23 24 25
```

Leading zeros are accepted, empty lines are ignored, and the order of the
numbers in a line does not matter. A line with a wrong count, a repeated
number or a number outside 1..25 aborts the program with an error.

| Option | Description | Values |
|--------|-------------|--------|
| `-n`   | number of random combinations | 1 ~ 3268760 |
| `-v`   | versose mode - show the statistics of each combination | (no value) |
| `-p`   | number of even numbers | 6 ~ 9 |
| `-a`   | numbers repeated from the previous draw (needs `lf_filter.conf`) | 7 ~ 11 |
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

If several values are given for `-p`, `-a`, `-l` or `-c`, a combination is kept
if it matches **any** of them. All the other filters must always match. `-r`
and `-m` can be used at most once each, and `-r` cannot be greater than `-m`.

Run without arguments to see the full help.

### Examples

```
# 10 random combinations: 7 even numbers, previous-draw repeats = 9, sum 184..199
./lf_filter -p7 -a9 -i184 -x199 -n10

# 5 combinations that contain 1 and 25 and never contain 13
./lf_filter -f1 -f25 -d13 -n5

# between 4 and 6 primes
./lf_filter -r4 -m6 -n10

# filter an existing list
./lf_filter -p7 -r5 matrix.txt

# same, showing the statistics of each combination
./lf_filter -v -p7 -r5 matrix.txt
```

### Output

One combination per line, in ascending order:

```
02 03 04 06 07 10 11 13 16 17 18 19 21 24 25
```

With `-v`, the statistics of each combination are appended:

```
01 02 03 04 06 07 11 15 16 17 18 19 22 24 25   Soma=190  Pares=07  Impares=08  Primos=6  Rep=9  L=44322  C=44322  SeqS=5  SeqNS=3
```

| Field | Meaning |
|-------|---------|
| `Soma` | sum of the 15 numbers |
| `Pares` / `Impares` | even / odd numbers |
| `Primos` | prime numbers |
| `Rep` | numbers repeated from the previous draw (only shown with `-a`) |
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
