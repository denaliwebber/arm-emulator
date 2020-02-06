#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define NREGS 16
#define STACK_SIZE 1024
#define SP 13
#define LR 14
#define PC 15

/* Assembly functions to emulate */
int quadratic_a(int a, int b, int c, int d);
int quadratic_c(int a, int b, int c, int d);
int sum_array_a(int *array, int n);
int sum_array_c(int *array, int n);
int find_max_a(int *array, int n);
int find_max_c(int *array, int n);
int fib_iter_a(int n);
int fib_iter_c(int n);
int fib_rec_a(int n);
int fib_rec_c(int n);
int strlen_a(char *s);
int strlen_c(char *s);

/* The complete machine state */
struct arm_state {
    unsigned int regs[NREGS];
    unsigned char stack[STACK_SIZE];
    int n_flag;
    int z_flag;
    int c_flag;
    int v_flag;
    unsigned int computation_count;
    unsigned int memory_count;
    unsigned int branch_taken;
    unsigned int branch_not_taken;
};

struct cache_slot {
    unsigned int v;
    unsigned int tag;
};

struct direct_mapped_cache {
    struct cache_slot slots[STACK_SIZE];
    int cache_hit;
    int cache_miss;
    int requests;
    int size;
};

/* Initialize an arm_state struct with a function pointer and arguments */
void arm_state_init(struct arm_state *as, struct direct_mapped_cache *cache, unsigned int *func, unsigned int arg0, unsigned int arg1, unsigned int arg2, unsigned int arg3)
{
    int i;
    
    // Zero out all arm state
    for (i = 0; i < NREGS; i++) {
        as->regs[i] = 0;
    }
    
    // Zero out CPSR
    as->n_flag = 0;
    as->z_flag = 0;
    as->c_flag = 0;
    as->v_flag = 0;
    
    // Zero out the stack
    for (i = 0; i < STACK_SIZE; i++) {
        as->stack[i] = 0;
    }
    
    // Set the PC to point to the address of the function to emulate
    as->regs[PC] = (unsigned int) func;
    
    // Set the SP to the top of the stack (the stack grows down)
    as->regs[SP] = (unsigned int) &as->stack[STACK_SIZE];
    
    // Initialize LR to 0, this will be used to determine when the function has called bx lr
    as->regs[LR] = 0;
    
    // Initialize the first 4 arguments
    as->regs[0] = arg0;
    as->regs[1] = arg1;
    as->regs[2] = arg2;
    as->regs[3] = arg3;
    
    // Initialize the instruction counts
    as->computation_count = 0;
    as->memory_count = 0;
    as->branch_taken = 0;
    as->branch_not_taken = 0;
    
    // Initialzies the Cache
    cache->cache_hit = 0;
    cache->cache_miss = 0;
    cache->requests = 0;
    
    for (i = 0; i < STACK_SIZE; i++) {
        cache->slots[i].v = 0;
        cache->slots[i].tag = 0;
    }
}

void arm_state_print(struct arm_state *as)
{
    int i;
    
    for (i = 0; i < NREGS; i++) {
        printf("reg[%d] = %d\n", i, as->regs[i]);
    }
    
    printf("cpsr flags\nn_flag = %d\nz_flag = %d\nc_flag = %d\nv_flag = %d\n", as->n_flag, as->z_flag, as->c_flag, as->v_flag);
}

void instruction_count_print(struct arm_state *state)
{
    unsigned int total;
    
    total = state->computation_count+state->memory_count+state->branch_taken+state->branch_not_taken;
    
    printf("\nTotal Instructions Executed: %d\n", total);
    printf("Total Computational Instructions Executed: %d\n", state->computation_count);
    printf("\t%0.0f%% of total instructions\n", 100 * ((float)state->computation_count/(float)total));
    printf("Total Memory Instructions Executed: %d\n", state->memory_count);
    printf("\t%0.0f%% of total instructions\n", ((float)state->memory_count/(float)total));
    printf("Total Branch Instructions Executed: %d\n", (state->branch_taken+state->branch_not_taken));
    printf("Total Branch Instructions Taken: %d\n", state->branch_taken);
    printf("\t%0.0f%% of branch instructions\n", 100 * ((float)state->branch_taken/(float)(state->branch_taken+state->branch_not_taken)));
    printf("\t%0.0f%% of total instructions\n", 100 * ((float)state->branch_taken/(float)total));
    printf("Total Branch Instructions Not Taken: %d\n", state->branch_not_taken);
    printf("\t%0.0f%% of branch instructions\n", 100 * ((float)state->branch_not_taken/(float)(state->branch_taken+state->branch_not_taken)));
    printf("\t%0.0f%% of total instructions\n\n", 100 * ((float)state->branch_not_taken/(float)total));
}

