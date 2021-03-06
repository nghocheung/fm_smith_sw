//
// Created by hcng on 9/20/18.
//

#ifndef FM_SA_ALIGN_READS_H
#define FM_SA_ALIGN_READS_H



#include <vector>
#include "file_op.h"
#include <sys/time.h>



struct read_t {
    char seq[MAX_READ_LEN+1];
    uint32_t seq_len;

    // symbol in binary
    uint8_t pck_sym[CEIL(MAX_READ_LEN, FM_BP_RANGE)];

    std::string at_data;
    std::string q_score;

    char plus_data;

    uint32_t b_low;
    uint32_t b_high;

    uint32_t f_low;
    uint32_t f_high;

    bool is_b_align;
    bool is_f_align;
    bool has_N;
};
/**
    Load the short reads for alignment

    @param  *fp         File descriptor for the short reads
    @param  &reads              vector that contains a batch of short reads
    @param  *buffer             (pointer) general buffer used to cache everything
    @param  *size_r             (pointer) size that I wants to read
    @param  *bytes_r            (pointer) accumulated size read
*/
void loadReads(FILE *fp, std::vector<read_t> &reads, char *buffer, uint64_t size_r, bool r_ctrl, uint64_t *bytes, uint64_t * n_Nchar);
uint64_t writeReads(FILE *fp, std::vector<read_t> &reads, char *buffer);


#endif //FM_SA_ALIGN_READS_H
