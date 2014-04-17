using namespace std;
#include "rvm.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>


struct segment {
	rvm_t rvm;
	int mapped;
	char *segname;
	void *segment;
	char *segpath;
	int segsize;
    trans_t transaction;
	struct segment *next;
};


int rvmid_count = 0;
int number_dir = 0;
int segmentlistflag = 0;
FILE *backstore;


char *logfile_path;
 
struct segment *Head;

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
        if (Segments->segment == segbase)
		{
			Segments->mapped = 0;
			Segments->segpath = NULL;
			return;
		}

	}
	printf("Segment not in mapped virtual memory");
}

void rvm_destroy(rvm_t rvm, const char *segname)
{
}

struct transaction {
    trans_t id;
    rvm_t rvm;
    int numsegs;
    void **segbases;
    struct transaction *next;
};

static trans_t counter = 0;
static struct transaction *trans_begin;
static struct transaction *trans_end;

static struct transaction* find_trans(trans_t tid)
{
    struct transaction *trans = trans_begin;
    while (trans) {
        if (trans->id == tid) {
            return trans;
        }
        trans = trans->next;
    }
    return NULL;
}

static void add_trans_to_list(struct transaction *trans)
{
    trans->next = NULL;
    if (trans_end) {
        trans_end->next = trans;
        trans_end = trans;
    } else {
        trans_begin = trans_end = trans;
    }
}

static void remove_trans_from_list(trans_t tid)
{
    struct transaction *prev = trans_begin;
    struct transaction *trans = prev->next;
    if (prev) {
        if (prev->id == tid) {
            if (trans_end == trans_begin) {
                trans_end = NULL;
                trans_begin = NULL;
            } else {
                trans_begin = prev->next;
            }
            free(prev);
        }
        while (trans) {
            if (trans->id == tid) {
                free(trans);
                prev->next = trans->next;
                if (trans == trans_end) {
                    trans_end = prev;
                }
                break;
            }
            prev = trans;
            trans = trans->next;
        }
    }
}

trans_t rvm_begin_trans(rvm_t rvm, int numsegs, void **segbases) 
{
    // check if one of the segments is already in transaction
    struct segment *segment = Head;
    while (segment) {
        for (int i = 0; i < numsegs; i++) {
            if (segment->segment == segbases+i && segment->transaction != -1) {
                return (trans_t) -1;
            }
        }
        segment = segment->next;
    }

    // add new transaction to list
    struct transaction *trans = (transaction *) malloc(sizeof(struct transaction));
    trans->id = counter++;
    trans->rvm = rvm;
    trans->numsegs = numsegs;
    trans->segbases = segbases;
    add_trans_to_list(trans);
    return (trans_t) -1;
}

struct undo_record {
    trans_t tid;
    void *segbase;
    int offset;
    int size;
    void *data;
    struct undo_record *next;
};

static struct undo_record *undo_begin;
static struct undo_record *undo_end;

static void add_undo_to_list(struct undo_record *undo)
{
    undo->next = NULL;
    if (undo_end) {
        undo_end->next = undo;
        undo_end = undo;
    } else {
        undo_begin = undo_end = undo;
    }
}

static void remove_undo_from_list(struct undo_record *undo)
{
}

void rvm_about_to_modify(trans_t tid, void *segbase, int offset, int size)
{
    struct undo_record *record;
    record = (undo_record *) malloc(sizeof(struct undo_record));
    record->tid = tid;
    record->segbase = segbase;
    record->offset = offset;
    record->size = size;
    record->data = malloc(size);
    memcpy(record->data, ((char *) segbase) + offset, size);
    add_undo_to_list(record);
}

static void save_log(struct undo_record *record)
{
}

void rvm_commit_trans(trans_t tid)
{
    struct undo_record *record = undo_begin;
    struct undo_record *next;
    while (record) {
        if (record->tid == tid) {
            save_log(record);
            next = record->next;
            remove_undo_from_list(record);
            record = next;
        } else {
            record = record->next;
        }
    }

    struct transaction *transaction = find_trans(tid);
    remove_trans_from_list(tid);
}

void rvm_abort_trans(trans_t tid)
{
    struct transaction *transaction = find_trans(tid);
    remove_trans_from_list(tid);
}

void rvm_truncate_log(rvm_t rvm)
{
}
