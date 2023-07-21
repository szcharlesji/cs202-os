#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <semaphore.h>
#include <string.h>
#include <math.h>

pthread_mutex_t mutex;

const long max_size = 1024 * 1024 * 1024;        // bytes in 1 GB
const int max_tasks = 1024 * 1024 * 1024 / 4096; // 1 GB / 4 KB blocks

char *input;
// unsigned char input[max_size];

struct Task
{
    int start;
    int end;
    int task;
} Task;

struct Task *task_pickup;
sem_t *task_sem;

long task_pickup_idx = -1;
pthread_mutex_t task_lock;

char *output;
sem_t *output_sem;

int n_jobs;
int total_size = 0;
int total_tasks = 0;

void *map_output()
{

    int task_i = 0;
    while (task_i != -1)
    {
        pthread_mutex_lock(&task_lock);
        task_pickup_idx++;
        pthread_mutex_unlock(&task_lock);

        if (task_pickup_idx < total_tasks)
        {
            printf("\ntask_pickup_idx: %ld\n", task_pickup_idx);
            task_i = task_pickup_idx;
            sem_wait(&task_sem[task_i]);
        }
        else
        {
            printf("Final task");
            task_i = -1;
            continue;
        }

        int start = task_pickup[task_i].start;
        int end = task_pickup[task_i].end;
        int task = task_pickup[task_i].task;

        int current_char = -1;
        int next_char;
        int count = 1;
        // for (int i = start; i < end; i++)
        // {
        //     next_char = input[i];
        //     if (next_char == current_char && count < 255)
        //     {
        //         count++;
        //     }
        //     else
        //     {
        //         if (current_char != -1)
        //         {
        //             output[task] = current_char;
        //             output[task + 1] = count;
        //             task += 2;
        //         }
        //         current_char = next_char;
        //         count = 1;
        //     }
        // }

        int output_index = start * 2;
        for (int i = start; i < end; i++) {
            unsigned char count = 1;
            while (i < end - 1 && input[i] == input[i + 1]) {
                count++;
                i++;
            }
            // Place the result
            output[output_index] = input[i];
            output[output_index + 1] = count;

            output_index += 2; // 1 char, 1 count
        }

        // output[task] = current_char;
        // output[task + 1] = count;

        // Print the output in this task

        sem_post(&output_sem[task_pickup[task_i].task]);
    }
}

void rle_encode(int argc, char *argv[], FILE *output)
{
    int current_char = -1;
    int next_char;
    int count = 1;
    for (int i = 0; i < argc; i++)
    {
        // Open the file
        FILE *input = fopen(argv[i], "rb");
        // Check if the file was opened successfully
        if (input == NULL)
        {
            perror("Error opening input file");
            exit(1);
        }

        // Read the file and encode it
        while ((next_char = fgetc(input)) != EOF)
        {
            if (next_char == current_char && count < 255)
            {
                count++;
            }
            else
            {
                if (current_char != -1)
                {
                    // From man page of fputc:
                    fputc(current_char, output);
                    fputc(count, output);
                }
                current_char = next_char;
                count = 1;
            }
        }
        fclose(input);
    }
    // From man page of fputc:
    fputc(current_char, output);
    fputc(count, output);
}

