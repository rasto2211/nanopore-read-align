#!/bin/bash

# Input file contains ref. read, empty line, lines with sequences. 
# Every sequence is on a separate line. 
# These sequences are compared against the ref. read.
# Input file is the first and last argument.

file=$1
file_without_ext=${file%.tmp}

# First line of input file contains ref. seq.
echo ">ref" > ${file_without_ext}_ref_seq.fa
head -1 $file >> ${file_without_ext}_ref_seq.fa

bwa index ${file_without_ext}_ref_seq.fa 2>/dev/null

# 2nd line of input is empty. 3rd line contains first seq. which we want to
# compare.
seq_num=0
echo "bwa_identity" > ${file_without_ext}_bwa_identity.csv
for line in `sed -n '3,$p' < $file`;
do
  suffix=`printf "seq%03d\n" $seq_num`

  # Create fasta file.
  echo ">seq" > ${file_without_ext}_${suffix}.fasta
  echo $line >> ${file_without_ext}_${suffix}.fasta

  bwa mem -x ont2d ${file_without_ext}_ref_seq.fa\
  ${file_without_ext}_${suffix}.fasta\
  > ${file_without_ext}_${suffix}.sam 2>/dev/null

  # Print the greatest identity or NA.
  # python3 get_bwa_stats.py ${file_without_ext}_temp.sam |
  # python3 choose_greatest_identity_alignment.py >>\
  # ${file_without_ext}_bwa_identity.csv

  echo "${file_without_ext}_${suffix}.sam"
  let "seq_num+=1"
done

# Delete temp. files.
#rm ${file_without_ext}_temp.fa
#rm ${file_without_ext}_temp.sam
#rm ${file_without_ext}_ref_seq.fa
