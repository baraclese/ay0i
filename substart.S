.globl _start

_start:
	call main
	# do exit syscall
	movq $60, %rax
	movq $0, %rdi
	syscall
