#include "rvm.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <vector>

using namespace std;

struct segment {
	rvm_t rvm;
	char *segname;
	void *segment;
	int segsize;
    trans_t tid;
	struct segment *next;
};

static int rvmid_count = 0;
static vector<segment> segments;

rvm_t rvm_init(const char *directory)
{
	rvm_t rvm;
	char temp[200];
	struct stat filestat;
	
	strcpy(temp , "mkdir ");
	strcat(temp , directory);

	strcpy(rvm.dir,directory);
    rvm.dir = strdup(directory);
	rvm.rvmid = ++rvmid_count;
	
	int filestatus = stat(directory, &filestat);
		
	if (filestatus >= 0) {
		printf("Directory already exists");
    } else {
		system(temp);
	}
	
	return rvm;
}

static void apply_log(void *segbase, char *log)
{
    strtok(log, " ");
    int offset = atoi(strtok(NULL, " "));
    int size = atoi(strtok(NULL, " "));
    void *data = strtok(NULL, " ");
    memcpy((char *) segbase + offset, data, size);
}

static void recover_data(const char *segname, void *segbase, int size, FILE *baseFp, FILE *logFp)
{
    char *line = NULL;
    ssize_t result;

    result = getline(&line, NULL, baseFp);
    if (result != -1) {
        memcpy(segbase, line, size);
    } else {
        memset(segbase, 0, size);
    }
    result = getline(&line, NULL, logFp);
    while (result != -1) {
        apply_log(segbase, line);
        free(line);
        line = NULL;
        result = getline(&line, NULL, logFp);
    }
}

void* rvm_map(rvm_t rvm, const char *segname, int size_to_create)
{
	if(rvm.dir == NULL) {
		printf("Empty Directory");
        exit(-1);
	}
	
	struct stat filestat;
	char temp[200];

	strcpy(temp, rvm.dir);
	strcat(temp,"/");
	strcat(temp,segname);
	int filestatus = stat(temp, &filestat);
    int recover = 0;

    // file already exists
    if (filestatus == 0) {
        for (unsigned int i = 0; i < segments.size(); ++i) {
            if (strcmp(segments[i].segname, segname) == 0) {
                // segment already exists
                if (segments[i].segsize != size_to_create) {
                    segments[i].segment = realloc(segments[i].segment, sizeof(char) * size_to_create);
                    segments[i].segsize = size_to_create;
                }
                return segments[i].segment;
            }
        }
        recover = 1;
    }

    struct segment seg;
    seg.segment = malloc(sizeof(char) * size_to_create);
    seg.segname = strdup(segname);
    seg.rvm = rvm;
    seg.segsize = size_to_create;
    seg.tid = -1;

    if (recover) {
        FILE *baseFp = fopen(temp, "r");
        memset(temp, 0, 200);
        strcpy(temp, rvm.dir);
        strcat(temp,"/logfile");
        FILE *logFp = fopen(temp, "r");
        recover_data(seg.segname, seg.segment, seg.segsize, baseFp, logFp);
        fclose(baseFp);
        fclose(logFp);
    }

    segments.push_back(seg);

    return seg.segment;
}

void rvm_unmap(rvm_t rvm, void *segbase)
{
    unsigned int index = -1;
    for (unsigned int i = 0; i < segments.size(); ++i) {
        if (segments[i].segment == segbase) {
            index = i;
            break;
        }
    }
    if (index >= 0) {
        free(segments[index].segment);
        segments.erase(segments.begin() + index);
    }
}

void rvm_destroy(rvm_t rvm, const char *segname)
{
	char temp[200];
	
    for (unsigned int i = 0; i < segments.size(); ++i) {
        if (strcmp(segments[i].segname, segname) == 0) {
			printf("\nMapped segment cannot be destroyed\n");
            return;
        }
    }

	strcpy(temp, rvm.dir);
	strcat(temp,"/");
	strcat(temp,segname);
	
	int res = remove(temp);
	if(res == 0) {
		printf("\nSegment destroyed\n");
	} else {
		printf("\nSegment backstore does not exist\n");
	}
}

struct transaction {
    trans_t id;
    rvm_t rvm;
    int numsegs;
    void **segbases;
};

static trans_t counter = 0;
static vector<transaction> transactions;

