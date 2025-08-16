#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#define MAX_LOG_ENTRIES 16                    // maximum number of log entries for a file
#define MAX_BUFFSIZE 102400                   // maximum size of the file

typedef struct{
    char* content;                            // char array to store the file content
    char* comment;                            // char array to store the comment 
    char* author_name;                        // char array to hold the author name
    time_t timestamp;                         // timestamp for log entry
    int version_id;                           // unique version identifier
}LogEntry;

// FILE DESCRIPTION
typedef struct{
    char* name;                                // char array to hold the file name
    LogEntry log_history[MAX_LOG_ENTRIES];     // stores the log history of the file
    int log_count;                             // keeps the count of changes made to the file
    int is_deleted;                            // flag to indicate if the file is deleted
    int front;                                 // points to the start of a file's log_history queue
    int rear;                                  // points to the last of a file's log_history queue
}File;                                         

typedef struct{
    File* files;                               // list of files
    int file_count;                            // number of files present at the instance
    char* tag;                                 // tag for identification
    time_t timestamp;                          // timestamp of the snapshot
    int is_obsolete;                           // flag to indicate if the snapshot is deleted
}Snapshot;

static File *files = NULL;                      // maintains the files in files array
static int file_count = 0;                      // keeps track of the number of files in the files array
static Snapshot *snapshots = NULL;              // stores all the snapshots created in snapshots array
static int snapshot_count = 0;                  // keeps track of the number of snapshots created so far
static int is_change = 0;                       // tracks if there has been any change in the file system


// save the file system state to disk
void save_to_disk(const char *filename){
    FILE *fp = fopen(filename, "wb");
    if(!fp){
        perror("Error opening file for writing");
        return;
    }

    fwrite(&file_count, sizeof(int), 1, fp);
    fwrite(&snapshot_count, sizeof(int), 1, fp);

    for(int i = 0; i < file_count; i++){
        int name_len = strlen(files[i].name);
        fwrite(&name_len, sizeof(int), 1, fp);
        fwrite(files[i].name, sizeof(char), name_len, fp);

        fwrite(&files[i].log_count, sizeof(int), 1, fp);
        for(int j = 0; j < files[i].log_count; j++){
            LogEntry *log = &files[i].log_history[j];
            int content_len = strlen(log->content);
            int comment_len = strlen(log->comment);
            int author_len = strlen(log->author_name);

            fwrite(&content_len, sizeof(int), 1, fp);
            fwrite(&comment_len, sizeof(int), 1, fp);
            fwrite(&author_len, sizeof(int), 1, fp);
            fwrite(log->content, sizeof(char), content_len, fp);
            fwrite(log->comment, sizeof(char), comment_len, fp);
            fwrite(log->author_name, sizeof(char), author_len, fp);
            fwrite(&log->timestamp, sizeof(time_t), 1, fp);
            fwrite(&log->version_id, sizeof(int), 1, fp);
        }

        fwrite(&files[i].is_deleted, sizeof(int), 1, fp);
        fwrite(&files[i].front, sizeof(int), 1, fp);
        fwrite(&files[i].rear, sizeof(int), 1, fp);
    }

    for(int i = 0; i < snapshot_count; i++){
        int tag_len = strlen(snapshots[i].tag);
        fwrite(&tag_len, sizeof(int), 1, fp);
        fwrite(snapshots[i].tag, sizeof(char), tag_len, fp);
        fwrite(&snapshots[i].timestamp, sizeof(time_t), 1, fp);
        fwrite(&snapshots[i].is_obsolete, sizeof(int), 1, fp);
        fwrite(&snapshots[i].file_count, sizeof(int), 1, fp);

        for(int j = 0; j < snapshots[i].file_count; j++){
            File *file = &snapshots[i].files[j];

            int name_len = strlen(file->name);
            fwrite(&name_len, sizeof(int), 1, fp);
            fwrite(file->name, sizeof(char), name_len, fp);

            fwrite(&file->log_count, sizeof(int), 1, fp);
            for(int k = 0; k < file->log_count; k++){
                LogEntry *log = &file->log_history[k];
                int content_len = strlen(log->content);
                int comment_len = strlen(log->comment);
                int author_len = strlen(log->author_name);

                fwrite(&content_len, sizeof(int), 1, fp);
                fwrite(&comment_len, sizeof(int), 1, fp);
                fwrite(&author_len, sizeof(int), 1, fp);
                fwrite(log->content, sizeof(char), content_len, fp);
                fwrite(log->comment, sizeof(char), comment_len, fp);
                fwrite(log->author_name, sizeof(char), author_len, fp);
                fwrite(&log->timestamp, sizeof(time_t), 1, fp);
                fwrite(&log->version_id, sizeof(int), 1, fp);
            }

            fwrite(&file->is_deleted, sizeof(int), 1, fp);
            fwrite(&file->front, sizeof(int), 1, fp);
            fwrite(&file->rear, sizeof(int), 1, fp);
        }
    }

    fclose(fp);
    printf("Data successfully saved to %s\n", filename);
}

