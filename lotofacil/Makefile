CC      := gcc
WARN    := -Wall -Wextra -Wformat-signedness
CFLAGS  := $(WARN) -O2
LDLIBS  := -lconfig
TARGET  := lf_filter
SRC     := lf_filter.c

.PHONY: all debug release clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

# sanitizers + simbolos para testes
debug: CFLAGS := $(WARN) -std=c11 -g -O1 -fsanitize=address,undefined
debug: clean $(TARGET)

# build final: mesmas flags do padrao, sem simbolos (strip)
release: CFLAGS += -s
release: clean $(TARGET)

clean:
	rm -f $(TARGET)
