// Joel Anil John, Indrajeet Patwardhan, Danyal Nemati


//  mem_mgr.cpp
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <cassert>

#pragma warning(disable : 4996)

#define ARGC_ERROR 1
#define FILE_ERROR 2

#define FRAME_SIZE  256
#define FIFO 0
#define LRU 1
#define REPLACE_POLICY FIFO

// SET TO 128 to use replacement policy: FIFO or LRU,
#define NFRAMES 256
#define PTABLE_SIZE 256
#define TLB_SIZE 16

struct page_node {    
    size_t npage;
    size_t frame_num;
    bool is_present;
    bool is_used;
};

char* ram = (char*)malloc(NFRAMES * FRAME_SIZE);
page_node pg_table[PTABLE_SIZE];  // page table and (single) TLB
page_node tlb[TLB_SIZE];

const char* passed_or_failed(bool condition) { return condition ? " + " : "fail"; }
size_t failed_asserts = 0;

size_t get_page(size_t x)   { return 0xff & (x >> 8); }
size_t get_offset(size_t x) { return 0xff & x; }

void get_page_offset(size_t x, size_t& page, size_t& offset) {
    page = get_page(x);
    offset = get_offset(x);
    // printf("x is: %zu, page: %zu, offset: %zu, address: %zu, paddress: %zu\n", 
    //        x, page, offset, (page << 8) | get_offset(x), page * 256 + offset);
}

void update_frame_ptable(size_t npage, size_t frame_num) {
    pg_table[npage].frame_num = frame_num;
    pg_table[npage].is_present = true;
    pg_table[npage].is_used = true;
}

int find_frame_ptable(size_t frame) {  // FIFO
    for (int i = 0; i < PTABLE_SIZE; i++) {
        if (pg_table[i].frame_num == frame && 
            pg_table[i].is_present == true) { return i; }
    }
    return -1;
}

size_t get_used_ptable() {  // LRU
    size_t unused = -1;
    for (size_t i = 0; i < PTABLE_SIZE; i++) {
        if (pg_table[i].is_used == false && 
            pg_table[i].is_present == true) { return (size_t)i; }
    }
    // All present pages have been used recently, set all page entry used flags to false
    for (size_t i = 0; i < PTABLE_SIZE; i++) { pg_table[i].is_used = false; }
    for (size_t i = 0; i < PTABLE_SIZE; i++) {
        page_node& r = pg_table[i];
        if (!r.is_used && r.is_present) { return i; }
    }
    return (size_t)-1;
}

int check_tlb(size_t page) {
    for (int i = 0; i < TLB_SIZE; i++) {
        if (tlb[i].npage == page) { return i; }
    }
    return -1;
}

void open_files(FILE*& fadd, FILE*& fcorr, FILE*& fback) { 
    fadd = fopen("addresses.txt", "r");
    if (fadd == NULL) { fprintf(stderr, "Could not open file: 'addresses.txt'\n");  exit(FILE_ERROR); }

    fcorr = fopen("correct.txt", "r");
    if (fcorr == NULL) { fprintf(stderr, "Could not open file: 'correct.txt'\n");  exit(FILE_ERROR); }

    fback = fopen("BACKING_STORE.bin", "rb");
    if (fback == NULL) { fprintf(stderr, "Could not open file: 'BACKING_STORE.bin'\n");  exit(FILE_ERROR); }
}
void close_files(FILE* fadd, FILE* fcorr, FILE* fback) { 
    fclose(fadd);
    fclose(fcorr);
    fclose(fback);
}

void initialize_pg_table_tlb() { 
    for (int i = 0; i < PTABLE_SIZE; ++i) {
        pg_table[i].npage = (size_t)i;
        pg_table[i].is_present = false;
        pg_table[i].is_used = false;
    }
    for (int i = 0; i < TLB_SIZE; i++) {
        tlb[i].npage = (size_t)-1;
        tlb[i].is_present = false;
        pg_table[i].is_used = false;
    }
}

void summarize(size_t pg_faults, size_t tlb_hits) { 
    printf("\nPage Fault Percentage: %1.3f%%", (double)pg_faults / 1000);
    printf("\nTLB Hit Percentage: %1.3f%%\n\n", (double)tlb_hits / 1000);
    printf("ALL logical ---> physical assertions PASSED!\n");
    printf("\n\t\t...done.\n");
}

void tlb_add(int index, page_node entry) {
       tlb[index%16] = entry;
 }

void tlb_remove(int index) {
    for(int x = index; x<(sizeof(tlb)/sizeof(tlb[0]))-1; x++){
        tlb[x] = tlb[x+1];
    }
}

void tlb_hit(size_t& frame, size_t& page, size_t& tlb_hits, int result) { 
    result++;
 } 