void cache_output(struct direct_mapped_cache *cache)
{
    printf("Cache size: %d\n", cache->size);
    printf("Total Cache Requests: %d\n", cache->requests);
    printf("Total Cache Hits: %d\n", cache->cache_hit);
    printf("\t%0.0f%% of Cache hits: \n", 100 * ((float)cache->cache_hit/(float)cache->requests));
    printf("Total Cache Misses: %d\n", cache->cache_miss);
    printf("\t%0.0f%% of Cache misses: \n\n", 100 * ((float)cache->cache_miss/(float)cache->requests));

}

void set_cpsr_flags(struct arm_state *state, unsigned int a, unsigned int b)
{
    int result, as, bs;
    long long al, bl;
    
    as = (int) a;
    bs = (int) b;
    al = (long long) a;
    bl = (long long) b;
    
    result = as - bs;
    
    state->n_flag = (result < 0);
    
    state->z_flag = (result == 0);
    
    state->c_flag = (b > a);
    
    state->v_flag = 0;
    if ((as > 0) && (bs < 0)) {
        if ((al + (-1 * bl)) > 0x7FFFFFFF) {
            state->v_flag = 1;
        }
    } else if ((as < 0) && (bs > 0)) {
        if (((-1 * al) + bl) > 0x80000000) {
            state->v_flag = 1;
        }
    }
}

// computes the slot that the address would go in the cache
int get_slot(int size, unsigned int addr)
{
    return((addr >> 2) & (size -1));
}
           
unsigned int get_tag(int size, unsigned int addr)
{
    int log2 = 0;
    while(size) {
        size = size >> 1;
        log2 = log2 + 1;
    }
    return (addr >> (2 + log2)) & ((2 + log2) -1);
}

void simulate_cache(struct direct_mapped_cache *cache, unsigned int addr)
{
    int addr_slot;
    unsigned int tag;
    
    //update cache requests
    cache->requests = cache->requests + 1;
    
    addr_slot = get_slot(cache->size, addr);
    tag = get_tag(cache->size, addr);
    
    int v = cache->slots[addr_slot].v;
    int slot_tag = cache->slots[addr_slot].tag;
        
    if(v) {
        if(tag == slot_tag){
            cache->cache_hit++;
        } else {
            cache->cache_miss++;
            cache->slots[addr_slot].tag = tag;
            cache->slots[addr_slot].v = 1;
        }
    } else {  //v == 0 so update the slot and increment the miss
        cache->cache_miss++;
        cache->slots[addr_slot].tag = tag;
        cache->slots[addr_slot].v = 1;
    }
}

bool is_data_processing_inst(unsigned int iw)
{
    return ((iw >> 26) & 0b11) == 0;
}

void armemu_data_processing(struct arm_state *state)
{
    unsigned int opcode;
    unsigned int iw;
    unsigned int rm_val;
    unsigned int rd;
    unsigned int rn;
    unsigned int i_bit;
    
    iw = *((unsigned int *) state->regs[PC]);
    
    opcode = (iw >> 21) & 0xF;
    i_bit = (iw >> 25) & 0b1;
    
    rd = (iw >> 12) & 0xF;
    rn = (iw >> 16) & 0xF;
    
    if (i_bit == 1)
        rm_val = iw & 0xFF;
    else
        rm_val = state->regs[(iw & 0xF)];
    
    switch(opcode)
    {
        case 2: //sub
            state->regs[rd] = state->regs[rn] - rm_val;
            break;
        case 4: //add
            state->regs[rd] = state->regs[rn] + rm_val;
            break;
        case 10: //cmp
            set_cpsr_flags(state, state->regs[rn], rm_val);
            break;
        case 13: //mov
            state->regs[rd] = rm_val;
            break;
    }
    
    state->computation_count++;
    state->regs[PC] = state->regs[PC] + 4;
}

