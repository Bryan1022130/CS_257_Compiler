PARSER  = minic_parser
LEX     = minic.l
YACC    = minic.y
CXX     = g++
CXXFLAGS= -std=c++17 -I../ast

OBJS = minic.tab.o lex.yy.o ast.o semantic.o

$(PARSER): $(OBJS) main.cpp
	$(CXX) $(CXXFLAGS) -o $(PARSER) main.cpp $(OBJS)

minic.tab.c minic.tab.h: $(YACC)
	bison -d $(YACC) -o minic.tab.c

minic.tab.o: minic.tab.c minic.tab.h
	$(CXX) $(CXXFLAGS) -c minic.tab.c -o minic.tab.o

lex.yy.c: $(LEX) minic.tab.h
	flex -o lex.yy.c $(LEX)

lex.yy.o: lex.yy.c
	$(CXX) $(CXXFLAGS) -c lex.yy.c -o lex.yy.o

ast.o: ../ast/ast.c ../ast/ast.h
	$(CXX) $(CXXFLAGS) -c ../ast/ast.c -o ast.o

semantic.o: semantic.cpp semantic.h
	$(CXX) $(CXXFLAGS) -c semantic.cpp -o semantic.o

clean:
	rm -f $(PARSER)
	rm -f *.o
	rm -f lex.yy.c
	rm -f minic.tab.c minic.tab.h
