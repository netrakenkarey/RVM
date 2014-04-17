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

    rvm.dir = strdup(directory);
	rvm.rvmid = ++rvmid_count;
	
	int filestatus = stat(directory, &filestat);
		
	if (filestatus != 0) {
		system(temp);
	}
	
	return rvm;
}

static void recover_data(const char *segname, void *segbase, FILE *baseFp, FILE *logFp)
{
    char *line = NULL;
    size_t len = 0;
    ssize_t result;

    int size = 0;
    fscanf(baseFp, "%d\n", &size);
    fread(segbase, 1, size, baseFp);
    result = getline(&line, &len, logFp);
    while (result != -1) {
        strtok(line, "|||");
        int offset = atoi(strtok(NULL, "|||"));
        int log_size = atoi(strtok(NULL, "|||"));
        fread((char *) segbase + offset, 1, log_size, logFp);

        free(line);
        line = NULL;
        result = getline(&line, &len, logFp);
    }
}

void* rvm_map(rvm_t rvm, const char *segname, int size_to_create)
{
	struct stat filestat;
	char temp[200];

	strcpy(temp, rvm.dir);
	strcat(temp,"/");
	strcat(temp,segname);
	int filestatus = stat(temp, &filestat);
    int recover = 0;

    // file already exists
    if (filestatus == 0) {
        int cur_size = 0;
        FILE *baseFp = fopen(temp, "r");
        fscanf(baseFp, "%d\n", &cur_size);
        if (cur_size != size_to_create) {
            void *temp_data = malloc(size_to_create);
            fread(temp_data, 1, cur_size, baseFp);
            baseFp = freopen(temp, "w", baseFp);
            fprintf(baseFp, "%d\n", size_to_create);
            fwrite(temp_data, 1, size_to_create, baseFp);
            free(temp_data);
        }
        fclose(baseFp);
        recover = 1;
    } else {
        FILE *baseFp = fopen(temp, "w");
        fprintf(baseFp, "%d\n", size_to_create);
        fclose(baseFp);
    }

    struct segment *segment = NULL;
    for (unsigned int i = 0; i < segments.size(); ++i) {
        if (strcmp(segments[i].segname, segname) == 0) {
            // segment already exists
            if (segments[i].segsize != size_to_create) {
                segments[i].segment = realloc(segments[i].segment, sizeof(char) * size_to_create);
                segments[i].segsize = size_to_create;
            }
            segment = &(segments[i]);
        }
    }

    struct segment seg;
    if (!segment) {
        seg.segment = malloc(sizeof(char) * size_to_create);
        seg.segname = strdup(segname);
        seg.rvm = rvm;
        seg.segsize = size_to_create;
        seg.tid = -1;
        segments.push_back(seg);
        segment = &seg;
    }

    if (recover) {
        memset(temp, 0, 200);
        strcpy(temp, rvm.dir);
        strcat(temp,"/logfile");
        FILE *baseFp = fopen(temp, "r");
        FILE *logFp = fopen(temp, "r");
        recover_data(segment->segname, segment->segment, baseFp, logFp);
        fclose(baseFp);
        fclose(logFp);
    }
    return segment->segment;
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
			printf("Mapped segment cannot be destroyed\n");
            return;
        }
    }

	strcpy(temp, rvm.dir);
	strcat(temp,"/");
	strcat(temp,segname);
	
	int res = remove(temp);
	if(res == 0) {
		printf("Segment destroyed\n");
	} else {
		printf("Segment backstore does not exist\n");
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

static void save_log(void *segbase, int offset, int size)
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

        void *data = malloc(sizeof(char) * (size+1));
        memcpy(data, (char *) segbase + offset, size);

        fprintf(f, "%s|||%d|||%d\n", segments[i].segname, offset, size);
        fwrite(data, 1, size, f);
        free(data);
        fclose(f);
    }
}

void rvm_commit_trans(trans_t tid)
{
    vector<undo_record>::iterator it = undo_records.begin();
    while (it != undo_records.end()) {
        if (it->tid == tid) {
            save_log(it->segbase, it->offset, it->size);
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
            memcpy((char *)it->segbase + it->offset, it->data, it->size);
            free(it->data);
            it = undo_records.erase(it);
        } else {
            it++;
        }
    }
    remove_trans_from_list(tid);
}

static void truncate_log_line(rvm_t rvm, char *line)
{
    void *temp;
    char *segname = strtok(line, "|||");

    char path[256];
    snprintf(path, sizeof(path), "%s/%s", rvm.dir, segname);
	struct stat filestat;
	int filestatus = stat(path, &filestat);
    if (filestatus == 0) {
        // read base file
        int size = 0;
        FILE *baseFp = fopen(path, "r");
        fscanf(baseFp, "%d\n", &size);
        temp = malloc(size);
        fread(temp, 1, size, baseFp);
        fclose(baseFp);

        int offset = atoi(strtok(NULL, "|||"));
        int log_size = atoi(strtok(NULL, "|||"));
        char *data = strtok(NULL, "|||");

        memcpy((char *) temp + offset, data, log_size);
        baseFp = fopen(path, "w");
        fprintf(baseFp, "%d\n", size);
        fwrite(temp, 1, size, baseFp);
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
    size_t len;
    ssize_t result;
    result = getline(&line, &len, logFp);
    while (result != -1) {
        truncate_log_line(rvm, line);
        free(line);
        line = NULL;
        result = getline(&line, &len, logFp);
    }
    fclose(logFp);
    logFp = fopen(path, "w");
    fclose(logFp);
}