bool is_mul_inst(unsigned int iw)
{
    unsigned int op;
    unsigned int op2;
    
    op = (iw >> 22) & 0b111111;
    op2 = (iw >> 4) & 0xF;
    
    return (op == 0) && (op2 == 0b1001);
    
}

void armemu_mul(struct arm_state *state)
{
    unsigned int iw;
    unsigned int rd;
    unsigned int rm;
    unsigned int rs;
    
    iw = *((unsigned int *) state->regs[PC]);
    
    rd = (iw >> 16) & 0xF;
    rm = iw & 0xF;
    rs = (iw >> 8) & 0xF;
    
    state->regs[rd] = state->regs[rm] * state->regs[rs];
    
    state->computation_count++;
    state->regs[PC] = state->regs[PC] + 4;
}

bool is_branch_inst(unsigned int iw)
{
    unsigned int opcode;
    // Strictly for checking if bl or b
    opcode = (iw >> 25) & 0b111 ;
    
    return (opcode == 0b101);
}

bool condition_flags(struct arm_state *state, unsigned int iw)
{
    unsigned int cond = (iw >> 28) & 0xF;
    
    switch(cond)
    {
        case 0: //beq
            //takes the branch as the Z flag is set
            return(state->z_flag == 1);
            
        case 1: //bne
            return(state->z_flag == 0);  //takes the branch
            
        case 11: //blt
            //n not equal to v
            return (state->n_flag != state->v_flag);
            
        case 12: //bgt
            //z clear and n equals v
            return(state->z_flag == 0 && (state->n_flag == state-> v_flag));
    }
}

// branch or branch and link w/ bne / beq
void armemu_branch(struct arm_state *state)
{
    unsigned int iw;
    unsigned int link;
    int offset;
    int significant_bit;
    int byte_address;
    int mask = 0xFF000000; //32 bit's of 1s

    iw = *((unsigned int *) state->regs[PC]);
    
    link = (iw >> 24) & 0b1;
    offset = iw & 0xFFFFFF;
    significant_bit = (iw >> 23) & 0b1;
    
    if(condition_flags(state, iw)){
        state->branch_taken += 1;
        
        if(significant_bit)
        {
            //adjust offset to be a 2's comp byte address by sign extending the offset
             offset = offset | mask;
             offset = (~ offset) + 1;
             offset = offset * -1;
        }
        //add new offset to PC
        byte_address = offset * 4;
        int adjust = byte_address + 8;
        
        if(link)   //branch with link
            state->regs[LR] = state->regs[PC] + 4;     //set LR to PC + 4 (the next instruction)
            state->regs[PC] = state->regs[PC] + adjust;
    }
    else {
        state->branch_not_taken++;
        state->regs[PC] += 4;
    }
}

bool is_bx_inst(unsigned int iw)
{
    unsigned int bx_code;
    
    bx_code = (iw >> 4) & 0x00FFFFFF;
    
    return (bx_code == 0b000100101111111111110001);
}

void armemu_bx(struct arm_state *state)
{
    unsigned int iw;
    unsigned int rn;
    
    iw = *((unsigned int *) state->regs[PC]);
    rn = iw & 0b1111;
    
    state->branch_taken++;
    state->regs[PC] = state->regs[rn];
}

bool is_single_data_transfer_inst(unsigned int iw)
{
    unsigned int op;
    
    op = (iw >> 26) & 0b11;
    
    return op == 0b01;
}