int main(int argc, char *argv[])
{
    // Check if the user entered anything
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s [-j jobs] <input_files...>\n", argv[0]);
        return 1;
    }

    else if (strncmp(argv[1], "-j", 2) != 0)
    {
        rle_encode(argc - 1, &argv[1], stdout);
    }

    else if (strncmp(argv[1], "-j", 2) == 0)
    {
        if (argc < 4)
        {
            fprintf(stderr, "Usage: %s [-j jobs] <input_files...>\n", argv[0]);
            return 1;
        }

        else
        {
            int jobs = atoi(argv[2]);

            if (jobs < 1)
            {
                fprintf(stderr, "Usage: %s [-j jobs] <input_files...>\n", argv[0]);
                return 1;
            }

            else
            {
                // Allocate memory
                input = malloc(max_size * sizeof(char));

                task_pickup = malloc(max_tasks * sizeof(struct Task));
                task_sem = malloc(max_tasks * sizeof(sem_t));
                output_sem = malloc(max_tasks * sizeof(sem_t));

                // Create threads
                pthread_t threads[jobs];
                int pick_up_point = 0;
                int file_index = 0;

                // Create mutex
                pthread_mutex_init(&mutex, NULL);

                // get number of files
                int num_files = argc - 3;

                // Open files
                // FILE files[num_files];
                int *file_sizes = malloc(sizeof(int) * num_files);
                char *input_ptr = input;

                for (int i = 0; i < num_files; i++)
                {
                    FILE *file = fopen(argv[i + 3], "rb");

                    // Print file name
                    // printf("File %d: %s with ", i + 1, argv[i + 3]);

                    if (file == NULL)
                    {
                        printf("Error opening input file: %s\n", argv[i + 3]);
                        exit(1);
                    }

                    fseek(file, 0, SEEK_END);
                    file_sizes[i] = ftell(file);
                    fseek(file, 0, SEEK_SET);

                    if (fread(input_ptr, (size_t)file_sizes[i], 1, file) != 1)
                    {
                        printf("Error reading input file: %s \n", argv[i + 3]);
                        exit(1);
                    }
                    // printf("size: %d \n", file_sizes[i]);

                    input_ptr += file_sizes[i];

                    fclose(file);
                }

                // get total size
                for (int i = 0; i < num_files; i++)
                {
                    total_size += file_sizes[i];
                }
                // printf("Total size: %d \n", total_size);

                // print input content
                // for (int i = 0; i < total_size; i++)
                // {
                //     printf("%c", input[i]);
                // }

                // Start rle with threads
                total_tasks = ceil((float)total_size / 4096);
                // File can have different neighboring characters
                output = malloc(sizeof(char) * total_tasks * 2);

               

                // Create semaphores
                pthread_mutex_init(&task_lock, NULL);

                for (int i = 0; i < total_tasks; i++)
                {
                    sem_init(&task_sem[i], 0, 0);
                    sem_init(&output_sem[i], 0, 0);
                }

                // Initialize tasks
                // printf("Initializing %d tasks \n", total_tasks);
                for (int i = 0; i < total_tasks; i++)
                {
                    task_pickup[i].start = pick_up_point;

                    // check minimum size
                    if (total_size - pick_up_point < 4096)
                    {
                        task_pickup[i].end = total_size;
                    }
                    else
                    {
                        task_pickup[i].end = pick_up_point + 4096;
                    }
                    
                    task_pickup[i].task = i;
                    pick_up_point += 4096;
                    sem_post(&output_sem[i]);
                    printf("Task %d: %d - %d \n", i, task_pickup[i].start, task_pickup[i].end);
                }


                // Create thread pool
                // printf("Creating %d threads \n", jobs);
                for (int i = 0; i < jobs; i++)
                {
                    pthread_create(&threads[i], NULL, map_output, NULL);
                }


                printf("\n\nIn main, input = ");
                for (int i = 0; i < total_size; i++)
                {
                    printf("%c", input[i]);
                }
                // printf("Address of input: %p \n", input);

                printf("\n\nIn main, output = ");
                for (int i = 0; i < total_size * 2; i = i + 2)
                {
                    printf("%c%d", output[i], output[i + 1]);
                }


                printf("\nFinished creating threads\n\n\n");
                


                // Start tasks
                pick_up_point = 0;
                char working_char, next_char;
                char last_byte, last_count = 0;
                char count = 0;
                for (pick_up_point = 0; pick_up_point < total_tasks; pick_up_point++)
                {
                    printf("Waiting for task %d to finish \n", pick_up_point);
                    sem_wait(&output_sem[pick_up_point]);
                    // output
                    working_char = output[pick_up_point * 4096 * 2];
                    count = output[pick_up_point * 4096 * 2 + 1];
                    next_char = output[pick_up_point * 4096 * 2 + 2];

                    // Check if the last byte is the same as the current byte
                    if (last_byte == working_char && last_count < 255)
                    {
                        last_count += count;
                    }
                    else
                    {
                        if (last_byte != -1)
                        {
                            fputc(last_byte, stdout);
                            fputc(last_count, stdout);
                        }
                    }

                    // Check if the next byte is the same as the current byte
                    while (next_char != 0)
                    {
                        fputc(working_char, stdout);
                        fputc(count, stdout);
                        pick_up_point++;

                        working_char = output[pick_up_point * 4096 * 2];
                        count = output[pick_up_point * 4096 * 2 + 1];
                        next_char = output[pick_up_point * 4096 * 2 + 2];
                    }

                    last_byte = working_char;
                    last_count = count;

                    pick_up_point++;
                }

                fputc(last_byte, stdout);
                fputc(last_count, stdout);

                // Join threads
                for (int i = 0; i < jobs; i++)
                {
                    pthread_join(threads[i], NULL);
                }

                // Destroy semaphores and threads
                for (int i = 0; i < total_tasks; i++)
                {
                    sem_destroy(&task_sem[i]);
                    sem_destroy(&output_sem[i]);
                }

                free(input);
                free(task_pickup);
                free(task_sem);
                free(output_sem);

                printf("\n");
            }
        }
    }
    return 0;
}
