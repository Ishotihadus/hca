CFLAGS += -std=c99 -O2 -Wall -Wextra
CPPFLAGS += -Iinclude `pkg-config --cflags sndfile`
LDLIBS += `pkg-config --libs sndfile` -lm
SOURCES = $(wildcard src/*.c)
OBJS = $(subst src/,obj/,$(SOURCES:.c=.o))
LIBS = lib/libhca.a
LIB_HCA_DEPENDENCIES = $(subst src/,obj/,$(subst .c,.o,$(wildcard src/hca_*.c)))
TARGET = bin/hca
PREFIX = $(HOME)/.local

all: $(TARGET) $(LIBS)

$(TARGET): $(OBJS)
	mkdir -p bin
	$(CC) -o $@ $^ $(LDLIBS)

obj/%.o: src/%.c
	mkdir -p obj
	$(CC) $(CFLAGS) $(CXXFLAGS) $(CPPFLAGS) -o $@ -c $<

lib/libhca.a: $(LIB_HCA_DEPENDENCIES)
	mkdir -p lib
	ar rs $@ $^

.PHONY: clean install install-bin install-lib
install: install-bin install-lib install-include
install-bin: $(TARGET)
	cp -vR $^ $(PREFIX)/bin/
install-lib: $(LIBS)
	cp -vR $^ $(PREFIX)/lib/
install-include: include/hca
	cp -vR $^ $(PREFIX)/include/
clean:
	rm -f $(OBJS) $(LIBS) $(TARGET)