// load the file system state from disk
void load_from_disk(const char *filename){
    FILE *fp = fopen(filename, "rb");
    if(!fp){
        perror("Error opening file for reading");
        return;
    }

    fread(&file_count, sizeof(int), 1, fp);
    fread(&snapshot_count, sizeof(int), 1, fp);

    files = (File *)malloc(file_count * sizeof(File));

    for(int i = 0; i < file_count; i++){
        int name_len;
        fread(&name_len, sizeof(int), 1, fp);
        files[i].name = (char *)malloc(name_len + 1);
        fread(files[i].name, sizeof(char), name_len, fp);
        files[i].name[name_len] = '\0';

        fread(&files[i].log_count, sizeof(int), 1, fp);
        for (int j = 0; j < files[i].log_count; j++) {
            LogEntry *log = &files[i].log_history[j];
            int content_len, comment_len, author_len;

            fread(&content_len, sizeof(int), 1, fp);
            fread(&comment_len, sizeof(int), 1, fp);
            fread(&author_len, sizeof(int), 1, fp);

            log->content = (char *)malloc(content_len + 1);
            log->comment = (char *)malloc(comment_len + 1);
            log->author_name = (char *)malloc(author_len + 1);

            fread(log->content, sizeof(char), content_len, fp);
            fread(log->comment, sizeof(char), comment_len, fp);
            fread(log->author_name, sizeof(char), author_len, fp);

            log->content[content_len] = '\0';
            log->comment[comment_len] = '\0';
            log->author_name[author_len] = '\0';

            fread(&log->timestamp, sizeof(time_t), 1, fp);
            fread(&log->version_id, sizeof(int), 1, fp);
        }

        fread(&files[i].is_deleted, sizeof(int), 1, fp);
        fread(&files[i].front, sizeof(int), 1, fp);
        fread(&files[i].rear, sizeof(int), 1, fp);
    }

    snapshots = (Snapshot *)malloc(snapshot_count * sizeof(Snapshot));

    for(int i = 0; i < snapshot_count; i++){
        int tag_len;
        fread(&tag_len, sizeof(int), 1, fp);
        snapshots[i].tag = (char *)malloc(tag_len + 1);
        fread(snapshots[i].tag, sizeof(char), tag_len, fp);
        snapshots[i].tag[tag_len] = '\0';

        fread(&snapshots[i].timestamp, sizeof(time_t), 1, fp);
        fread(&snapshots[i].is_obsolete, sizeof(int), 1, fp);
        fread(&snapshots[i].file_count, sizeof(int), 1, fp);

        snapshots[i].files = (File *)malloc(snapshots[i].file_count * sizeof(File));
        for(int j = 0; j < snapshots[i].file_count; j++){
            File *file = &snapshots[i].files[j];
            int name_len;
            fread(&name_len, sizeof(int), 1, fp);
            file->name = (char *)malloc(name_len + 1);
            fread(file->name, sizeof(char), name_len, fp);
            file->name[name_len] = '\0';

            fread(&file->log_count, sizeof(int), 1, fp);
            for (int k = 0; k < file->log_count; k++) {
                LogEntry *log = &file->log_history[k];
                int content_len, comment_len, author_len;

                fread(&content_len, sizeof(int), 1, fp);
                fread(&comment_len, sizeof(int), 1, fp);
                fread(&author_len, sizeof(int), 1, fp);

                log->content = (char *)malloc(content_len + 1);
                log->comment = (char *)malloc(comment_len + 1);
                log->author_name = (char *)malloc(author_len + 1);

                fread(log->content, sizeof(char), content_len, fp);
                fread(log->comment, sizeof(char), comment_len, fp);
                fread(log->author_name, sizeof(char), author_len, fp);

                log->content[content_len] = '\0';
                log->comment[comment_len] = '\0';
                log->author_name[author_len] = '\0';

                fread(&log->timestamp, sizeof(time_t), 1, fp);
                fread(&log->version_id, sizeof(int), 1, fp);
            }

            fread(&file->is_deleted, sizeof(int), 1, fp);
            fread(&file->front, sizeof(int), 1, fp);
            fread(&file->rear, sizeof(int), 1, fp);
        }
    }

    fclose(fp);
    printf("Data successfully loaded from %s\n", filename);
}

