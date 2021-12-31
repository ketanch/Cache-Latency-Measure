#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <assert.h>

typedef unsigned long long uint64_t;

#define BLOCK_LEN 6
#define SET_MASK 0x3f
#define SET(x) ((x >> BLOCK_LEN) & SET_MASK)

FILE* log_file;

extern int inline __attribute__((__gnu_inline__, __always_inline__, __artificial__)) measure(char *addr) { 
    volatile unsigned long t;
    asm volatile (
    "mfence             \n\t"
    "lfence             \n\t"
    "rdtsc              \n\t"
    "lfence             \n\t"
    "mov %%rax, %%rsi   \n\t"
    "movb (%1), %%al    \n\t"
    "rdtscp             \n\t"
    "lfence             \n\t"
    "sub %%rsi, %%rax   \n\t"
    : "=a" (t)
    : "b" (addr)
    : "%rdx", "%rcx", "%rsi");
    return t;
}

extern int inline __attribute__((__gnu_inline__, __always_inline__, __artificial__)) measure_const() { 
    volatile unsigned long t;
    asm volatile (
    "mfence             \n\t"
    "lfence             \n\t"
    "rdtsc              \n\t"
    "lfence             \n\t"
    "mov %%rax, %%rsi   \n\t"
//  "movb (%1), %%al    \n\t"   Removing the memory load
    "rdtscp             \n\t"
    "lfence             \n\t"
    "sub %%rsi, %%rax   \n\t"
    : "=a" (t):
    :  "%rdx", "%rcx", "%rsi");
    return t;
}

void report_result(int time) {

    char buff[16];
    int l = sprintf(buff, "%d ", time);
    fwrite(buff, 1, l, log_file);
}

int main() {

    log_file = fopen("log.txt", "a");
    if ( log_file == NULL ) {
        printf("fopen error\n");
        exit(0);
    }

    char* addr = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    int set = SET((uint64_t)addr);
    char temp;
    int time;

    char* set_filler[8];
    for ( int i = 0; i < 8; i++ ) {
        set_filler[i] = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
        //Asserting that these pages will map to the same set as addr
        assert(set == SET((uint64_t)set_filler[i]));
    }

//-------------------------Measuring measurement induced latency-------------------------
    time = measure_const();
    report_result(time);

//-------------------------------------Main memory---------------------------------------
    asm volatile(
        "lfence\n\t"
        "clflush (%0)"
        :: "c" (addr)
    );
    time = measure(addr);
    report_result(time);

//-----------------------------------------L1--------------------------------------------
    temp = addr[5];
    time = measure(addr);
    report_result(time);

//-----------------------------------------L2--------------------------------------------
    //Evicting addr block from L1 cache
    for ( int i = 0; i < 8; i++ ) {
        for( int j = 0; j < 50; j++) {
            temp = set_filler[i][j];
        }
    }
    time = measure(addr);
    report_result(time);

//-----------------------------------------L3--------------------------------------------
    //Hoping for processor switch/eviction of addr from L1 and L2
    sleep(0.5);
    //Accessing random memory address in range [64, 4095] to bring the page in TLB but not bring the addr[0:64] block in L1 cache
    temp = addr[1111];
    time = measure(addr);
    report_result(time);
    
//---------------------------------------------------------------------------------------
    fwrite("\n", 1, 1, log_file);
    fclose(log_file);

}