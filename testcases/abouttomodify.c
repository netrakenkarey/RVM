/* Check rvm_about_to_modify - If segments are the same as mentioned in rvm_begin_trans
  */

#include "rvm.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_STRING1 "hello, world"
#define TEST_STRING2 "bleg!"
#define OFFSET2 1000


int main(int argc, char **argv)
{
     rvm_t rvm;
     char *seg;
     char *segs[1];
     trans_t trans;
     
     rvm = rvm_init("rvm_segments");
     
     rvm_destroy(rvm, "testseg");
     
     segs[0] = (char *) rvm_map(rvm, "testseg", 10000);
     seg = segs[0];

     /* Check if modifications to the same memory area can be made*/
     trans = rvm_begin_trans(rvm, 1, (void**) segs);
     rvm_about_to_modify(trans, seg, 0, 100);
     sprintf(seg, TEST_STRING1);
     
     rvm_about_to_modify(trans, seg, 0, 100);
     sprintf(seg+OFFSET2, TEST_STRING1);
     
     rvm_commit_trans(trans);

     if(strcmp(seg, TEST_STRING1)) {
	  printf("ERROR: same memory area not being modified multiple times (%s)\n",
		 seg);
	  exit(2);
     }
 

     rvm_unmap(rvm, seg);
     printf("OK\n");
     exit(0);
}

