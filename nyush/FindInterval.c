#include <stdio.h>
#include <stdlib.h>

struct pair
{
    int start;
    int end;
};

struct pair find_interval(int arr[], int size, int interval) {
    int start_index = -1;
    int end_index = size-1;
    int count = 0;
    
    for (int i = 0; i < size; i++) {
        if (arr[i] == 1) {
            count++;
            if (count == interval) {
                start_index = i;
            }
            else if (count == interval+1) {
                end_index = i-1;
                break;
            }
        }
    }
    
    if (start_index != -1 && end_index == -1) {
        end_index = size-1;
    }
    start_index++;
    struct pair result = {start_index, end_index};
    return result;
}

int main()
{
    // int arr[] = {0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 0, 1, 0, 0};
    int arr[] = {0, 0, 1, 0, 0, 1, 0};
    int size = sizeof(arr) / sizeof(arr[0]);
    for (int i = 0; i < 3; i++)
    {
        int interval = i;
        struct pair result = find_interval(arr, size, interval);
        printf("Interval %d: [%d,%d]\n", interval, result.start, result.end);
    }
    return 0;
}