static int find_trans(trans_t tid)
{
    for (unsigned int i = 0; i < transactions.size(); ++i) {
        if (transactions[i].id == tid) {
            return i;
        }
    }
    return -1;
}

static void remove_trans_from_list(trans_t tid)
{
    int i = find_trans(tid);
    transactions.erase(transactions.begin() + i);
}

trans_t rvm_begin_trans(rvm_t rvm, int numsegs, void **segbases) 
{
    // check if one of the segments is already in transaction
    for (unsigned int i = 0; i < segments.size(); ++i) {
        for (int j = 0; j < numsegs; ++j) {
            if (segments[i].segment == segbases + j && segments[i].tid != -1) {
                return (trans_t) -1;
            }
        }
    }

    // add new transaction to list
    struct transaction trans;
    trans.id = counter++;
    trans.rvm = rvm;
    trans.numsegs = numsegs;
    trans.segbases = segbases;
    transactions.push_back(trans);
    return (trans_t) -1;
}

struct undo_record {
    trans_t tid;
    void *segbase;
    int offset;
    int size;
    void *data;
};

static vector<undo_record> undo_records;

void rvm_about_to_modify(trans_t tid, void *segbase, int offset, int size)
{
    struct undo_record record;
    record.tid = tid;
    record.segbase = segbase;
    record.offset = offset;
    record.size = size;
    record.data = malloc(size);
    memcpy(record.data, ((char *) segbase) + offset, size);
    undo_records.push_back(record);
}

static void save_log(void *segbase, int offset, int size, void *data)
{
    unsigned int i = -1;
    for (i = 0; i < segments.size(); ++i) {
        if (segments[i].segment == segbase) {
            break;
        }
    }
    if (i >= 0) {
        char path[256];
        snprintf(path, sizeof(path), "%s/logfile", segments[i].rvm.dir);
        FILE *f = fopen(path, "a");

        char *data_s = (char *) malloc(sizeof(char) * (size+1));
        memcpy(data_s, data, size);
        data_s[size] = 0;

        fprintf(f, "%s %d %d %s\n", segments[i].segname, offset, size, data_s);
        free(data_s);
        fclose(f);
    }
}

void rvm_commit_trans(trans_t tid)
{
    vector<undo_record>::iterator it = undo_records.begin();
    while (it != undo_records.end()) {
        if (it->tid == tid) {
            save_log(it->segbase, it->offset, it->size, it->data);
            free(it->data);
            it = undo_records.erase(it);
        } else {
            it++;
        }
    }
    remove_trans_from_list(tid);
}

void rvm_abort_trans(trans_t tid)
{
    vector<undo_record>::iterator it = undo_records.begin();
    while (it != undo_records.end()) {
        if (it->tid == tid) {
            save_log(it->segbase, it->offset, it->size, it->data);
            free(it->data);
            memcpy((char *)it->segbase + it->offset, it->data, it->size);
            it = undo_records.erase(it);
        } else {
            it++;
        }
    }
    remove_trans_from_list(tid);
}

static void truncate_log_line(rvm_t rvm, char *line)
{
    char *temp;
    char *segname = strtok(line, " ");

    char path[256];
    snprintf(path, sizeof(path), "%s/%s", rvm.dir, segname);
	struct stat filestat;
	int filestatus = stat(path, &filestat);
    if (filestatus == 0) {
        // read base file
        FILE *baseFp = fopen(path, "r");
        getline(&temp, NULL, baseFp);
        fclose(baseFp);

        int offset = atoi(strtok(NULL, " "));
        int size = atoi(strtok(NULL, " "));
        char *data = strtok(NULL, " ");

        memcpy(temp + offset, data, size);
        baseFp = fopen(path, "w");
        fprintf(baseFp, "%s", temp);
        fclose(baseFp);

        free(temp);
    }
}

void rvm_truncate_log(rvm_t rvm)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/logfile", rvm.dir);
    FILE *logFp = fopen(path, "r");

    char *line;
    ssize_t result;
    result = getline(&line, NULL, logFp);
    while (result != -1) {
        truncate_log_line(rvm, line);
        free(line);
        line = NULL;
        result = getline(&line, NULL, logFp);
    }
    fclose(logFp);
    logFp = fopen(path, "w");
    fclose(logFp);
}
