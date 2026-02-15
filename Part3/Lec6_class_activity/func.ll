; ModuleID = 'minic_module'
source_filename = "minic_module"

declare i32 @read()

define i32 @func(i32 %0) {
entry:
  %p = alloca i32, align 4
  %x = alloca i32, align 4
  store i32 %0, ptr %p, align 4
  %xval = call i32 @read()
  store i32 %xval, ptr %x, align 4
  %pval = load i32, ptr %p, align 4
  %xval2 = load i32, ptr %x, align 4
  %cond = icmp sgt i32 %xval2, %pval
  br i1 %cond, label %then, label %else

then:                                             ; preds = %entry
  ret i32 %pval

else:                                             ; preds = %entry
  ret i32 %xval2
}
