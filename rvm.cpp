#include "rvm.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>


struct segment
{
	rvm_t rvm;
	int mapped;
	char *segname;
	char *segment;
	char *segpath;
	int segsize;
	struct segment *next;
};


int rvmid_count = 0;
int number_dir = 0;
int segmentlistflag = 0;
FILE *backstore;


char *logfile_path ;
 
struct segment *Head = NULL;

rvm_t rvm_init(const char *directory)
{
	rvm_t rvm;
	char temp[200];
	struct stat filestat;

	
	logfile_path =	(char*)malloc(200*sizeof(char));
	
	strcpy(temp , "mkdir ");
	strcat(temp , directory);

	strcpy(rvm.dir,directory);
	rvm.rvmid = ++rvmid_count;
	
	int filestatus = stat(directory, &filestat);
	
	if( filestatus >= 0)
		printf("Directory already exists");
	else
	{
		system(temp);
	}

	strcpy(temp , directory);
	strcat(temp , "/logfile");
	
	strcpy(logfile_path , temp);
	
	number_dir++;

	return rvm;
}

void *rvm_map(rvm_t rvm, const char *segname, int size_to_create)
{
	if(rvm.dir == NULL)
	{
		printf("Empty Directory");exit(-1);
	}
	
	struct segment *N = (struct segment *)malloc(sizeof(struct segment));
	struct stat filestat;
	char temp[200];
	struct segment *Segments = NULL,

	strcpy(temp, rvm.dir);
	strcat(temp,"/");
	strcat(temp,segname);
	int filestatus = stat(temp, &filestat);

	if(segmentlistflag == 0) //Linked list empty
	{
	//Enter first element into the linked list

	if( filestatus >=0) //File exists in directory
	{
		backstore = fopen(temp,"r");
	
		Segments = (struct segment *)malloc(sizeof(struct segment));
     	  	Segments->segment = (char*)malloc(sizeof(char)*size_to_create);
		Segments->segpath = (char*)malloc(sizeof(char)*100);
		Segments->segname=(char*)malloc(sizeof(char)*100);
	
		strcpy(Segments->segpath,segname);
		strcpy(Segments->segname,segname);
	
		Segments->mapped = 1;
		Segments->rvm = rvm;
		Segments->next = NULL;
		Head = Segments;
		segmentlistflag=1;

		fclose(backstore);
		fflush(backstore);
	
	
	}
	else
	{}

	return Segments->segment;
	}
	else
	{
	if(filestatus >=0)
	{
		backstore = fopen(temp,"r");
		Segments = Head;		
	
		while(Segments != NULL)
		{
			if(strcmp(Segments->segname,segname) == 0)
			{
				if(Segments->segsize < 	size_to_create)
				{
					Segments->segment = realloc(Segments->segment,sizeof(char)*size_to_create);
					return Segments->segment;
				}
				
			
			}	
			if(Segments->next == NULL)
				break;
			Segments = Segments->next;
		
		}
		N->segment =(char*)malloc(sizeof(char)*size_to_create);
		N->segpath = (char*)malloc(sizeof(char)*100);
		N->rvm = rvm;

		N->segname=(char*)malloc(sizeof(char)*100);
		strcpy(N->segpath,segname);


		strcpy(N->segname,segname);
		N->mapped = 1;
		N->next = NULL;
		
		fclose(backstore);
		fflush(backstore);
	
	}
	else
	{}
	Segments->next = N;
	Segments = Segments->next;


	return Segments->segment;
		
	}
	


}


void rvm_unmap(rvm_t rvm, void *segbase)
{


}


int main()
{
	rvm_init("backstore");
}