void armemu_single_data_transfer(struct arm_state *state)
{
    unsigned int iw;
    unsigned int rd;
    unsigned int rn;
    unsigned int rm_val;
    unsigned int i_bit;
    unsigned int b_bit;
    unsigned int l_bit;
    unsigned int target_address;
    
    iw = *((unsigned int *) state->regs[PC]);
    
    rd = (iw >> 12) & 0xF;
    rn = (iw >> 16) & 0xF;
    
    i_bit = (iw >> 25) & 0b1;
    b_bit = (iw >> 22) & 0b1;
    l_bit = (iw >> 20) & 0b1;
    
    //Check i bit
    if (i_bit == 1)
    {
        rm_val = state->regs[(iw & 0xF)];
        target_address = state->regs[rn] + rm_val;
    }
    else
        target_address = state->regs[rn] + (iw & 0xFFF);
    
    //Check b bit
    if (b_bit == 1) {
    // Check l bit
        if (l_bit == 1) {
            state->regs[rd] = (unsigned int)*((char *)target_address); //ldrb
        }
    }
    else {
    //Check l bit
        if (l_bit == 1) {
            state->regs[rd] = *((unsigned int *) target_address); //ldr
        }
        else {
            *((unsigned int *) target_address) = state->regs[rd]; //str
        }
    }
    state->memory_count++;
    state->regs[PC] = state->regs[PC] + 4;
}

void armemu_one(struct arm_state *state, struct direct_mapped_cache *cache)
{
    unsigned int iw;
    iw = *((unsigned int *) state->regs[PC]);
    simulate_cache(cache, state->regs[PC]);
    
    if (is_bx_inst(iw)){
        armemu_bx(state);
    } else if (is_branch_inst(iw)) {
            armemu_branch(state);
    } else if (is_mul_inst(iw)) {
        armemu_mul(state);
    } else if (is_data_processing_inst(iw)) {
        armemu_data_processing(state);
    } else if (is_single_data_transfer_inst(iw)) {
        armemu_single_data_transfer(state);
    }
}

unsigned int armemu(struct arm_state *state, struct direct_mapped_cache *cache)
{
    //Execute instructions until PC = 0
    //This happens when bx lr is issued and lr is 0
    while (state->regs[PC] != 0) {
        armemu_one(state, cache);
    }
    return state->regs[0];
}

void execute_sum_array(int c_size)
{
    struct arm_state state;
    struct direct_mapped_cache cache;
    unsigned int c_result;
    unsigned int a_result;
    unsigned int emu_result;
    int test[] = {1, 2, 3, 4};
    
    cache.size=c_size;
    arm_state_init(&state, &cache, (unsigned int *) sum_array_a,(unsigned int)test, 4, 0, 0);
    c_result = sum_array_c(test, 4);
    a_result = sum_array_a(test, 4);
    emu_result = armemu(&state, &cache);
    
    printf("\n-- Executing Sum Array Functions --\n");
    printf("quadratic_c(1, 2, 3, 4) = %d\n", c_result);
    printf("quadratic_a(1, 2, 3, 4) = %d\n", a_result);
    printf("armemu(quadratic_a(1, 2, 3, 4)) = %d\n", emu_result);

    instruction_count_print(&state);
    cache_output(&cache);

    int test2[] = {4, 0, 3, 0, 5};
    
    arm_state_init(&state, &cache, (unsigned int *) sum_array_a,(unsigned int)test2, 5, 0, 0);
    
    c_result = sum_array_c(test2, 5);
    a_result = sum_array_a(test2, 5);
    emu_result = armemu(&state, &cache);
    
    printf("\n");
    printf("sum_array_c(4, 0, 3, 0, 5) = %d\n", c_result);
    printf("sum_array_a(4, 0, 3, 0, 5) = %d\n", a_result);
    printf("armemu(sum_array_a(4, 0, 3, 0, 5)) = %d\n", emu_result);

    instruction_count_print(&state);
    cache_output(&cache);

    int test3[] = {9, 0, -5, -1, 12};
    
    arm_state_init(&state, &cache, (unsigned int *) sum_array_a,(unsigned int)test3, 5, 0, 0);
    
    c_result = sum_array_c(test3, 5);
    a_result = sum_array_a(test3, 5);
    emu_result = armemu(&state, &cache);
    
    printf("\n");
    printf("sum_array_c(9, 0, -5, -1, 12) = %d\n", c_result);
    printf("sum_array_a(9, 0, -5, -1, 12) = %d\n", a_result);
    printf("armemu(sum_array_a(9, 0, -5, -1, 12)) = %d\n", emu_result);

    instruction_count_print(&state);
    cache_output(&cache);

    int test4[1000];

    for (int i = 0; i < 1000; i++) {
        test4[i] = i;
    }
    
    arm_state_init(&state, &cache, (unsigned int *) sum_array_a,(unsigned int)test4, 1000, 0, 0);
    
    c_result = sum_array_c(test4, 1000);
    a_result = sum_array_a(test4, 1000);
    emu_result = armemu(&state, &cache);
    
    printf("\n");
    printf("sum_array_c(0-999) = %d\n", c_result);
    printf("sum_array_a(0-999) = %d\n", a_result);
    printf("armemu(sum_array_a(0-999)) = %d\n", emu_result);
    
    instruction_count_print(&state);
    cache_output(&cache);
}