// deleting the log for the file in FIFO manner
void delete_log(const char *name){
    for(int i=0; i<file_count; i++){
        if(strcmp(files[i].name, name) == 0){
            free(files[i].log_history[files[i].front].content);
            free(files[i].log_history[files[i].front].author_name);
            free(files[i].log_history[files[i].front].comment);
            files[i].front = (files[i].front + 1)%MAX_LOG_ENTRIES;
            files[i].log_count--;
            printf("Deleted log of file %s using FIFO policy.\n", name); 
            return;
        }
    }
    printf("File %s not found.\n", name);  
}

// adding a new file in the file system
void add_file(const char *name, const char *file_name, const char *author_name, const char *note){
    int fd = open(file_name, O_RDONLY);
    if(fd == -1){
        fprintf(stderr, "Error: File could not be opened.\n");
        return;
    }
    char* content = malloc(MAX_BUFFSIZE);
    if(content == NULL){
        fprintf(stderr, "Error: Memory allocation for file content failed.\n");
        close(fd);
        return;
    }
    ssize_t bytes_read = read(fd, content, MAX_BUFFSIZE);
    if(bytes_read == -1){
        fprintf(stderr, "Error: File couldn't be read.\n");
        free(content);
        close(fd);
        return;
    }
    content[bytes_read] = '\0';
    LogEntry new_log;
    new_log.content = strdup(content);
    new_log.comment = strdup(note);
    new_log.author_name = strdup(author_name);
    new_log.timestamp = time(NULL);
    close(fd);
    free(content);
    for(int i=0; i<file_count; i++){
        if(strcmp(files[i].name, name) == 0){
            if(files[i].is_deleted){
                fprintf(stderr, "Error: Failed to update. File %s already exists and currently unavailable.\n", name);
                return;
            }
            if(files[i].log_count >= MAX_LOG_ENTRIES){
                delete_log(name);
            }
            new_log.version_id = files[i].log_count + 1;
            files[i].rear = (files[i].rear + 1)%MAX_LOG_ENTRIES;
            files[i].log_count++;
            files[i].log_history[files[i].rear] = new_log;
            is_change = 1;
            printf("File %s updated successfully.\n", name);
            return;
        }
    }
    files = realloc(files, (file_count + 1)*sizeof(File));
    if(!files){
        perror("Error: Memory allocation failed.\n");  
        exit(EXIT_FAILURE);
    }
    files[file_count].name = strdup(name);
    files[file_count].log_history[0] = new_log;
    files[file_count].log_history[0].version_id = 1;
    files[file_count].log_count = 1;
    files[file_count].is_deleted = 0;
    files[file_count].front = 0;
    files[file_count].rear = 0;
    file_count++;
    is_change = 1;
    printf("File %s added successfully.\n", name);
}

// prints the current state of the file system
void view_fileSystem(){
    for(int i=0; i<file_count; i++){
        if(!files[i].is_deleted){
            printf("File name:     %s\n", files[i].name);
        }
    }
}

// read the file content
void view_fileContent(const char* filename){
    for(int i=0; i<file_count; i++){
        if(strcmp(files[i].name, filename) == 0){
            if(files[i].is_deleted){
                fprintf(stderr, "Error: File '%s' is currently unavailable. Use 'recover' to restore.\n", filename);
                return;
            }
            printf("%s\n\n", files[i].log_history[files[i].rear].content);
            return;
        }
    }
    printf("File not found.\n");
    return;
}

