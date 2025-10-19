PYTHON := python3
PYTHON_CONFIG := $(PYTHON)-config
PYTHON_INCLUDE := $(shell $(PYTHON_CONFIG) --includes)
PYTHON_LDFLAGS := $(shell $(PYTHON_CONFIG) --ldflags --embed 2>/dev/null || $(PYTHON_CONFIG) --ldflags)

CC := cc
CFLAGS := -fPIC -Wall $(PYTHON_INCLUDE)
TARGET := libpyusdt.so
SRC := pyusdt.c

all: $(TARGET)

$(TARGET): $(SRC) usdt.h
	$(CC) $(CFLAGS) -shared -o $(TARGET) $(SRC) $(PYTHON_LDFLAGS)

clean:
	rm -f $(TARGET)

test:
	for test in tests/*.py; do PYTHONPATH=. python $$test || exit 1; done

.PHONY: all clean test
