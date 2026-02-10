PARSER = minic_parser
LEX    = minic.l
YACC   = minic.y

OBJS = minic.tab.o lex.yy.o ast.o semantic.o

$(PARSER): $(OBJS) driver.cpp
	g++ -std=c++17 -I../ast -o $(PARSER) driver.cpp $(OBJS)

# Bison
minic.tab.c minic.tab.h: $(YACC)
	bison -d $(YACC) -o minic.tab.c

minic.tab.o: minic.tab.c
	g++ -std=c++17 -I../ast -c minic.tab.c -o minic.tab.o

# Flex
lex.yy.c: $(LEX) minic.tab.h
	flex -o lex.yy.c $(LEX)

lex.yy.o: lex.yy.c
	g++ -std=c++17 -I../ast -c lex.yy.c -o lex.yy.o

# AST + semantic
ast.o: ../ast/ast.c
	g++ -std=c++17 -I../ast -c ../ast/ast.c -o ast.o

semantic.o: semantic.cpp
	g++ -std=c++17 -I../ast -c semantic.cpp -o semantic.o

clean:
	rm -f $(PARSER)
	rm -f *.o
	rm -f lex.yy.c
	rm -f minic.tab.c minic.tab.h
