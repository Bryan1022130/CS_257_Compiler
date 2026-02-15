LLVMCODE = llvm_builder
TEST = func

$(LLVMCODE): $(LLVMCODE).c
	clang -g `llvm-config-17 --cflags` -I /usr/include/llvm-c-17/ -c $(LLVMCODE).c
	clang++ `llvm-config-17 --cxxflags --ldflags --libs core` -I /usr/include/llvm-c-17/ $(LLVMCODE).o -o $@

minic: $(LLVMCODE)
	./$(LLVMCODE)
	clang main.c $(TEST).ll -o $(TEST).out

clean:
	rm -rf $(TEST).ll
	rm -rf $(LLVMCODE)
	rm -rf *.o
	rm -rf *.out
