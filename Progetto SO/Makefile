CC = gcc

CFLAGS = -g -o -Wvla -Wextra -Werror -Wpedantic
INCLUDES = src/functions.h

COMMON_DEPS = $(INCLUDES) Makefile

build/%.o: src/%.c $(COMMON_DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

bin/starter: build/starter.o build/functions.o $(COMMON_DEPS)
	$(CC) -o bin/starter build/functions.o build/starter.o

bin/master: build/master.o build/functions.o $(COMMON_DEPS)
	$(CC) -o bin/master build/functions.o build/master.o

bin/atom: build/atom.o build/functions.o $(COMMON_DEPS)
	$(CC) -o bin/atom build/functions.o build/atom.o

bin/activator: build/activator.o build/functions.o $(COMMON_DEPS)
	$(CC) -o bin/activator build/functions.o build/activator.o

bin/feeder: build/feeder.o build/functions.o $(COMMON_DEPS)
	$(CC) -o bin/feeder build/functions.o build/feeder.o

bin/inhibitor: build/inhibitor.o build/functions.o $(COMMON_DEPS)
	$(CC) -o bin/inhibitor build/functions.o build/inhibitor.o

# Regola 'all' per compilare tutti gli eseguibili
all: bin/starter bin/master bin/feeder bin/atom bin/activator bin/inhibitor

# Regola 'clean' per eliminare tutti i file generati
clean:
	rm -f build/* bin/*