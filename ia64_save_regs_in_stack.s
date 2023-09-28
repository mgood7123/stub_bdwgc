        .text
        .align 16
        .global MANAGED_STACK_ADDRESS_BOEHM_GC_save_regs_in_stack
        .proc MANAGED_STACK_ADDRESS_BOEHM_GC_save_regs_in_stack
MANAGED_STACK_ADDRESS_BOEHM_GC_save_regs_in_stack:
        .body
        flushrs
        ;;
        mov r8=ar.bsp
        br.ret.sptk.few rp
        .endp MANAGED_STACK_ADDRESS_BOEHM_GC_save_regs_in_stack