// taking snapshot at the current time instance
void create_snapshot(const char *tag){ 
    if(!is_change){
        printf("Already the latest file system version.\n");
        return;
    }
    for(int i=0; i<snapshot_count; i++){
        if(strcmp(snapshots[i].tag, tag) == 0){
            fprintf(stderr, "Error: Snapshot with tag '%s' already exists\n", tag);   
            return;
        }
    }
    snapshots = realloc(snapshots, (snapshot_count + 1)*sizeof(Snapshot));
    if (!snapshots) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        
        exit(EXIT_FAILURE);
    }
    Snapshot *snapshot = &snapshots[snapshot_count++];
    snapshot->files = malloc(file_count*sizeof(File));
    snapshot->file_count = file_count;
    snapshot->timestamp = time(NULL);
    snapshot->tag = strdup(tag);
    snapshot->is_obsolete = 0;

    for(int i=0; i<file_count; i++){
        if(files[i].is_deleted) continue;
        snapshot->files[i].name = strdup(files[i].name);
        snapshot->files[i].log_count = files[i].log_count;
        snapshot->files[i].is_deleted = files[i].is_deleted;
        snapshot->files[i].rear = files[i].rear;
        snapshot->files[i].front = files[i].front;

        for(int j=0; j<files[i].log_count; j++){
            snapshot->files[i].log_history[j].comment = strdup(files[i].log_history[j].comment);
            snapshot->files[i].log_history[j].author_name = strdup(files[i].log_history[j].author_name);
            snapshot->files[i].log_history[j].timestamp = files[i].log_history[j].timestamp;
            snapshot->files[i].log_history[j].version_id = files[i].log_history[j].version_id;
            snapshot->files[i].log_history[j].content = strdup(files[i].log_history[j].content);
        }
    }
    is_change = 0;
    printf("Snapshot '%s' created successfully.\n", tag);
}

//  reverts the file system to a specified snapshot based on its index
void rollback(int snapshot_index){
    if(snapshot_index < 0 || snapshot_index >= snapshot_count){
        fprintf(stderr, "Error: Invalid snapshot index\n");
        return;
    }

    if(snapshots[snapshot_index].is_obsolete){
        fprintf(stderr, "Error: Referred snapshot index %d is obsolete\n", snapshot_index);
        return;
    }

    for(int i = 0; i < file_count; i++){
        free(files[i].name);
        for(int j = 0; j < files[i].log_count; j++){
            free(files[i].log_history[j].content);
            free(files[i].log_history[j].author_name);
            free(files[i].log_history[j].comment);
        }
    }
    free(files);
    Snapshot *snapshot = &snapshots[snapshot_index];
    file_count = snapshot->file_count;
    files = malloc(file_count * sizeof(File));
    if(!files){
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < file_count; i++){
        File *src = &snapshot->files[i];
        File *dst = &files[i];
        dst->name = strdup(src->name);
        dst->is_deleted = src->is_deleted;
        dst->log_count = src->log_count;
        dst->front = src->front;
        dst->rear = src->rear;
        for(int j = 0; j < src->log_count; j++){
            dst->log_history[j].author_name = strdup(src->log_history[j].author_name);
            dst->log_history[j].comment = strdup(src->log_history[j].comment);
            dst->log_history[j].content = strdup(src->log_history[j].content);
            dst->log_history[j].timestamp = src->log_history[j].timestamp;
            dst->log_history[j].version_id = src->log_history[j].version_id;
        }
    }

    printf("Rolled back to snapshot index %d successfully.\n", snapshot_index);
}

// recover back the obsolete snapshot
void recover_snapshot(int snapshot_index){
    if(snapshot_index < 0 || snapshot_index >= snapshot_count){
        fprintf(stderr, "Error: Invalid snapshot index\n");
        return;
    }

    if(!snapshots[snapshot_index].is_obsolete){
        fprintf(stderr, "Snapshot %d is already Active\n", snapshot_index);
        return;
    }
    
    snapshots[snapshot_index].is_obsolete = 0;
    printf("Snapshot %d is successfully recovered\n", snapshot_index);
    return;
}

// delete file from the file system
void delete_file(const char *name){
    for(int i=0; i<file_count; i++){
        if(strcmp(files[i].name, name) == 0 && !files[i].is_deleted){
            files[i].is_deleted = 1;
            is_change = 1;
            printf("File %s deleted successfully.\n", name);
            
            return;
        }
    }
    printf("File %s not found.\n", name);  
}

