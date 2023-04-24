CC := gcc
CXX := g++

SRCDIR = src
OBJDIR = obj

TARGET = bin/hca
TARGET_LIBS = lib/libhca.a
PREFIX ?= $(HOME)/.local

DEPENDENCIES_CFLAGS := $(shell pkg-config --cflags sndfile)
DEPENDENCIES_LDFLAGS := $(shell pkg-config --libs sndfile)

CFLAGS += -std=c11 -O2 -Wall -Wextra
CXXFLAGS += -std=c++11 -O2 -Wall -Wextra
CPPFLAGS += -Iinclude $(DEPENDENCIES_CFLAGS)
LDFLAGS += -Llib -lm $(DEPENDENCIES_LDFLAGS)

COBJS := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(wildcard $(SRCDIR)/*.c))
CXXOBJS := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(wildcard $(SRCDIR)/*.cpp))
OBJS := $(COBJS) $(CXXOBJS)

all: $(TARGET) $(TARGET_LIBS)

$(TARGET): $(OBJDIR)/main.o $(OBJDIR)/writer.o lib/libhca.a
	@mkdir -p bin
	$(CC) $^ $(LDFLAGS) -lhca -o $@

$(COBJS): $(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(CXXOBJS): $(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

lib/libhca.a: $(filter $(OBJDIR)/hca_%.o,$(OBJS))
	@mkdir -p lib
	ar rs $@ $^

.PHONY: clean install
install:
	mkdir -p $(DESTDIR)$(PREFIX)/bin $(DESTDIR)$(PREFIX)/lib $(DESTDIR)$(PREFIX)/include
	cp -v $(TARGET) $(DESTDIR)$(PREFIX)/bin/
	cp -v $(TARGET_LIBS) $(DESTDIR)$(PREFIX)/lib/
	cp -vr include/hca $(DESTDIR)$(PREFIX)/include/
clean:
	rm -f bin/* lib/* $(OBJDIR)/*
