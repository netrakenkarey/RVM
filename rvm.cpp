using namespace std;
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
 
struct segment *Head ;

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

void* rvm_map(rvm_t rvm, const char *segname, int size_to_create)
{
	if(rvm.dir == NULL)
	{
		printf("Empty Directory");exit(-1);
	}
	
	struct segment *N = (struct segment *)malloc(sizeof(struct segment));
	struct stat filestat;
	char temp[200];
	struct segment *Segments = NULL;
	int crash = 0;

	strcpy(temp, rvm.dir);
	strcat(temp,"/");
	strcat(temp,segname);
	int filestatus = stat(temp, &filestat);

	if(Head == NULL) //Linked list empty
	{
	//Enter first element into the linked list

	Segments = (struct segment *)malloc(sizeof(struct segment));
       	Segments->segment = (char*)malloc(sizeof(char)*size_to_create);
	Segments->segpath = (char*)malloc(sizeof(char)*100);
	Segments->segname=(char*)malloc(sizeof(char)*100);

	if( filestatus >=0) //File exists in directory
	{
		backstore = fopen(temp,"r");
		//Here do I need to copy the data from the external file into the virtual memory i.e Segments->segment ?
	
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
	else//File does not exist in directory
	{
		
		backstore = fopen(temp,"w");
	
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

	return Segments->segment;
	}
	else //Linked List is not Empty
	{
	if(filestatus >=0)//File exists in directory.
	{
		backstore = fopen(temp,"r"); //Im not sure if something needs to be done with the external file opened at this stage
		Segments = Head;		
	
		while(Segments != NULL)
		{
			if(strcmp(Segments->segname,segname) == 0)
			{
				if(Segments->mapped == 1)
				{
					printf("ERROR ! Segment already mapped");
					exit(-1);
				}
				else
				{
					if(Segments->segsize < 	size_to_create)
					{
						Segments->segment = (char*)realloc(Segments->segment,sizeof(char)*size_to_create);
					}
					
					
				}
				Segments->mapped =1 ;
				Segments->segpath = NULL ; 
				fclose(backstore);
				fflush(backstore);
	
				return Segments->segment;			
			
			}	
			if(Segments->next == NULL)
			{
			crash =1;
			break ;
			}

		
		}
		
		if(crash ==1)
		{	//Crash Recovery code here
		}
		
		
		fclose(backstore);
		fflush(backstore);
	
	}
	else // File does not exist in directory
	{
		backstore = fopen(temp,"w");
		Segments = Head;
		while (Segments->next != NULL)
		{
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
		Segments->next = N;
		Segments = Segments->next;
		

		fclose(backstore);
		fflush(backstore);
	}
	

	
		
	}
	
return Segments->segment;

}

void rvm_unmap(rvm_t rvm, void *segbase)
{
	struct segment *Segments = Head;
	
	while(Segments != NULL)
	{
		if(strcmp(Segments->segment,(char*)segbase) == 0)
		{
			Segments->mapped = 0;
			Segments->segpath = NULL;
			return;
		}

	}
	printf("Segment not in mapped virtual memory");
}


int main()
{
	//rvm_t r;
	//rvm_init("backstore");
	//rvm_map(r , "segment1" , 100000);
}




