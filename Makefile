CC = cc
CFLAGS = -fPIC -Wall
LDFLAGS = -Wl,-init,pyusdt_init -shared
TARGET = libpyusdt.so
SRC = pyusdt.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

test:
	for test in tests/*.py; do PYTHONPATH=. python $$test || exit 1; done

.PHONY: all clean test
