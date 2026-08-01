    .set noreorder
    .text
    .align 2

    .globl setjmp
    .type setjmp, @function
setjmp:
    sw      $s0, 0($a0)
    sw      $s1, 4($a0)
    sw      $s2, 8($a0)
    sw      $s3, 12($a0)
    sw      $s4, 16($a0)
    sw      $s5, 20($a0)
    sw      $s6, 24($a0)
    sw      $s7, 28($a0)
    sw      $fp, 32($a0)
    sw      $gp, 36($a0)
    sw      $sp, 40($a0)
    sw      $ra, 44($a0)
    move    $v0, $zero
    jr      $ra
    nop
    .size setjmp, .-setjmp

    .globl longjmp
    .type longjmp, @function
longjmp:
    lw      $s0, 0($a0)
    lw      $s1, 4($a0)
    lw      $s2, 8($a0)
    lw      $s3, 12($a0)
    lw      $s4, 16($a0)
    lw      $s5, 20($a0)
    lw      $s6, 24($a0)
    lw      $s7, 28($a0)
    lw      $fp, 32($a0)
    lw      $gp, 36($a0)
    lw      $sp, 40($a0)
    lw      $ra, 44($a0)
    move    $v0, $a1
    bne     $v0, $zero, 1f
    nop
    li      $v0, 1
1:
    jr      $ra
    nop
    .size longjmp, .-longjmp
