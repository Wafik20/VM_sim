#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PAGE_SIZE 2048        // 2kb page size
#define ADDRESS_SIZE 32       // 32 bit virtual address
#define TABLE_ENTRIES 2097152 // 2^21 pages

// Global variables
int num_of_frames = 0; // This will be set from command line
char *algorithm = NULL;
int refresh_rate = 0;

static int frames_allocated = 0;

struct page_table_entry
{
    int valid;
    int ref;
    int dirty;
};

struct tuple
{
    char instruction_type;
    __uint32_t add;
};

// Circular linked list implementation
// begin implementation
// Define the doubly circular linked list node
struct node
{
    struct page_table_entry entry;
    struct node *next;
    struct node *prev;
};

// Define the list structure that will handle the nodes
struct page_list
{
    struct node *head;
};

// Function to create a new node
struct node *create_node(int valid, int ref, int dirty)
{
    struct node *new_node = (struct node *)malloc(sizeof(struct node));
    if (new_node == NULL)
    {
        perror("Unable to allocate memory for new node");
        exit(EXIT_FAILURE);
    }
    new_node->entry.valid = valid;
    new_node->entry.ref = ref;
    new_node->entry.dirty = dirty;
    new_node->next = new_node; // Points to itself, circular by default
    new_node->prev = new_node; // Points to itself, circular by default
    return new_node;
}

// Function to insert a node at the end of the list
void insert_node(struct page_list *list, struct node *new_node)
{
    if (list->head == NULL)
    {
        // List is empty
        list->head = new_node;
        new_node->next = new_node;
        new_node->prev = new_node;
    }
    else
    {
        // Insert new node before head and make it the new tail
        struct node *tail = list->head->prev;
        tail->next = new_node;
        new_node->prev = tail;
        new_node->next = list->head;
        list->head->prev = new_node;
    }
}

// Function to display the list
void display_list(struct page_list *list)
{
    if (list->head == NULL)
    {
        printf("List is empty.\n");
        return;
    }
    struct node *temp = list->head;
    do
    {
        printf("Node - Valid: %d, Ref: %d, Dirty: %d\n", temp->entry.valid, temp->entry.ref, temp->entry.dirty);
        temp = temp->next;
    } while (temp != list->head);
}

// Declare a list for the clock algorithm
struct page_list clock_list;
// end implementation

static struct page_table_entry page_table[TABLE_ENTRIES];

int get_page_number(__uint32_t virt_address)
{
    return virt_address >> 11;
}

int get_offset(__uint32_t virt_address)
{
    return virt_address & 0x7FF;
}

struct tuple sanitize_trace_line(char *trace_line)
{
    struct tuple result;
    result.instruction_type = trace_line[0];
    sscanf(trace_line + 1, "%*c %x", &result.add);
    return result;
}

void allocate_frame(char instruction_type, __uint32_t address, int page_number, int offset, int *valid, int *ref, int *dirty)
{
    // Update the page table entry
    // set the ref to 1 anyways
    *ref = 1;
    // we know valid is 1, or else we wouldn't have allocated.
    *valid = 1;
    // set dirty to 0
    *dirty = 0;

    switch (instruction_type)
    {
    case 'I':
        printf("I type: Address: 0x%X, putting in page number %d, with offset %d\n", address, page_number, offset);
        break;
    case 'L':
        printf("L type: Address: 0x%X, putting in page number %d, with offset %d\n", address, page_number, offset);
        break;
    case 'S':
        printf("S type: Address: 0x%X, putting in page number %d, with offset %d\n", address, page_number, offset);
        // set dirty to 1
        *dirty = 1;
        break;
    case 'M':
        printf("M type: Address: 0x%X, putting in page number %d, with offset %d\n", address, page_number, offset);
        // set dirty to 1
        *dirty = 1;
        break;
    default:
        break;
    }
}