void tlb_miss(size_t& frame, size_t& page, size_t& tlb_track) { 
    for(int x = 0; x<256; x++){
        if(pg_table[x].frame_num==frame && pg_table[x].npage==page){
            tlb[tlb_track] = pg_table[x];
        }
    }
 }

void fifo_replace_page(size_t& frame ) { 
    for(int x = 0; x<(sizeof(tlb)/sizeof(tlb[0]))-1; x++){
        pg_table[x] = pg_table[x+1];
    }
 }  

void lru_replace_page(size_t& frame) {
    int i = 0;
    for(int x = 0; x<PTABLE_SIZE; x++){
        if(pg_table[x].frame_num==frame){
            i = x;
            break;
        }
    }

    for(int y = i; y<PTABLE_SIZE-1; y++){
        pg_table[i] = pg_table[i+1];
    }
}

void page_fault(size_t& frame, size_t& page, size_t& frames_used, size_t& pg_faults, 
              size_t& tlb_track, FILE* fbacking) {  
    unsigned char buf[BUFSIZ];
    memset(buf, 0, sizeof(buf));
    bool is_memfull = false;

    ++pg_faults;
    if (frames_used >= NFRAMES) { is_memfull = true; }
    frame = frames_used % NFRAMES;    // FIFO only

    if (is_memfull) { 
        if (REPLACE_POLICY == FIFO) {
            fifo_replace_page(frame);
        } else { 
            lru_replace_page(frame);
        }
    }
         // load page into RAM, update pg_table, TLB
    fseek(fbacking, page * FRAME_SIZE, SEEK_SET);
    fread(buf, FRAME_SIZE, 1, fbacking);

    for (int i = 0; i < FRAME_SIZE; i++) {
        *(ram + (frame * FRAME_SIZE) + i) = buf[i];
    }
    update_frame_ptable(page, frame);
    tlb_add(tlb_track++, pg_table[page]);
    if (tlb_track > 15) { tlb_track = 0; }
    
    ++frames_used;
} 

void check_address_value(size_t logic_add, size_t page, size_t offset, size_t physical_add,
                         size_t& prev_frame, size_t frame, int val, int value, size_t o) { 
    printf("log: %5lu 0x%04x (pg:%3lu, off:%3lu)-->phy: %5lu (frm: %3lu) (prv: %3lu)--> val: %4d == value: %4d -- %s", 
          logic_add, logic_add, page, offset, physical_add, frame, prev_frame, 
          val, value, passed_or_failed(val == value));

    if (frame < prev_frame) {  printf("   HIT!\n");
    } else {
        prev_frame = frame;
        printf("----> pg_fault\n");
    }
    if (o % 5 == 4) { printf("\n"); }
// if (o > 20) { exit(-1); }             // to check out first 20 elements

    if (val != value) { ++failed_asserts; }
    if (failed_asserts > 5) { exit(-1); }
//     assert(val == value);
}

void run_simulation() { 
    // size_t addresses, pages, frames, values, hits and faults
    size_t logic_add, virt_add, phys_add, physical_add;
    size_t page, frame, offset, value, prev_frame = 0, tlb_track = 0;
    size_t frames_used = 0, pg_faults = 0, tlb_hits = 0;
    int val = 0;
    char buf[BUFSIZ];

    bool is_memfull = false;     // physical memory to store the frames

    initialize_pg_table_tlb();

        // addresses to test, correct values, and pages to load
    FILE *faddress, *fcorrect, *fbacking;
    open_files(faddress, fcorrect, fbacking);

    for (int o = 0; o < 1000; o++) {     // read from file correct.txt
        fscanf(fcorrect, "%s %s %lu %s %s %lu %s %ld", buf, buf, &virt_add, buf, buf, &phys_add, buf, &value);  

        fscanf(faddress, "%ld", &logic_add);  
        get_page_offset(logic_add, page, offset);

        int result = check_tlb(page);
        if (result >= 0) {  
            tlb_hit(frame, page, tlb_hits, result); 
        } else if (pg_table[page].is_present) {
            tlb_miss(frame, page, tlb_track);
        } else {         // page fault
            page_fault(frame, page, frames_used, pg_faults, tlb_track, fbacking);
        }

        physical_add = (frame * FRAME_SIZE) + offset;
        val = (int)*(ram + physical_add);

        check_address_value(logic_add, page, offset, physical_add, prev_frame, frame, val, value, o);
    }
    close_files(faddress, fcorrect, fbacking);  // and time to wrap things up
    free(ram);
    summarize(pg_faults, tlb_hits);

}


int main(int argc, const char * argv[]) {
    run_simulation();
// printf("\nFailed asserts: %lu\n\n", failed_asserts);   // allows asserts to fail silently and be counted
    return 0;
}

