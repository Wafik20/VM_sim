#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PAGE_SIZE 2048        // 2kb page size
#define ADDRESS_SIZE 32       // 32 bit virtual address
#define TABLE_ENTRIES 2097152 // 2^21 pages
#define INT_MAX 2147483647

// Global variables
int num_of_frames = 0; // This will be set from command line
char *algorithm = NULL;
int refresh_rate = 0;

// Stats
static int page_faults = 0;
static int writes = 0;
static int total_accesses = 0;

// Track of how many frames have been allocated so far
static int frames_allocated = 0;

// Structs to hold the page table entry
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

// OPT list
// begin implementation
// Define the two dimentional list
struct access
{
    int line_num;
    struct access *next;
};

struct page_list_opt
{
    int page_num;
    struct page_list_opt *next;
    struct access *head;
};

struct page_list_opt *create_page_list_node(int page_num)
{
    struct page_list_opt *node = (struct page_list_opt *)malloc(sizeof(struct page_list_opt));
    if (!node)
    {
        perror("Failed to allocate memory for page list node");
        exit(EXIT_FAILURE);
    }
    node->page_num = page_num;
    node->next = NULL;
    node->head = NULL;
    return node;
}

void add_access(struct page_list_opt *page_node, int line_num)
{
    struct access *new_access = (struct access *)malloc(sizeof(struct access));
    if (!new_access)
    {
        perror("Failed to allocate memory for access");
        exit(EXIT_FAILURE);
    }
    new_access->line_num = line_num;
    new_access->next = page_node->head;
    page_node->head = new_access;
}

struct page_list_opt *find_or_add_page(struct page_list_opt **root, int page_num)
{
    struct page_list_opt *current = *root;
    struct page_list_opt *last = NULL;

    // Search for the page or the last node in the list
    while (current != NULL && current->page_num != page_num)
    {
        last = current;
        current = current->next;
    }

    // If the page was found
    if (current != NULL)
    {
        return current;
    }

    // Page not found, create a new node
    struct page_list_opt *new_node = create_page_list_node(page_num);
    if (last == NULL)
    { // This means the list was empty
        *root = new_node;
    }
    else
    {
        last->next = new_node;
    }
    return new_node;
}

// Function to find a page in the list
struct page_list_opt *find_page(struct page_list_opt *root, int page_num)
{
    struct page_list_opt *current = root;

    // Search for the page
    while (current != NULL)
    {
        if (current->page_num == page_num)
        {
            return current; // Page found
        }
        current = current->next;
    }

    return NULL; // Page not found
}

void free_all(struct page_list_opt *root)
{
    while (root != NULL)
    {
        struct page_list_opt *temp = root;
        root = root->next;

        struct access *acc = temp->head;
        while (acc != NULL)
        {
            struct access *acc_temp = acc;
            acc = acc->next;
            free(acc_temp);
        }

        free(temp);
    }
}

void print_opt_list(struct page_list_opt *root)
{
    struct page_list_opt *current = root;
    // if list is empty, print empty and return
    if (current == NULL)
    {
        printf("List is empty.\n");
        return;
    }

    while (current != NULL)
    {
        printf("Page number: %d\n", current->page_num);
        struct access *acc = current->head;
        while (acc != NULL)
        {
            printf("Line number: %d\n", acc->line_num);
            acc = acc->next;
        }
        current = current->next;
    }
}

// Given a page number and current line number, return the line with closest distance to the current line number in the future
int get_closest_use_of_page(int page_num, int curr_line_num, struct page_list_opt *root)
{
    struct page_list_opt *target = find_page(root, page_num);
    if (target == NULL)
    {
        return -1; // Page not found, return an error code or handle it as appropriate
    }

    struct access *acc = target->head;
    int closest = INT_MAX; // Use INT_MAX to signify no future access is found

    // Loop over the accesses and find the closest one to curr_line_num
    while (acc != NULL)
    {
        if (acc->line_num > curr_line_num && acc->line_num < closest)
        {
            closest = acc->line_num;
        }
        acc = acc->next;
    }

    return closest == INT_MAX ? -1 : closest; // If no access is closer, return -1 or another indicator
}