// recover a deleted file by its name
void recover_file(const char *name){
    for(int i=0; i<file_count; i++){
        if(strcmp(files[i].name, name) == 0 && files[i].is_deleted){
            files[i].is_deleted = 0;
            is_change = 1;
            printf("File %s recovered successfully.\n", name);
            return;
        }
    }
    
    printf("File %s not found or not deleted.\n", name);
}

// revert file to a specific version
void revert_file(const char *name, int version){
    for(int i=0; i<file_count; i++){
        if(strcmp(files[i].name, name) == 0){
            if(files[i].is_deleted){
                fprintf(stderr, "Error: Cannot revert. The file '%s' is currently deleted. Please recover it first.\n", name);
            return;
            }
            if(version < 1 || version > files[i].log_count){
                fprintf(stderr, "Version is out of bounds.\n");
                return;
            }
            int idx = (files[i].front + version - 1) % MAX_LOG_ENTRIES;
            LogEntry temp = files[i].log_history[files[i].rear];
            files[i].log_history[files[i].rear] = files[i].log_history[idx];
            files[i].log_history[idx] = temp;

            printf("File content successfully reverted to version %d.\n", version);
            return;
        }
    }
    printf("No history found for the file: %s\n", name);   
}

// delete snapshot based on the tag
void delete_snapshot(const char* tag) {
    int found = 0;
    for(int i = 0; i < snapshot_count; i++){
        if(strcmp(snapshots[i].tag, tag) == 0){
            found = 1;
            for(int j = 0; j < snapshots[i].file_count; j++){
                File *file = &snapshots[i].files[j];
                free(file->name);
                for(int k = 0; k < file->log_count; k++){
                    free(file->log_history[k].content);
                    free(file->log_history[k].comment);
                    free(file->log_history[k].author_name);
                }
            }
            free(snapshots[i].files);
            for(int j = i; j < snapshot_count - 1; j++){
                snapshots[j] = snapshots[j + 1];
            }
            snapshot_count--;
            printf("Snapshot with tag '%s' deleted successfully.\n", tag);
            break;
        }
    }
    if(!found){
        printf("Snapshot not found for the tag '%s'.\n", tag);
    }
}

// obsoleting snapshot based on the tag for audit purposes
void obsolete_snapshot(const char* tag){
    int found = 0;
    for(int i=0; i<snapshot_count; i++){
        if(strcmp(snapshots[i].tag, tag) == 0){
            found = 1;
            snapshots[i].is_obsolete = 1;
            printf("Snapshot with tag '%s' deleted successfully.\n", tag);
            break;
        }
    }
    if(!found){
        printf("Snapshot not found for the tag '%s'.\n", tag);
    }
}

// lists all the snapshots that have been created
void list_snapshots(){
    printf("Available Snapshots:\n");
    for (int i = 0; i < snapshot_count; i++){
        char *time_str = ctime(&snapshots[i].timestamp);
        time_str[strlen(time_str) - 1] = '\0';        
        printf("Snapshot %d: %s, Timestamp: %s, Status: %s\n", i, snapshots[i].tag, time_str, snapshots[i].is_obsolete ? "Deleted" : "Active");
    }  
}

// log history for a file
void log_history(const char *name){
    for(int i=0; i<file_count; i++){
        if(strcmp(files[i].name, name) == 0){
            printf("Log History for %s:\n", name);
            if(files[i].is_deleted){
                printf("Note: This file is currently deleted.\n");
            }
            int idx = files[i].front;
            int count = files[i].log_count;
            while(count > 0){
                printf("Author: %s\n", files[i].log_history[idx].author_name);
                printf("Note: %s\n", files[i].log_history[idx].comment);
                printf("Timestamp: %s\n", ctime(&files[i].log_history[idx].timestamp));
                printf("Version: %d\n", files[i].log_history[idx].version_id);
                printf("-----------------------------\n");
                idx = (idx + 1) % MAX_LOG_ENTRIES;
                count--;
            }
            return;
        }
    }
    printf("No history found for the file: %s\n", name);   
}

