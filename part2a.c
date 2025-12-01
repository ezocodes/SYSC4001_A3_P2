#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define NUM_QUESTIONS 5
#define MAX_EXAMS     64
#define MAX_NAME_LEN  256

typedef struct {
    int current_student;
    int question_marked[NUM_QUESTIONS];   // 0 = not done, 1 = done
    char rubric[NUM_QUESTIONS];

    int num_exams;
    int next_exam_index;
    char exam_files[MAX_EXAMS][MAX_NAME_LEN];

    int stop;   // becomes 1 when we hit student 9999
} shared_data_t;


void load_rubric(const char *rubric_file, shared_data_t *shm) {
    FILE *f = fopen(rubric_file, "r");
    if (!f) {
        perror("fopen rubric_file");
        exit(1);
    }

    char line[256];
    int q = 0;

    while (q < NUM_QUESTIONS && fgets(line, sizeof(line), f)) {
        // expecting something like "1, A"
        char *comma = strchr(line, ',');
        if (comma) {
            char *p = comma + 1;
            while (*p == ' ' || *p == '\t') p++;   // skip spaces

            if (*p && *p != '\n')
                shm->rubric[q] = *p;
            else
                shm->rubric[q] = 'A' + q;         // fallback
        } else {
            shm->rubric[q] = 'A' + q;             // fallback
        }
        q++;
    }

    // if the file is short, fill the rest
    while (q < NUM_QUESTIONS) {
        shm->rubric[q] = 'A' + q;
        q++;
    }

    fclose(f);
}

void load_exam_list(const char *exam_list_file, shared_data_t *shm) {
    FILE *f = fopen(exam_list_file, "r");
    if (!f) {
        perror("fopen exam_list_file");
        exit(1);
    }

    char line[MAX_NAME_LEN];
    int count = 0;

    while (count < MAX_EXAMS && fgets(line, sizeof(line), f)) {
        // trim newline
        line[strcspn(line, "\r\n")] = '\0';

        if (line[0] == '\0') continue; // skip blank lines

        strncpy(shm->exam_files[count], line, MAX_NAME_LEN - 1);
        shm->exam_files[count][MAX_NAME_LEN - 1] = '\0';
        count++;
    }

    fclose(f);

    shm->num_exams = count;
    shm->next_exam_index = 0;

    if (count == 0) {
        fprintf(stderr, "No exams found in %s\n", exam_list_file);
        exit(1);
    }
}


void load_exam_from_file(shared_data_t *shm, int exam_index) {
    if (exam_index < 0 || exam_index >= shm->num_exams) {
        shm->stop = 1;
        return;
    }

    const char *filename = shm->exam_files[exam_index];
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("fopen exam file");
        shm->stop = 1;
        return;
    }

    int student;
    if (fscanf(f, "%d", &student) != 1) {
        fprintf(stderr, "Could not read student number from %s\n", filename);
        fclose(f);
        shm->stop = 1;
        return;
    }

    fclose(f);

    shm->current_student = student;

    // reset marking flags for the new exam
    for (int i = 0; i < NUM_QUESTIONS; i++) {
        shm->question_marked[i] = 0;
    }

    printf("Loaded exam %s (student %04d)\n", filename, student);

    if (student == 9999) {
        // 9999 = stop all TA's
        shm->stop = 1;
    }
}


void review_rubric(int ta_id, shared_data_t *shm) {
    for (int q = 0; q < NUM_QUESTIONS; q++) {
        char old_letter = shm->rubric[q];

        printf("TA %d: checking Q%d (now '%c')\n",
               ta_id, q + 1, old_letter);

        // 0.5–1.0 seconds
        int delay_us = 500000 + (rand() % 500001);
        usleep(delay_us);

        int need_correction = rand() % 2; // 0 or 1

        if (need_correction) {
            char new_letter = old_letter + 1;
            shm->rubric[q] = new_letter;

            printf("TA %d: updated rubric Q%d from %c to %c\n",
                   ta_id, q + 1, old_letter, new_letter);

        } else {
            printf("TA %d: no change for Q%d\n", ta_id, q + 1);
        }
    }
}

