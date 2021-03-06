# FPGA-alike software for short read alignment using FM-index 

In the repository `EMBothWay`, DNA alignment (exact-matching) using FM-index is accelerated on Xilinx VU9P using MaxJ. This software aims at generating the corresponding FM-index, and providing FPGA-alike software for testing and debugging purposes.

## Index Generation
```
# cd SW/fm-sa-build
# .fm-sa-build <reference_fasta> <index_name_you_like> true
```
8 index files are generated. <index_name_you_like>.1.ral to <index_name_you_like>.8.ral

## Exact-Match Alignment
```
# cd ../SW/fm-align
# .fm-sa-build ../fm-sa-build/<index_name_you_like> <read>.fq
```
Using the 8 index files, short read alignment can be performed.

## Principle
### FM-Index
FM-index [[1]](#1) is a commonly used alignment algorithm in state-of-the-art software such as Bowtie. It is a space-efficient data structure that combines the properties of suffix array with the Burrows-Wheeler transform (BWT) [[2]](#2). This data structure provides an efficient mechanism to perform substring matching of a pattern `P` in a long reference sequence `R`. 

The generation of FM-index begins with the computation of BWT of the reference genome `R`, i.e. `BWT(R)`.
First, `R` is terminated with a unique character: `$`, which is lexicographically the smallest value. Then, all the rotations of the text are obtained and sorted correspondingly. The suffix array can be obtained by considering the characters before `$` in each entry of the rotation list. `BWT(R)` can be formed by extracting and concatenating the last characters of all the entries on the sorted list. The following table demonstrates the derivation of BWT with an example reference genome `R=ACACGT.`. The string preceding the `$` sign in the sorted rotations forms the suffix array (`SA`), which indicates the position of each possible suffix in the original string.

index *n* | SA | Sorted Rotations
------| ---| -----------
0 | 6 |  $ACACGT
1 | 0 |  **ACACGT**$
2 | 2 |  **ACGT**$AC
3 | 1 |  **CACGT**$A
4 | 3 |  **CGT**$ACA
5 | 4 |  **GT**$ACAC
5 | 5 |  **T**$ACACG

After having the suffix array, `BWT(R)` is sorted to form the `i` and `c` tables. For each element `x` of the alphabet of `R`, `i(x)` is defined as the index of its first occurrence in `sorted-BWT(R)`. For each index `n` in `BWT(R)` and for each character `x` in the alphabet, `c(n, x)` stores the number of occurrences of `x` in `BWT(R)` in the range `[0, n-1]`. The following table illustrates the `i(x)` and `c(n, x)` tables for the sequence `R`.

`c(n, x)`
index *n* | A | C | G | T
----------| - | - | - | -
0 | 0 | 0 | 0 | 0
1 | 0 | 0 | 0 | 1
2 | 0 | 0 | 0 | 1
3 | 0 | 1 | 0 | 1
4 | 1 | 1 | 0 | 1
5 | 2 | 1 | 0 | 1
6 | 2 | 2 | 0 | 1
7 | 2 | 2 | 1 | 1

`i(x)`
A | C | G | T
-- | -- | -- | --
1 | 3 | 5 | 6

Alignment of reads with FM-index operates on the functions `i(x)` and `c(n, x)` recursively. Two pointers `top` and `bottom` are defined to perform the search. `top` refers to an index of the suffix array element where a specific pattern is first located, and `bottom` is the location where the pattern can be last found. If `bottom` points to an index that is less than or equal to the index pointed by the `top`, the pattern does not occur on the text.

To search for a specific pattern `P` with the FM-index, one character is processed at a time, starting with the last character of `P`. The `top` and `bottom` are first initialized with the first and last indices of the `c(n, x)` function respectively. Then both pointers are updated according to the following equations:

<a href="https://www.codecogs.com/eqnedit.php?latex=top_{new}&space;=&space;c(top_{current},&space;x)&space;&plus;&space;i(x)&space;\\&space;bottom_{new}&space;=&space;c(bottom_{current},&space;x)&space;&plus;&space;i(x)" target="_blank"><img src="https://latex.codecogs.com/gif.latex?top_{new}&space;=&space;c(top_{current},&space;x)&space;&plus;&space;i(x)&space;\\&space;bottom_{new}&space;=&space;c(bottom_{current},&space;x)&space;&plus;&space;i(x)" title="top_{new} = c(top_{current}, x) + i(x) \\ bottom_{new} = c(bottom_{current}, x) + i(x)" /></a>

The time complexity of locating a pattern in the reference genome is in linear of the length `P` instead of `R`.

The following example demonstrates an example of locating the sequence `S=CG` in the reference `R=ACACGT`. top <sub>current</sub> and bottom <sub>current</sub> are first initialized to 0 and 7 respectively. Then equation above is applied based on the character in `S`.

<img src="https://github.com/nghocheung/FM-Smith-SW/blob/master/eq.png" width="400">

After the second iteration, all the characters in S are exhausted and the final top is smaller than the bottom, i.e. `4<5`. Since the suffix array element corresponds to the location of each suffix in the reference `R`, the location of `S` is 3 in the reference when the final `top` and `bottom-1` are mapped onto the suffix array.

### Our Compressed FM-Index
The alignment process begins with streaming the reads from the host and initializing the pointers `Top` and `Bottom`. These pointers are updated based on the current character and the correlated `i(x)` and `c(n, x)` values from the onboard memory. A command block is responsible for sending memory requests based on the new pointer values.

<img src="https://github.com/nghocheung/FM-Smith-SW/blob/master/bucket.png" width="600">

When the human reference genome is converted into FM-index, the resulting table `c(n, x)` is around \SI{51}{\giga\byte}. This is often far larger than the capacity of onboard memory. To compress the index size so that multiple copies of the index can be associated with multiple kernels, we only store a subset of `c(n, x)`. The remaining entries are substituted with a portion of the original BWT. Intrinsically, we sample every `d` entry of `c(n, x)` and pack the BWT in the range of every `d` and `d-1` alongside, forming a bucket. Normally we set the bucket being a multiple of the burst size to fully utilize the memory bandwidth. During a character search, the missing entries can be recalculated on-the-fly using the ranged BWT. The figure above displays the final compressed index and the required memory storage is significantly reduced to:

<a href="https://www.codecogs.com/eqnedit.php?latex=\mbox{Sampled&space;}&space;c(n,&space;x)&space;\mbox{&space;Size}&space;&plus;&space;\mbox{Sampled&space;}&space;BWT&space;\mbox{&space;Size}\\&space;=&space;\frac{3.2G\times32bit}{d}\times4&plus;3.2G\times2bit\\&space;=&space;(\frac{51.2}{d}&plus;0.8)GB" target="_blank"><img src="https://latex.codecogs.com/gif.latex?\mbox{Sampled&space;}&space;c(n,&space;x)&space;\mbox{&space;Size}&space;&plus;&space;\mbox{Sampled&space;}&space;BWT&space;\mbox{&space;Size}\\&space;=&space;\frac{3.2G\times32bit}{d}\times4&plus;3.2G\times2bit\\&space;=&space;(\frac{51.2}{d}&plus;0.8)GB" title="\mbox{Sampled } c(n, x) \mbox{ Size} + \mbox{Sampled } BWT \mbox{ Size}\\ = \frac{3.2G\times32bit}{d}\times4+3.2G\times2bit\\ = (\frac{51.2}{d}+0.8)GB" /></a>

## References
<a id="1">[1]</a> 
 P. Ferragina and G. Manzin,
"An Experimental Study of an Opportunistic Index,"
12th Annual ACM-SIAM Symposium on Discrete Algorithms, 2001, pp. 269-278.

<a id="2">[2]</a> 
M. Burrows and D. Wheeler,
"Block-sorting Lossless Data Compression Algorithm,"
Digital Equipment Corporation, Tech. Rep., 1994.