// frees up the memory
void cleanup(){
    for(int i = 0; i < file_count; i++){
        for(int j = 0; j < files[i].log_count; j++){
            free(files[i].log_history[j].content);
            free(files[i].log_history[j].author_name);
            free(files[i].log_history[j].comment);
        }
        free(files[i].name);
    }
    free(files);
    for(int i = 0; i < snapshot_count; i++){
        for(int j = 0; j < snapshots[i].file_count; j++){
            for(int k = 0; k < snapshots[i].files[j].log_count; k++){
                free(snapshots[i].files[j].log_history[k].content);
                free(snapshots[i].files[j].log_history[k].author_name);
                free(snapshots[i].files[j].log_history[k].comment);
            }
            free(snapshots[i].files[j].name);
        }
        free(snapshots[i].files);
        free(snapshots[i].tag);
    }
    free(snapshots);
}

// help for interactive command-line execution
void help(){
    printf("***** Available commands *****\n");
    printf("add <file_name> <file_path> <author> <note>     ---> Add a new file\n");
    printf("viewfs                                          ---> View all files\n");
    printf("view <file_name>                                ---> View latest file content\n");
    printf("delete <file_name>                              ---> Delete a file\n");
    printf("recover <file_name> <user>                      ---> Recover a deleted file\n");
    printf("log <file_name>                                 ---> View log history\n");
    printf("snapshot <tag>                                  ---> Create snapshot\n");
    printf("rollback <index>                                ---> Rollback to snapshot\n");
    printf("deletesnap <tag>                                ---> Delete snapshot\n");
    printf("obsoletesnap <tag>                              ---> Obsolete snapshot\n");
    printf("recoversnap <index>                             ---> Recover snapshot\n");
    printf("listsnap                                        ---> List snapshots\n");
    printf("revert <file_name> <version>                    ---> Revert file to version\n");
    printf("save <filename>                                 ---> Save state to disk\n");
    printf("load <filename>                                 ---> Load state from disk\n");
    printf("help                                            ---> Show this help\n");
    printf("exit                                            ---> Exit the program\n");
}

int main(){
    char cmd[512];
    printf("------------------------------------ Version Based File System ------------------------------------ \n");
    printf("\nWelcome!...\n");
    printf("Starting the CLI...\n\n");
    help();
    while(1){
        printf("\n> ");
        if(!fgets(cmd, sizeof(cmd), stdin)) break;
        char *args[6];
        int argc = 0;
        char *token = strtok(cmd, " \n");
        while (token && argc < 6) {
            args[argc++] = token;
            token = strtok(NULL, " \n");
        }
        if(argc == 0) continue;
        if(strcmp(args[0], "add") == 0 && argc == 5){
            add_file(args[1], args[2], args[3], args[4]);
        }
        else if(strcmp(args[0], "viewfs") == 0){
            view_fileSystem();
        }
        else if(strcmp(args[0], "view") == 0 && argc == 2){
            view_fileContent(args[1]);
        }
        else if(strcmp(args[0], "delete") == 0 && argc == 2){
            delete_file(args[1]);
        }
        else if(strcmp(args[0], "recover") == 0 && argc == 2){
            recover_file(args[1]);
        }
        else if(strcmp(args[0], "log") == 0 && argc == 2){
            log_history(args[1]);
        }
        else if(strcmp(args[0], "snapshot") == 0 && argc == 2){
            create_snapshot(args[1]);
        }
        else if(strcmp(args[0], "rollback") == 0 && argc == 2){
            rollback(atoi(args[1]));
        }
        else if(strcmp(args[0], "deletesnap") == 0 && argc == 2){
            delete_snapshot(args[1]);
        }
        else if(strcmp(args[0], "obsoletesnap") == 0 && argc == 2){
            obsolete_snapshot(args[1]);
        }
        else if(strcmp(args[0], "recoversnap") == 0 && argc == 2){
            recover_snapshot(atoi(args[1]));
        }
        else if(strcmp(args[0], "listsnap") == 0){
            list_snapshots();
        }
        else if(strcmp(args[0], "revert") == 0 && argc == 3){
            revert_file(args[1], atoi(args[2]));
        }
        else if(strcmp(args[0], "save") == 0 && argc == 2){
            save_to_disk(args[1]);
        }
        else if(strcmp(args[0], "load") == 0 && argc == 2){
            load_from_disk(args[1]);
        }
        else if(strcmp(args[0], "help") == 0){
            help();
        }
        else if(strcmp(args[0], "exit") == 0){
            cleanup();
            printf("Cleaning up...\n");
            printf("Exiting...\n");
            break;
        }
        else{
            printf("Invalid command or incorrect usage. Type 'help' to see available commands.\n");
        }
    }
    return 0;
}

