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
./lf_filter [FILTERS] -n<number of random combinations>
./lf_filter [FILTERS] <matrix file>
```

With `-n`, random combinations are generated (the matrix file is ignored). Without
it, the combinations are read from the matrix file: one combination per line, 15 distinct
numbers (1..25) separated by spaces.

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
| `-n`   | number of random combinations | 1 ~ 3.268.760 |
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
| `-r`   | number of primes | 4 ~ 7 |

Run without arguments to see the full help.

### Examples

```
# 10 random combinations: 7 even numbers, previous-draw repeats = 9, sum 184..199
./lf_filter -p7 -a9 -i184 -x199 -n10

# 5 combinations that contain 1 and 25 and never contain 13
./lf_filter -f1 -f25 -d13 -n5

# filter an existing list
./lf_filter -p7 -r5 matrix.txt
```

Output (one combination per line, followed by its sum):

```
01 03 04 06 08 10 11 13 15 17 18 20 22 24 25    Soma = 216
```

### Notes

* Very restrictive filters can take a long time (or never finish) in random
  mode, since combinations are generated and then discarded until `-n` valid ones
  are found.
* Lotteries are random: these filters do not increase the odds of winning.