void mark_exam_questions(int ta_id, shared_data_t *shm) {
    while (1) {
        int all_done = 1;
        int q_to_mark = -1;

        // check if anything is left to do
        for (int i = 0; i < NUM_QUESTIONS; i++) {
            if (shm->question_marked[i] == 0) {
                all_done = 0;
                q_to_mark = i;
                break;
            }
        }

        if (all_done) {
            printf("TA %d: all questions marked for %04d\n",
                   ta_id, shm->current_student);
            break;
        }

        if (q_to_mark == -1) {
            continue;
        }

        printf("TA %d: marking Q%d for %04d\n",
               ta_id, q_to_mark + 1, shm->current_student);

        // 1–2 seconds
        int delay_us = 1000000 + (rand() % 1000001);
        usleep(delay_us);

        shm->question_marked[q_to_mark] = 1;

        printf("TA %d: done Q%d for %04d\n",
               ta_id, q_to_mark + 1, shm->current_student);
    }
}


void ta_process(int ta_id, shared_data_t *shm) {
    // separate seed per TA
    srand(time(NULL) ^ (getpid() << 16) ^ ta_id);

    printf("TA %d: starting\n", ta_id);

    while (!shm->stop) {
        printf("TA %d: working on student %04d\n",
               ta_id, shm->current_student);

        // rubric check/corrections
        review_rubric(ta_id, shm);

        printf("TA %d: rubric now: ", ta_id);
        for (int i = 0; i < NUM_QUESTIONS; i++) {
            printf("%c ", shm->rubric[i]);
        }
        printf("\n");

        // mark questions for this exam
        mark_exam_questions(ta_id, shm);

        printf("TA %d: done with %04d\n",
               ta_id, shm->current_student);

        // load next exam (2a = no sync, so races are allowed)
        if (!shm->stop) {
            int idx = shm->next_exam_index;
            shm->next_exam_index++;

            printf("TA %d: loading exam index %d\n", ta_id, idx);

            load_exam_from_file(shm, idx);

            if (shm->stop) {
                printf("TA %d: stop flag hit, leaving loop\n", ta_id);
            }
        }
    }

    printf("TA %d: stopping (final student %04d)\n",
           ta_id, shm->current_student);
}


int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr,
                "Usage: %s <num_TAs> <rubric_file> <exam_list_file>\n",
                argv[0]);
        return 1;
    }

    int num_TAs = atoi(argv[1]);
    const char *rubric_file = argv[2];
    const char *exam_list_file = argv[3];

    if (num_TAs < 2) {
        fprintf(stderr, "Error: num_TAs must be at least 2.\n");
        return 1;
    }

    printf("Number of TAs: %d\n", num_TAs);
    printf("Rubric file: %s\n", rubric_file);
    printf("Exam list file: %s\n", exam_list_file);

    // shared memory setup
    key_t key = ftok(".", 'R');
    if (key == -1) {
        perror("ftok");
        return 1;
    }

    int shmid = shmget(key, sizeof(shared_data_t), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget");
        return 1;
    }

    shared_data_t *shm = (shared_data_t *)shmat(shmid, NULL, 0);
    if (shm == (void *)-1) {
        perror("shmat");
        return 1;
    }

    memset(shm, 0, sizeof(*shm));

    printf("Shared memory set up.\n");

    // load rubric + exam list + first exam
    load_rubric(rubric_file, shm);
    printf("Rubric loaded: ");
    for (int i = 0; i < NUM_QUESTIONS; i++) {
        printf("%c ", shm->rubric[i]);
    }
    printf("\n");

    load_exam_list(exam_list_file, shm);
    printf("Exam list loaded. Total exams: %d\n", shm->num_exams);

    load_exam_from_file(shm, 0);
    shm->next_exam_index = 1;   // next exam index

    printf("Current student in shared memory: %04d\n", shm->current_student);

    // create TA processes
    printf("Creating %d TA processes...\n", num_TAs);

    for (int i = 0; i < num_TAs; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
        } else if (pid == 0) {
            // child = TA
            ta_process(i, shm);

            if (shmdt(shm) == -1) {
                perror("shmdt in child");
            }
            exit(0);
        }
        // parent just continues the loop to fork next TA
    }

    // parent waits for all TAs
    for (int i = 0; i < num_TAs; i++) {
        int status;
        wait(&status);
    }

    printf("All TA processes have finished.\n");

    // cleanup shared memory
    if (shmdt(shm) == -1) perror("shmdt");
    if (shmctl(shmid, IPC_RMID, NULL) == -1) perror("shmctl");

    printf("Program finished.\n");
    return 0;
}
