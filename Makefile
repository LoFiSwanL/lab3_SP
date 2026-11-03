CC = cc
CFLAGS = -std=c11 -Wall -Wextra -Wno-unused-function -g
BISON = bison
FLEX = flex

all: build/minic

build:
	mkdir -p build

build/parser.tab.c: src/parser.y src/ast.h | build
	$(BISON) -d -v --report=state,solved -o $@ $<

build/parser.tab.h: build/parser.tab.c
	@test -f $@

build/lexer.c: src/lexer.l build/parser.tab.h
	$(FLEX) -o $@ $<

build/assets.h: src/embed.py src/runtime.py src/viewer.html | build
	python3 src/embed.py

build/minic: build/parser.tab.c build/lexer.c src/ast.c src/main.c src/ast.h build/assets.h
	$(CC) $(CFLAGS) -Isrc -Ibuild build/parser.tab.c build/lexer.c src/ast.c src/main.c -o $@

test: build/minic
	python3 -m unittest discover -s tests -v

demo: build/minic
	./build/minic examples/factorial.c --run --python build/result.py --html build/ast.html
	python3 build/result.py

clean:
	rm -rf build

.PHONY: all test demo clean
