#include <llvm-c/Core.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char const *argv[]) {
    // Create a module
    LLVMModuleRef mod = LLVMModuleCreateWithName("minic_module");

    // Types
    LLVMTypeRef i32 = LLVMInt32Type();

    // Declare external: int read();
    LLVMTypeRef read_ty = LLVMFunctionType(i32, NULL, 0, 0);
    LLVMValueRef read_fn = LLVMAddFunction(mod, "read", read_ty);

    // Define: int func(int p)
    LLVMTypeRef param_types[] = { i32 };
    LLVMTypeRef func_ty = LLVMFunctionType(i32, param_types, 1, 0);
    LLVMValueRef func = LLVMAddFunction(mod, "func", func_ty);

    // Entry block
    LLVMBasicBlockRef entry = LLVMAppendBasicBlock(func, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilder();
    LLVMPositionBuilderAtEnd(builder, entry);

    // Allocate locals: p, x
    LLVMValueRef p_alloc = LLVMBuildAlloca(builder, i32, "p");
    LLVMSetAlignment(p_alloc, 4);

    LLVMValueRef x_alloc = LLVMBuildAlloca(builder, i32, "x");
    LLVMSetAlignment(x_alloc, 4);

    // Store param p into local p
    LLVMBuildStore(builder, LLVMGetParam(func, 0), p_alloc);

    // x = read()
    LLVMValueRef x_val = LLVMBuildCall2(builder, read_ty, read_fn, NULL, 0, "xval");
    LLVMBuildStore(builder, x_val, x_alloc);

    // Load for compare
    LLVMValueRef p_val = LLVMBuildLoad2(builder, i32, p_alloc, "pval");
    LLVMValueRef x_val2 = LLVMBuildLoad2(builder, i32, x_alloc, "xval2");

    // cond = (x > p)
    LLVMValueRef cond = LLVMBuildICmp(builder, LLVMIntSGT, x_val2, p_val, "cond");

    // Create then/else blocks
    LLVMBasicBlockRef then_BB = LLVMAppendBasicBlock(func, "then");
    LLVMBasicBlockRef else_BB = LLVMAppendBasicBlock(func, "else");

    // Branch on cond
    LLVMBuildCondBr(builder, cond, then_BB, else_BB);

    // then: return p
    LLVMPositionBuilderAtEnd(builder, then_BB);
    // (p_val already loaded; it's still valid SSA)
    LLVMBuildRet(builder, p_val);

    // else: return x
    LLVMPositionBuilderAtEnd(builder, else_BB);
    LLVMBuildRet(builder, x_val2);

    // Dump + write IR
    LLVMDumpModule(mod);
    if (LLVMPrintModuleToFile(mod, "func.ll", NULL) != 0) {
        fprintf(stderr, "Error: failed to write func.ll\n");
    }

    // Cleanup
    LLVMDisposeBuilder(builder);
    LLVMDisposeModule(mod);

    return 0;
}