// Function to get the page with the farthest first future use
int get_page_with_farthest_next_use(struct page_list_opt *root, int line_num)
{
    struct page_list_opt *current = root;
    int farthest = -1;      // Initialize to -1 to indicate no future use has been found yet
    int farthest_page = -1; // This will store the page number with the farthest future use

    while (current != NULL)
    {
        int first_future_use = get_closest_use_of_page(current->page_num, line_num, root);
        if (first_future_use > farthest)
        {
            farthest = first_future_use;
            farthest_page = current->page_num;
        }
        current = current->next;
    }

    return farthest_page; // Return the page number with the farthest future use, or -1 if none is found
}

// remove a page from the list
void remove_page(struct page_list_opt **root, int page_num)
{
    if (root == NULL || *root == NULL)
    {
        return; // List is empty or NULL pointer provided
    }

    struct page_list_opt *current = *root;
    struct page_list_opt *previous = NULL;

    // Find the page and keep track of the previous node
    while (current != NULL && current->page_num != page_num)
    {
        previous = current;
        current = current->next;
    }

    if (current == NULL)
    {
        return; // Page not found
    }

    // If the page is the first node in the list
    if (previous == NULL)
    {
        *root = current->next; // Update root to be the next node
    }
    else
    {
        previous->next = current->next; // Bypass the current node
    }

    // Free the associated accesses
    struct access *acc = current->head;
    while (acc != NULL)
    {
        struct access *next = acc->next;
        free(acc);
        acc = next;
    }

    // Free the page node itself
    free(current);
}

// end implementation

// Circular linked list implementation
// begin implementation
// Define the doubly circular linked list node
struct node
{
    int ref;
    int page_number;
    struct node *next;
    struct node *prev;
};

// Define the list structure that will handle the nodes
struct page_list
{
    struct node *head;
};

// Function to create a new node
struct node *create_node(int ref, int page_number)
{
    struct node *new_node = (struct node *)malloc(sizeof(struct node));
    if (new_node == NULL)
    {
        perror("Unable to allocate memory for new node");
        exit(EXIT_FAILURE);
    }

    new_node->ref = ref;
    new_node->page_number = page_number;

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
        printf("Page number: %d, ref: %d\n", temp->page_number, temp->ref);
        temp = temp->next;
    } while (temp != list->head);
}

// Declare a list for the clock algorithm
struct page_list clock_list;
struct page_list_opt *opt_list = NULL;

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

int opt(int line_num, int page_num)
{
    int farthest_page = get_page_with_farthest_next_use(opt_list, line_num);
    if (farthest_page == -1)
    {
        perror("Failed to find a page with farthest next use");
        return -1;
    }

    return farthest_page;
}

int clock()
{
    printf("Clock algorithm\n");
    struct node *current = clock_list.head;
    while (1)
    {
        if (current->ref == 0)
        {
            int page_to_be_evicted = current->page_number;

            // remove page from clock list
            if (current->next == current)
            {
                clock_list.head = NULL;
            }
            else
            {
                current->prev->next = current->next;
                current->next->prev = current->prev;
                if (clock_list.head == current)
                {
                    clock_list.head = current->next;
                }
            }
            return page_to_be_evicted;
        }
        else
        {
            current->ref = 0;
            current = current->next;
        }
    }
    return -1;
}