int nru(int line_num)
{
    // if line_num % refresh_rate == 0, reset ref bit
    if (line_num % refresh_rate == 0)
    {
        for (int i = 0; i < num_of_frames; i++)
        {
            page_table[i].ref = 0;
        }
    }

    int first_class_0 = -1;
    int first_class_1 = -1;
    int first_class_2 = -1;
    int first_class_3 = -1;
    for (int i = 0; i < num_of_frames; i++)
    {
        struct page_table_entry *entry = &page_table[i];

        if (entry->ref == 0 && entry->dirty == 0)
        { // class 0
            if (first_class_0 == -1)
            {
                first_class_0 = i;
                return first_class_0;
            }
        }
        else if (entry->ref == 0 && entry->dirty == 1)
        { // class 1
            if (first_class_1 == -1)
            {
                first_class_1 = i;
                continue;
            }
        }
        else if (entry->ref == 1 && entry->dirty == 0)
        { // class 2
            if (first_class_2 == -1)
            {
                first_class_2 = i;
                continue;
            }
        }
        else if (entry->ref == 1 && entry->dirty == 1)
        { // class 3
            if (first_class_3 == -1)
            {
                first_class_3 = i;
                continue;
            }
        }
    }

    if (first_class_0 >= 0)
    {
        return first_class_0;
    }
    else if (first_class_1 >= 0)
    {
        return first_class_1;
    }
    else if (first_class_2 >= 0)
    {
        return first_class_2;
    }
    else if (first_class_3 >= 0)
    {
        return first_class_3;
    }

    return -1;
}

int opt()
{
    printf("Optimal algorithm\n");
    return 0;
}

int clock()
{
}

void process_trace_file(FILE *f)
{
    if (f == NULL)
    {
        perror("Unable to open file!");
        return;
    }

    char line[128];
    int line_num = 0;
    while (fgets(line, sizeof(line), f) != NULL)
    {
        // Sanitize the trace line by line
        struct tuple mem_access = sanitize_trace_line(line);

        // Get the intruction type and address
        char instruction_type = mem_access.instruction_type;
        __uint32_t address = mem_access.add;

        // Compute page number and offset
        int page_number = get_page_number(address);
        int offset = get_offset(address);

        // Check that page number is in bounds
        if (page_number >= TABLE_ENTRIES)
        {
            perror("invalid page number");
            return;
        }

        // Use proper page entry
        struct page_table_entry *entry = &page_table[page_number];
        int is_valid = entry->valid;
        int is_referenced = entry->ref;
        int is_dirty = entry->dirty;

        // if the page is invalid, allocate a frame
        if (is_valid == 0)
        {
            printf("Page Fault, allocating frame...\n");
            if (frames_allocated < num_of_frames)
            {
                printf("Frame %d allocated\n", frames_allocated);
                allocate_frame(instruction_type, address, page_number, offset, &entry->valid, &entry->ref, &entry->dirty);
                frames_allocated++;
            }
            else
            {
                printf("No frames available, evicting...\n");
                // Evict a frame using an algorithm opt, nru, clock.
                // Each algorithm will take an array of frames and return the frame to be evicted (int).
                int to_be_evicted = -1;
                if (algorithm == NULL)
                {
                    perror("No algorithm specified");
                    return;
                }
                if (strcmp(algorithm, "opt") == 0)
                {
                    // Optimal algorithm
                    to_be_evicted = opt();
                }
                else if (strcmp(algorithm, "nru") == 0)
                {
                    to_be_evicted = nru(line_num);
                }
                else if (strcmp(algorithm, "clock") == 0)
                {
                    // Clock algorithm
                    to_be_evicted = clock();
                }
                else
                {
                    perror("Invalid algorithm specified");
                    return;
                }

                // Evict the frame
                if (to_be_evicted < 0)
                {
                    perror("Optimal algorithm failed to find a frame to evict");
                    return;
                }

                printf("Frame %d evicted\n", to_be_evicted);

                // Evict the frame
                struct page_table_entry *evicted_entry = &page_table[to_be_evicted];
                evicted_entry->valid = 0;
                evicted_entry->ref = 0;
                evicted_entry->dirty = 0;

                // get page number and offset of the evicted frame
                int evicted_page_number = to_be_evicted;
                int evicted_offset = 0;

                // Allocate the new frame
                printf("Frame %d allocated\n", to_be_evicted);
                allocate_frame(instruction_type, address, evicted_page_number, evicted_offset, &evicted_entry->valid, &evicted_entry->ref, &evicted_entry->dirty);
            }
        }
        else
        {
            printf("Page hit\n");
        }

        // increment the line number
        line_num++;
    }
}

int main(int argc, char *argv[])
{
    clock_list.head = NULL;
    if (argc < 4)
    {
        perror("Usage: ./vm num_of_frames algorithm refresh_rate");
        return 1;
    }

    num_of_frames = atoi(argv[1]);
    algorithm = argv[2];
    refresh_rate = atoi(argv[3]);

    FILE *f = fopen("trace.txt", "r");
    process_trace_file(f);
    fclose(f);

    return 0;
}