void execute_find_max(int c_size)
{
    struct arm_state state;
    struct direct_mapped_cache cache;
    unsigned int c_result;
    unsigned int a_result;
    unsigned int emu_result;
    
    int test[] = {10, 2, 6, 3, 5};
    
    cache.size=c_size;
    arm_state_init(&state, &cache, (unsigned int *) find_max_a,(unsigned int) test, 5, 0, 0);
    c_result = find_max_c(test, 5);
    a_result = find_max_a(test, 5);
    emu_result = armemu(&state, &cache);
    
    printf("\n-- Executing Find Max Functions --\n");
    printf("find_max_c(10, 2, 6, 3, 5) = %d\n", c_result);
    printf("find_max_a(10, 2, 6, 3, 5) = %d\n", a_result);
    printf("armemu(find_max_a(10, 2, 6, 3, 5)) = %d\n", emu_result);

    instruction_count_print(&state);
    cache_output(&cache);

    int test2[] = {-4, 3, 7, -2, 12};
    
    arm_state_init(&state, &cache, (unsigned int *) find_max_a,(unsigned int) test2, 5, 0, 0);
    
    c_result = find_max_c(test2, 5);
    a_result = find_max_a(test2, 5);
    emu_result = armemu(&state, &cache);
    
    printf("find_max_c(-4, 3, 7, -2, 12) = %d\n", c_result);
    printf("find_max_a(-4, 3, 7, -2, 12) = %d\n", a_result);
    printf("armemu(find_max_a(-4, 3, 7, -2, 12)) = %d\n", emu_result);

    instruction_count_print(&state);
    cache_output(&cache);

    int test3[] = {0, -5, 0, 3, 1};
    
    arm_state_init(&state, &cache, (unsigned int *) find_max_a,(unsigned int) test3, 5, 0, 0);
    
    c_result = find_max_c(test3, 5);
    a_result = find_max_a(test3, 5);
    emu_result = armemu(&state, &cache);
    
    printf("find_max_c(0, -5, 0, 3, 1) = %d\n", c_result);
    printf("find_max_a(0, -5, 0, 3, 1) = %d\n", a_result);
    printf("armemu(find_max_a(0, -5, 0, 3, 1)) = %d\n", emu_result);

    instruction_count_print(&state);
    cache_output(&cache);

    int test4[1000];

    for (int i = 0; i < 1000; i++) {
        test4[i] = i;
    }
    
    arm_state_init(&state, &cache, (unsigned int *) find_max_a,(unsigned int) test4, 1000, 0, 0);
    
    c_result = find_max_c(test4, 1000);
    a_result = find_max_a(test4, 1000);
    emu_result = armemu(&state, &cache);
    
    printf("find_max_c(0-999) = %d\n", c_result);
    printf("find_max_a(0-999) = %d\n", a_result);
    printf("armemu(find_max_a(0-999)) = %d\n", emu_result);
    
    instruction_count_print(&state);
    cache_output(&cache);    
}

