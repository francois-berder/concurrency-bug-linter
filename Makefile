BUILDDIR := build
OBJDIR := $(BUILDDIR)/obj
DEPDIR := $(BUILDDIR)/dep
BINDIR := $(BUILDDIR)/bin

TARGET := $(BINDIR)/concurrency-bug-linter
SRCS := src/main.c src/function.c src/cJSON.c

OBJS := $(SRCS:src/%.c=$(OBJDIR)/%.o)

CFLAGS += -Wall -Wextra
CFLAGS += -I sparse
CFLAGS += -ggdb3
DEPFLAGS = -MMD -MP -MF $(@:$(OBJDIR)/%.o=$(DEPDIR)/%.d)

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJS) sparse/libsparse.a
	@mkdir -p $(@D)
	$(CC) $(LDFLAGS) $^ -o $@

$(OBJDIR)/%.o: src/%.c
	@mkdir -p $(@D)
	@mkdir -p $(DEPDIR)/$(<D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

sparse/libsparse.a:
	$(MAKE) -C $(@D) libsparse.a

.PHONY: clean
clean:
	rm -rf $(BUILDDIR)

.PHONY: distclean
distclean:
	rm -rf $(BUILDDIR)
	$(MAKE) -C sparse clean

