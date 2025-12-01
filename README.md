# Assignment 3 — SYSC 4001
## Ezo Otuonye 101306172

This repository contains all of the work completed for Assignment 3. The assignment focused on shared memory, process creation, synchronization with semaphores, and simulating multiple TAs marking exams at the same time.

## Part 2
This part required building two versions of the TA-marking system.

### Part 2a
The first version uses shared memory and multiple TA processes but does not use any synchronization. This intentionally allows race conditions so we can observe inconsistent behaviour when TAs modify the rubric, choose questions, and load exams at the same time.

### Part 2b
The second version adds semaphores to protect the three critical areas of the program: modifying the rubric, selecting questions, and loading the next exam. This version prevents the races seen in Part 2a and ensures that the system runs without deadlock or livelock.

Both versions were tested using different numbers of TA processes. In every run, the program successfully reached the sentinel student (9999) and terminated normally.

## Report
The written report includes:
- A discussion of race conditions in 2a  
- An explanation of how semaphores fix the problem in 2b  
- Testing notes showing that both versions terminate correctly  
- The answers for Part 3 (page replacement and TLB calculations)

## Input Files
All required input files for testing are included in the input folder
This includes:
- rubric.txt
- exam_list.txt
- exam01.txt through exam20.txt

Each exam file starts with the student number on the first line.

## How to Compile

gcc -Wall -O2 part2a.c -o part2a\
gcc -Wall -O2 part2b.c -o part2b -pthread

## How to Run

./part2a 3 input/rubric.txt input/exam_list.txt\
./part2b 3 input/rubric.txt input/exam_list.txt
