/* rvm_destroy - Check if segment is already mapped and if segment was never created in the backing store 
	Check if correct output messages are printed*/

#include "rvm.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_STRING1 "hello, world"
#define TEST_STRING2 "bleg!"

#define SEGNAME  "checkDestroySegment"

#define OFFSET2 1000


int main(int argc, char **argv)
{
     rvm_t rvm;
     char *seg;
     char *segs[1];
     trans_t trans;
     
     rvm = rvm_init("rvm_segments");
     
     rvm_destroy(rvm, SEGNAME); // Error message should be displayed as segment does not exist in backing store
     
     segs[0] = (char *) rvm_map(rvm, SEGNAME, 10000);
     seg = segs[0];

     rvm_destroy(rvm, SEGNAME);// Error message should be displayed as mapped segments cannot be destroyed
	
     rvm_unmap(rvm, seg);
     printf("OK\n");
     exit(0);
}