void init_opt_list(struct page_list_opt **opt_list, FILE *trace_file)
{
    char line[128];
    int line_num = 0;
    struct page_list_opt *page_node;

    // Read each line from the trace file
    while (fgets(line, sizeof(line), trace_file) != NULL)
    {
        struct tuple mem_access = sanitize_trace_line(line);
        int page_number = get_page_number(mem_access.add);

        // Find or create a new page list node for the current page number
        page_node = find_or_add_page(opt_list, page_number);
        add_access(page_node, line_num);

        line_num++; // Increment line number for the next read
    }

    // Reset the file pointer to the beginning of the file for future use
    rewind(trace_file);
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

        if (instruction_type == 'M')
        {
            total_accesses += 2;
        }
        else
        {
            total_accesses++;
        } 

        printf("\n\n\n total_access so far: %d\n", total_accesses);

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
            page_faults++;

            // Add the frame to clock list
            struct node *new_node = create_node(1, page_number);
            insert_node(&clock_list, new_node);

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
                    to_be_evicted = opt(line_num, page_number);
                    if (to_be_evicted == -1)
                    {
                        perror("Optimal algorithm failed to find a frame to evict");
                        return;
                    }
                }
                else if (strcmp(algorithm, "nru") == 0)
                {
                    to_be_evicted = nru(line_num);
                }
                else if (strcmp(algorithm, "clock") == 0)
                {
                    // Clock algorithm
                    to_be_evicted = clock();

                    // Remove the page from the clock list
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

                // If to_be_evicted is dirty, write to disk
                if (evicted_entry->dirty == 1)
                {
                    writes++;
                    printf("Writing frame %d to disk\n", to_be_evicted);
                }

                // Clean up the evicted frame
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

void print_stats(char *algorithm)
{
    printf("\n\n\nStats:#######################################################\n");
    printf("Algorithm: %s\n", algorithm);
    printf("Number of Frame: %d\n", num_of_frames);
    printf("Total Accesses: %d\n", total_accesses);
    printf("Page Faults: %d\n", page_faults);
    printf("Writes: %d\n", writes);
}

void print_usage()
{
    printf("Usage: vmsim -n <numframes> -a <opt|clock|nru> [-r <refresh>] <tracefile>\n");
}

int main(int argc, char *argv[])
{
    int opt;
    extern char *optarg;
    extern int optind;
    int n_flag = 0, a_flag = 0, r_flag = 0;
    char *tracefile = NULL;
    clock_list.head = NULL;

    while ((opt = getopt(argc, argv, "n:a:r:")) != -1)
    {
        switch (opt)
        {
        case 'n':
            num_of_frames = atoi(optarg);
            if (num_of_frames <= 0)
            {
                fprintf(stderr, "Invalid number of frames: Must be greater than zero.\n");
                return EXIT_FAILURE;
            }
            n_flag = 1;
            break;
        case 'a':
            algorithm = optarg;
            a_flag = 1;
            break;
        case 'r':
            refresh_rate = atoi(optarg);
            if (refresh_rate <= 0)
            {
                fprintf(stderr, "Invalid refresh rate: Must be greater than zero.\n");
                return EXIT_FAILURE;
            }
            r_flag = 1;
            break;
        case '?':
            print_usage();
            return EXIT_FAILURE;
        }
    }

    if (optind >= argc)
    {
        fprintf(stderr, "Missing trace file.\n");
        print_usage();
        return EXIT_FAILURE;
    }

    tracefile = argv[optind];

    if (!n_flag || !a_flag || strcmp(algorithm, "nru") == 0 && !r_flag)
    {
        fprintf(stderr, "Missing required arguments.\n");
        print_usage();
        return EXIT_FAILURE;
    }

    if (strcmp(algorithm, "opt") == 0)
    {
        FILE *f = fopen(tracefile, "r");
        if (f == NULL)
        {
            perror("Failed to open trace file");
            return EXIT_FAILURE;
        }

        init_opt_list(&opt_list, f);
        fclose(f);
    }

    FILE *f = fopen(tracefile, "r");
    if (f == NULL)
    {
        perror("Failed to open trace file");
        return EXIT_FAILURE;
    }

    process_trace_file(f);
    print_stats(algorithm);
    fclose(f);

    free_all(opt_list);

    return 0;
}