void execute_quadratic(int c_size)
{
    struct arm_state state;
    struct direct_mapped_cache cache;
    unsigned int c_result;
    unsigned int a_result;
    unsigned int emu_result;
    
    cache.size=c_size;
    arm_state_init(&state, &cache, (unsigned int *) quadratic_a, 1, 2, 3, 4);
    c_result = quadratic_c(1, 2, 3, 4);
    a_result = quadratic_a(1, 2, 3, 4);
    emu_result = armemu(&state, &cache);
    
    printf("-- Executing Quadratic Functions --\n");
    printf("quadratic_c(1, 2, 3, 4) = %d\n", c_result);
    printf("quadratic_a(1, 2, 3, 4) = %d\n", a_result);
    printf("armemu(quadratic_a(1, 2, 3, 4)) = %d\n", emu_result);

    instruction_count_print(&state);
    cache_output(&cache);

    arm_state_init(&state, &cache, (unsigned int *) quadratic_a, 7, 0, 4, -1);
    
    c_result = quadratic_c(7, 0, 4, -1);
    a_result = quadratic_a(7, 0, 4, -1);
    emu_result = armemu(&state, &cache);
    
    printf("quadratic_c(7, 0, 4, -1) = %d\n", c_result);
    printf("quadratic_a(7, 0, 4, -1) = %d\n", a_result);
    printf("armemu(quadratic_a(7, 0, 4, -1)) = %d\n", emu_result);

    instruction_count_print(&state);
    cache_output(&cache);

    arm_state_init(&state, &cache, (unsigned int *) quadratic_a, -10, 13, 0, 6);
    
    c_result = quadratic_c(-10, 13, 0, 6);
    a_result = quadratic_a(-10, 13, 0, 6);
    emu_result = armemu(&state, &cache);
    
    printf("quadratic_c(-10, 13, 0, 6) = %d\n", c_result);
    printf("quadratic_a(-10, 13, 0, 6) = %d\n", a_result);
    printf("armemu(quadratic_a(-10, 13, 0, 6)) = %d\n", emu_result);

    instruction_count_print(&state);
    cache_output(&cache);

    arm_state_init(&state, &cache, (unsigned int *) quadratic_a, -5, -8, -23, -1);
    
    c_result = quadratic_c(-5, -8, -23, -1);
    a_result = quadratic_a(-5, -8, -23, -1);
    emu_result = armemu(&state, &cache);

    printf("quadratic_c(-5, -8, -23, -1) = %d\n", c_result);
    printf("quadratic_a(-5, -8, -23, -1) = %d\n", a_result);
    printf("armemu(quadratic_a(-5, -8, -23, -1)) = %d\n", emu_result);
    
    instruction_count_print(&state);
    cache_output(&cache);
}

void execute_fib_iter(int c_size)
{
    struct arm_state state;
    struct direct_mapped_cache cache;
    unsigned int c_result;
    unsigned int a_result;
    unsigned int emu_result;

    printf("\n-- Executing Fib Iterative Functions --\n");

    for (int i = 0; i <= 20; i++)
    {
        cache.size=c_size;
        arm_state_init(&state, &cache, (unsigned int *) fib_iter_a, i, 0, 0, 0);
        c_result = fib_iter_c(i);
        a_result = fib_iter_a(i);
        emu_result = armemu(&state, &cache);

        printf("fib_iter_c(%d) = %d\n", i, c_result);
        printf("fib_iter_a(%d) = %d\n", i, a_result);
        printf("armemu(fib_iter_a(%d)) = %d\n\n", i, emu_result);

        instruction_count_print(&state);
        cache_output(&cache);
    }
}

void execute_fib_rec(int c_size)
{
    struct arm_state state;
    struct direct_mapped_cache cache;
    unsigned int c_result;
    unsigned int a_result;
    unsigned int emu_result;
    
    for (int i = 0; i <= 20; i++)
    {
        cache.size=c_size;
        arm_state_init(&state, &cache, (unsigned int *) fib_rec_a, i, 0, 0, 0);
        c_result = fib_rec_c(i);
        a_result = fib_rec_a(i);
        emu_result = armemu(&state, &cache);

        printf("fib_rec_c(%d) = %d\n", i, c_result);
        printf("fib_rec_a(%d) = %d\n", i, a_result);
        printf("armemu(fib_rec_a(%d)) = %d\n\n", i, emu_result);

        instruction_count_print(&state);
        cache_output(&cache);
    }    
}

