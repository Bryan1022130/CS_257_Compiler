Creating a module: LLVMModuleCreateWithName

Setting the final architecture target (use "x86_64-pc-linux-gnu" for target triple): LLVMSetTarget(LLVMModuleRef M, const char *Triple) 
Creating the function type (LLVMTypeRef *ParamTypes is a set of parameter types, see create_ir class example for details): LLVMTypeRef LLVMFunctionType(LLVMTypeRef ReturnType, LLVMTypeRef *ParamTypes, unsigned ParamCount, LLVMBool IsVarArg)

Adding function to a module: LLVMAddFunction(LLVMModuleRef M, const char *Name, LLVMTypeRef FunctionTy)
Adding a basic block to a function: LLVMAppendBasicBlock(LLVMValueRef Fn, const char *Name)  
This group of functions are for adding instructions to each basic block. The most important is for creating a builder.  A builder is responsible for placing the instruction at the correct location in a basic block. 
Creating a builder: LLVMCreateBuilder
Setting the location of a builder: 
LLVMPositionBuilderAtEnd(LLVMBuilderRef Builder, LLVMBasicBlockRef Block);

LLVMPositionBuilderBefore(LLVMBuilderRef Builder, LLVMValueRef Instr)

LLVMPositionBuilder(LLVMBuilderRef Builder, LLVMBasicBlockRef Block, LLVMValueRef Instr)

LLVMGetInsertBlock(LLVMBuilderRef Builder) 

 LLVMClearInsertionPosition(LLVMBuilderRef Builder)

LLVMInsertIntoBuilder(LLVMBuilderRef Builder, LLVMValueRef Instr)

Once done creating your LLVM IR, dispose of the builder: LLVMDisposeBuilder(LLVMBuilderRef Builder) 
Function to create different types of instructions: 
Terminator instructions:
Unconditional branch: LLVMBuildBr(LLVMBuilderRef, LLVMBasicBlockRef Dest)
Conditional branch: LLVMBuildCondBr(LLVMBuilderRef, LLVMValueRef If,  LLVMBasicBlockRef Then, LLVMBasicBlockRef Else)
Return statement: LLVMBuildRet(LLVMBuilderRef, LLVMValueRef  V)
Arithmetic and comparison instructions:
Integer comparison: LLVMBuildICmp(LLVMBuilderRef, LLVMIntPredicate Op, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)
Integer addition: LLVMBuildAdd(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)
Integer subtraction: LLVMBuildSub(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name) 
Integer multiplication: LLVMBuildMul(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)
Integer division (we always use signed division): LLVMBuildSDiv(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name)
Call instruction: LLVMBuildCall2(LLVMBuilderRef, LLVMTypeRef, LLVMValueRef Fn, LLVMValueRef *Args, unsigned NumArgs, const char *Name)                            
Load and store instructions:
Create a load instruction: LLVMBuildLoad2(LLVMBuilderRef, LLVMTypeRef Ty, LLVMValueRef PointerVal, const char *Name)
Create a store instruction: LLVMBuildStore(LLVMBuilderRef, LLVMValueRef Val, LLVMValueRef Ptr)
Alloc instructions:
Create alloc instructions: LLVMBuildAlloca(LLVMBuilderRef, LLVMTypeRef Ty, const char *Name)
Set the memory alignment: LLVMSetAlignment(LLVMValueRef V, unsigned Bytes)