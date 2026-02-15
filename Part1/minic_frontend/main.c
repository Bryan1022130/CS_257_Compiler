int main(int argc, char** argv) {
    if (argc == 2) {
        yyin = fopen(argv[1], "r");
    }
    yyparese();
    printNode(root);
    sem_flag = semantic_analysis(root);
    //convert to LLVM IR
    // Code Optimizer 
    //Asemmbly Code Generator

    // Clean up
    freeNode(root);
    if(yyin != stdin)
        fclose(yyin);
    yylex_destroy();

    //free ast
    
    return 0;
    }
