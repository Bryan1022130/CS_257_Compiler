Your pseudocode must include the following details (these are more things that you need to care about):

When is a new basic block created
How does the builder move to different basic blocks (where are you adding IR instructions)
A clear order in which instructions are created
What are the operands of each instruction, and how to get their reference
 

Here is an example pseudocode to generate LLVM IR for values and binary expressions:

If the AST node is a binary expression:
Generate LLVMValueRef for the lhs in the binary expression node by calling genIRExpr recursively.
Generate LLVMValueRef for the rhs in the binary expression node by calling genIRExpr recursively.
Based on the operator in the binary expression node, generate an addition/subtraction/multiplication instruction using the LLVMValueRef of lhs and rhs as operands.
Return the LLVMValueRef of the instruction generated in step C.
If the node is a constant:
Generate an LLVMValueRef using LLVMConstInt using the constant value in the node.
Return the LLVMValueRef of the LLVM Constant.