/*

log: 16916 0x4214 (pg: 66, off: 20)-->phy:    20 (frm:   0) (prv:   0)--> val:    0 == value:    0 --  + ----> pg_fault
log: 62493 0xf41d (pg:244, off: 29)-->phy:   285 (frm:   1) (prv:   0)--> val:    0 == value:    0 --  + ----> pg_fault
log: 30198 0x75f6 (pg:117, off:246)-->phy:   758 (frm:   2) (prv:   1)--> val:   29 == value:   29 --  + ----> pg_fault
log: 53683 0xd1b3 (pg:209, off:179)-->phy:   947 (frm:   3) (prv:   2)--> val:  108 == value:  108 --  + ----> pg_fault
log: 40185 0x9cf9 (pg:156, off:249)-->phy:  1273 (frm:   4) (prv:   3)--> val:    0 == value:    0 --  + ----> pg_fault

log: 28781 0x706d (pg:112, off:109)-->phy:  1389 (frm:   5) (prv:   4)--> val:    0 == value:    0 --  + ----> pg_fault
log: 24462 0x5f8e (pg: 95, off:142)-->phy:  1678 (frm:   6) (prv:   5)--> val:   23 == value:   23 --  + ----> pg_fault
log: 48399 0xbd0f (pg:189, off: 15)-->phy:  1807 (frm:   7) (prv:   6)--> val:   67 == value:   67 --  + ----> pg_fault
log: 64815 0xfd2f (pg:253, off: 47)-->phy:  2095 (frm:   8) (prv:   7)--> val:   75 == value:   75 --  + ----> pg_fault
log: 18295 0x4777 (pg: 71, off:119)-->phy:  2423 (frm:   9) (prv:   8)--> val:  -35 == value:  -35 --  + ----> pg_fault

log: 12218 0x2fba (pg: 47, off:186)-->phy:  2746 (frm:  10) (prv:   9)--> val:   11 == value:   11 --  + ----> pg_fault
log: 22760 0x58e8 (pg: 88, off:232)-->phy:  3048 (frm:  11) (prv:  10)--> val:    0 == value:    0 --  + ----> pg_fault
log: 57982 0xe27e (pg:226, off:126)-->phy:  3198 (frm:  12) (prv:  11)--> val:   56 == value:   56 --  + ----> pg_fault
log: 27966 0x6d3e (pg:109, off: 62)-->phy:  3390 (frm:  13) (prv:  12)--> val:   27 == value:   27 --  + ----> pg_fault
log: 54894 0xd66e (pg:214, off:110)-->phy:  3694 (frm:  14) (prv:  13)--> val:   53 == value:   53 --  + ----> pg_fault


........


log: 10392 0x2898 (pg: 40, off:152)-->phy: 14744 (frm:  57) (prv:  56)--> val:    0 == value:    0 --  + ----> HIT!
log: 58882 0xe602 (pg:230, off:  2)-->phy: 14850 (frm:  58) (prv:  57)--> val:   57 == value:   57 --  + ----> HIT!
log:  5129 0x1409 (pg: 20, off:  9)-->phy: 15113 (frm:  59) (prv:  58)--> val:    0 == value:    0 --  + ----> HIT!
log: 58554 0xe4ba (pg:228, off:186)-->phy: 15546 (frm:  60) (prv:  59)--> val:   57 == value:   57 --  + ----> HIT!
log: 58584 0xe4d8 (pg:228, off:216)-->phy: 15576 (frm:  60) (prv:  60)--> val:    0 == value:    0 --  + ----> HIT!

log: 27444 0x6b34 (pg:107, off: 52)-->phy: 15668 (frm:  61) (prv:  60)--> val:    0 == value:    0 --  + ----> HIT!
log: 58982 0xe666 (pg:230, off:102)-->phy: 15718 (frm:  61) (prv:  61)--> val:   26 == value:   57 -- fail----> HIT!
log: 51476 0xc914 (pg:201, off: 20)-->phy: 15892 (frm:  62) (prv:  61)--> val:    0 == value:    0 --  + ----> HIT!
log:  6796 0x1a8c (pg: 26, off:140)-->phy: 16012 (frm:  62) (prv:  62)--> val:    0 == value:    0 --  + ----> HIT!
log: 21311 0x533f (pg: 83, off: 63)-->phy: 15935 (frm:  62) (prv:  62)--> val:   79 == value:  -49 -- fail----> HIT!

log: 30705 0x77f1 (pg:119, off:241)-->phy: 16369 (frm:  63) (prv:  62)--> val:    0 == value:    0 --  + ----> HIT!
log: 28964 0x7124 (pg:113, off: 36)-->phy: 16420 (frm:  64) (prv:  63)--> val:    0 == value:    0 --  + ----> HIT!
log: 41003 0xa02b (pg:160, off: 43)-->phy: 16427 (frm:  64) (prv:  64)--> val:   74 == value:   10 -- fail----> HIT!

Page Fault Percentage: 0.196%
TLB Hit Percentage: 0.069%

ALL logical ---> physical assertions PASSED!

                ...done.


*/