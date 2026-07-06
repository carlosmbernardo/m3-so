.section .text
.global user_task_start

user_task_start:
    la a0, message

loop:
    li a7, 1      # syscall print
    ecall
    j loop

.section .rodata
message:
    .string "Hello from User Mode!\n"