void execute_strlen(int c_size)
{
    struct arm_state state;
    struct direct_mapped_cache cache;
    unsigned int c_result;
    unsigned int a_result;
    unsigned int emu_result;
    char test[] = "hello";
    
    arm_state_init(&state, &cache, (unsigned int *) strlen_a, (unsigned int)test, 0, 0, 0);
    cache.size=c_size;
    c_result = strlen_c(test);
    a_result = strlen_a(test);
    emu_result = armemu(&state, &cache);
    
    printf("\n-- Executing Strlen Functions --\n");
    printf("strlen_c(hello) = %d\n", c_result);
    printf("strlen_a(hello) = %d\n", a_result);
    printf("armemu(strlen_a(hello)) = %d\n", emu_result);
    
    instruction_count_print(&state);
    cache_output(&cache);

    char test2[] = "project04";
    
    arm_state_init(&state, &cache, (unsigned int *) strlen_a, (unsigned int)test2, 0, 0, 0);
    
    c_result = strlen_c(test2);
    a_result = strlen_a(test2);
    emu_result = armemu(&state, &cache);
    
    printf("strlen_c(project04) = %d\n", c_result);
    printf("strlen_a(project04) = %d\n", a_result);
    printf("armemu(strlen_a(project04)) = %d\n", emu_result);
    
    instruction_count_print(&state);
    cache_output(&cache);

    char test3[] = "hi";
    
    arm_state_init(&state, &cache, (unsigned int *) strlen_a, (unsigned int)test3, 0, 0, 0);
    
    c_result = strlen_c(test3);
    a_result = strlen_a(test3);
    emu_result = armemu(&state, &cache);
    
    printf("strlen_c(hi) = %d\n", c_result);
    printf("strlen_a(hi) = %d\n", a_result);
    printf("armemu(strlen_a(hi)) = %d\n", emu_result);
    
    instruction_count_print(&state);
    cache_output(&cache);

    char test4[] = "opportunity";
    
    arm_state_init(&state, &cache, (unsigned int *) strlen_a, (unsigned int)test4, 0, 0, 0);
    
    c_result = strlen_c(test4);
    a_result = strlen_a(test4);
    emu_result = armemu(&state, &cache);
    
    printf("strlen_c(opportunity) = %d\n", c_result);
    printf("strlen_a(opportunity) = %d\n", a_result);
    printf("armemu(strlen_a(opportunity)) = %d\n", emu_result);
    
    instruction_count_print(&state);
    cache_output(&cache);

    char test5[] = "backpack";
    
    arm_state_init(&state, &cache, (unsigned int *) strlen_a, (unsigned int)test5, 0, 0, 0);
    
    c_result = strlen_c(test5);
    a_result = strlen_a(test5);
    emu_result = armemu(&state, &cache);
    
    printf("strlen_c(backpack) = %d\n", c_result);
    printf("strlen_a(backpack) = %d\n", a_result);
    printf("armemu(strlen_a(backpack)) = %d\n", emu_result);
    
    instruction_count_print(&state);
    cache_output(&cache);    
}

int check_cache_size(int num)
{
    if(num > 7 && num < pow(2, 10)) {
        if(num %4 == 0)
            return num;
    }
    return 8;
}

int main(int argc, char **argv)
{
    
    int num;
    int size;
    char c[] = "-c";

    if(argc < 2){
        size = 8;
    } else if (strcmp(argv[1], c)==0) {
        num = atoi(argv[2]);
        size = check_cache_size(num);
    }

    execute_quadratic(size);
    
    execute_sum_array(size);

    execute_find_max(size);

    execute_fib_iter(size);
    
    execute_fib_rec(size);
    
    execute_strlen(size);
   
    return 0;
}
