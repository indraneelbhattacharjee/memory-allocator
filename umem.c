#include "umem.h"
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define PAGE_SIZE 4096
#define UMEM_ERROR -1

static void *memory_region = NULL;
static void *free_list = NULL;
static size_t previous_allocated_size = 0;
static int alloc_algo;
static size_t region_size = 0;
static int is_initialized = 0;

// Function prototypes
void *best_fit_algorithm(size_t size);
void *first_fit_algorithm(size_t size);
void *next_fit_algorithm(size_t size);
void *worst_fit_algorithm(size_t size);
void coalescing_memory();

// Initialize the memory allocator
int umeminit(size_t sizeOfRegion, int allocationAlgo) {
  // Check if memory system is already initialized
  if (is_initialized) {
    fprintf(stderr, "Memory system already initialized.\n");
    return UMEM_ERROR;
  }

  if (sizeOfRegion <= 0) {
    return UMEM_ERROR; // Invalid size
  }

  // Align the requested region size to the nearest page size
  sizeOfRegion = (sizeOfRegion + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

  // Use mmap with MAP_ANONYMOUS
  memory_region = mmap(NULL, sizeOfRegion, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (memory_region == MAP_FAILED) {
    perror("mmap");
    return UMEM_ERROR;
  }

  // Set up the initial free block
  free_list = memory_region;
  *((size_t *)free_list) =
      sizeOfRegion - sizeof(size_t); // Store the size of the initial block
  *((void **)((char *)free_list + sizeof(size_t))) = NULL; // No next block

  // Store settings and mark initialized
  alloc_algo = allocationAlgo;
  previous_allocated_size = sizeOfRegion;
  region_size = sizeOfRegion;
  is_initialized = 1; // Mark as initialized

  return 0;
}

// Allocate memory using the below allocation algorithm
void *umalloc(size_t size) {
  switch (alloc_algo) {
  case FIRST_FIT:
    return first_fit_algorithm(size);
  case BEST_FIT:
    return best_fit_algorithm(size);
  case WORST_FIT:
    return worst_fit_algorithm(size);
  case NEXT_FIT:
    return next_fit_algorithm(size);
  default:
    return NULL;
  }
}

int ufree(void *ptr) {
  if (ptr == NULL) {
    printf("Attempt to free a NULL pointer.\n");
    return UMEM_ERROR; // No operation if ptr is NULL
  }

  void *block_to_free = (char *)ptr - sizeof(size_t);
  size_t block_size = *((size_t *)block_to_free);

  printf("Freeing block at %p with size %zu\n", block_to_free, block_size);

  // Find the appropriate position to insert the block into the free list
  void *current_block = free_list;
  void *prev_block = NULL;

  while (current_block != NULL && current_block < block_to_free) {
    prev_block = current_block;
    current_block = *((void **)((char *)current_block + sizeof(size_t)));
  }

  // Link the new free block into the free list
  if (prev_block != NULL) {
    *((void **)((char *)prev_block + sizeof(size_t))) = block_to_free;
  } else {
    free_list = block_to_free; // Insert at the head if no previous block
  }

  *((void **)((char *)block_to_free + sizeof(size_t))) = current_block;

  // Coalesce free memory blocks
  coalescing_memory();

  return 0;
}

// Function to coalesce contiguous free memory blocks
void coalescing_memory() {
  void *current_block =
      free_list; // Start with the first block in the free list.

  // Iterate over the free list until the end is reached or the next block pointer is NULL
  while (current_block != NULL &&
         *((void **)(current_block + sizeof(size_t))) != NULL) {
    void *next_block =
        *((void **)(current_block +
                    sizeof(size_t))); // Retrieve the pointer to the next block in the free list.

    // Calculate if the current block is immediately adjacent to the next block in memory
    if ((char *)current_block + *((size_t *)current_block) + sizeof(size_t) ==
        next_block) {
      printf("Coalescing %p and %p\n", current_block,
             next_block); // Print a message indicating the blocks are being coalesced.

      // Increase the size of the current block by the size of the next block plus the size of the header (size field).
      *((size_t *)current_block) += *((size_t *)next_block) + sizeof(size_t);

      // Update the link in the current block to skip the next block, linking directly to the block after the next block.
      *((void **)(current_block + sizeof(size_t))) =
          *((void **)(next_block + sizeof(size_t)));
    } else {
      // Move to the next block if they are not adjacent (no coalescing needed).
      current_block = *((void **)(current_block + sizeof(size_t)));
    }
  }
}

void umemdump() {
  void *current_block = free_list;
  size_t block_number = 1;

  printf("Memory Dump:\n");
  printf("-------------------------------------------------\n");
  printf("| Block Number | Block Size | Block Address    |\n");
  printf("-------------------------------------------------\n");

  while (current_block != NULL) {
    printf("| %-12zu| %-11zu| %-18p|\n", block_number,
           *((size_t *)current_block), (void *)current_block);
    block_number++;
    current_block = *((void **)((char *)current_block + sizeof(size_t)));
  }

  printf("-------------------------------------------------\n");
}

void *allocate_memory_block(void *block, size_t requested_size) {
  size_t block_size = *((size_t *)block);

  // Check if the remaining space after allocation is enough for another block
  if (block_size > requested_size + sizeof(size_t) + sizeof(void *)) {
    // Split the block
    size_t remaining_size = block_size - requested_size - sizeof(size_t);
    void *new_block = (char *)block + sizeof(size_t) + requested_size;
    *((size_t *)new_block) = remaining_size;
    *((void **)(new_block + sizeof(size_t))) =
        *((void **)(block + sizeof(size_t)));
    *((size_t *)block) = requested_size;
  } else {
    // If not enough space to split, update the linked list to remove this block
    void *next_block = *((void **)(block + sizeof(size_t)));
    if (block == free_list) {
      free_list = next_block; // Update head of the list if necessary
    } else {
      // Find the previous block and update its next pointer
      void *prev_block = free_list;
      while (prev_block && *((void **)(prev_block + sizeof(size_t))) != block) {
        prev_block = *((void **)(prev_block + sizeof(size_t)));
      }
      if (prev_block) {
        *((void **)(prev_block + sizeof(size_t))) = next_block;
      }
    }
  }
  return block + sizeof(size_t); // Return the address after the size header
}
void print_free_list() {
  void *current = free_list;
  printf("Current Free List:\n");
  while (current != NULL) {
    size_t current_size = *((size_t *)current);
    void *next = *((void **)(current + sizeof(size_t)));
    printf("Block at %p, size %zu, next %p\n", current, current_size, next);
    current = next;
  }
}
// First Fit allocation algorithm
void *first_fit_algorithm(size_t size) {
  size = (size + 7) & ~7; // Aligns the requested size to the nearest multiple
                          // of 8 bytes for alignment purposes.

  void *current_block =
      free_list; // Start searching from the head of the free list.
  void *prev_block =
      NULL; // To keep track of the previous block in the free list.

  printf("Starting allocation for size %zu\n",
         size); // Debugging output to show the start of an allocation attempt.
  while (current_block != NULL) { // Iterate over all blocks in the free list.
    size_t block_size =
        *((size_t *)current_block); // Read the size of the current block.
    printf("Checking block at %p with size %zu\n", current_block,
           block_size); // Debugging output for current block.

    if (block_size >= size) { // Check if the current block has enough space to
                              // fulfill the request.
      printf("Found suitable block at %p\n",
             current_block); // Debugging output when a suitable block is found.
      if (block_size >
          size + sizeof(size_t)) { // Check if the block is big enough to split.
        void *new_block =
            (char *)current_block + sizeof(size_t) +
            size; // Create a new block at the end of the allocated space.
        *((size_t *)new_block) =
            block_size - size -
            sizeof(size_t); // Set the size of the new block.
        *((void **)new_block + 1) = *(
            (void **)((char *)current_block +
                      sizeof(size_t))); // Link the new block to the next block.
        *((size_t *)current_block) =
            size; // Update the size of the current block to the requested size.
        if (prev_block != NULL) {
          *((void **)(prev_block + 1)) =
              new_block; // Link the previous block to the new block if not the
                         // first block.
        }
      }

      if (prev_block == NULL) {
        free_list = *((void **)((char *)current_block +
                                sizeof(size_t))); // Update the head of the free
                                                  // list if necessary.
      } else {
        *((void **)(prev_block + 1)) =
            *((void **)((char *)current_block +
                        sizeof(size_t))); // Link the previous block to the next
                                          // block.
      }
      return (char *)current_block +
             sizeof(size_t); // Return a pointer to the allocated space.
    }
    prev_block = current_block; // Move prev_block pointer to current block.
    current_block =
        *((void **)((char *)current_block +
                    sizeof(size_t))); // Move to the next block in the list.
  }

  printf("No suitable block found\n"); // Output if no suitable block is found.
  return NULL; // Return NULL if no block is big enough to satisfy the
               // allocation request.
}

// Best Fit allocation algorithm
void *best_fit_algorithm(size_t size) {
  size = (size + sizeof(size_t) - 1) &
         ~(sizeof(size_t) - 1); // Aligns the requested size to the word size.
  void *best_block = NULL;      // To store the best fitting block found.
  void **best_prev_pointer =
      NULL; // To keep track of the pointer pointing to the best block.
  size_t smallest_diff =
      SIZE_MAX; // To find the block with the minimum difference in size.

  void **prev_pointer =
      &free_list; // Pointer to the pointer leading to the current block,
                  // starting with the free list head.
  void *current_block =
      free_list; // Start searching from the head of the free list.

  printf("Starting Best Fit allocation for size %zu\n",
         size); // Debugging output to show the start of an allocation attempt.

  while (current_block != NULL) { // Iterate over all blocks in the free list.
    size_t current_size =
        *((size_t *)current_block); // Get the size of the current block.
    if (current_size >= size) { // Check if the current block has enough space.
      size_t diff =
          current_size - size;    // Calculate the difference after allocation.
      if (diff < smallest_diff) { // Check if this block offers a better fit.
        smallest_diff = diff;     // Update the smallest difference found.
        best_block = current_block; // Update the best block.
        best_prev_pointer =
            prev_pointer; // Update the pointer to the best block.
      }
    }
    prev_pointer =
        (void **)((char *)current_block +
                  sizeof(size_t)); // Move to the next block's pointer location.
    current_block = *prev_pointer; // Move to the next block.
  }

  if (best_block) { // Check if a suitable block was found.
    printf("Best block found at %p with size %zu, difference %zu\n", best_block,
           *((size_t *)best_block),
           smallest_diff); // Debugging output for the best block found.
    size_t remaining_size =
        *((size_t *)best_block) -
        size; // Calculate the remaining size after allocation.
    if (remaining_size >
        sizeof(size_t) + sizeof(void *)) { // Check if there is enough space
                                           // left to split the block.
      void *new_block = (char *)best_block + sizeof(size_t) +
                        size; // Create a new block after the allocated space.
      *((size_t *)new_block) =
          remaining_size - sizeof(size_t); // Set the new block's size.
      *((void **)new_block + 1) =
          *((void **)((char *)best_block +
                      sizeof(size_t))); // Link the new block to the next block.
      *((size_t *)best_block) =
          size; // Set the size of the best block to the requested size.
      *best_prev_pointer =
          new_block; // Redirect the pointer to the best block to the new block.
    } else {
      *best_prev_pointer =
          *((void **)((char *)best_block +
                      sizeof(size_t))); // If not splitting, just update the
                                        // pointer to skip the best block.
    }
    return (char *)best_block +
           sizeof(size_t); // Return the address of the allocated space.
  }

  printf("No suitable block found\n"); // Output if no suitable block is found.
  return NULL; // Return NULL if no block is big enough to satisfy the
               // allocation request.
}

void *worst_fit_algorithm(size_t size) {
  void *worst_block = NULL;
  void **worst_prev_pointer =
      NULL; // This will track the link just before the worst block
  size_t largest_diff = 0;
  void **prev_pointer =
      &free_list; // Starting by pointing to the head of the list

  void *current_block = free_list;
  while (current_block != NULL) {
    size_t current_size = *((size_t *)current_block);
    if (current_size >= size) { // Checking if the block is large enough
      size_t diff = current_size - size;
      if (diff > largest_diff) { // Checking if this block is a worse fit
        largest_diff = diff;
        worst_block = current_block;
        worst_prev_pointer =
            prev_pointer; // Saving the location of the pointer to this block
      }
    }
    prev_pointer =
        (void **)((char *)current_block +
                  sizeof(size_t)); // Move to next block's pointer location
    current_block = *prev_pointer; // Move to the next block
  }

  if (worst_block != NULL) {
    // Allocate the block
    size_t remaining_size = *((size_t *)worst_block) - size - sizeof(size_t);
    if (remaining_size > sizeof(size_t)) { // Checking if there is enough space
                                           // to create a new free block
      void *new_block = (char *)worst_block + sizeof(size_t) + size;
      *((size_t *)new_block) = remaining_size; // Set the size of the new block
      *((void **)new_block + 1) =
          *((void **)((char *)worst_block +
                      sizeof(size_t))); // Link to the next block
      *worst_prev_pointer =
          new_block; // Redirect the previous block's pointer to the new block
    } else {
      *worst_prev_pointer = *(
          (void **)((char *)worst_block +
                    sizeof(size_t))); // Remove the current block from the list
    }
    *((size_t *)worst_block) = size; // Update the size of the allocated block
    return (char *)worst_block +
           sizeof(size_t); // Return the pointer to the usable memory (after the
                           // size field)
  }

  return NULL; // If no suitable block found, returning NULL
}

// Next Fit allocation algorithm

static void *last_allocated =
    NULL; // Track the last allocated block for next fit

void *next_fit_algorithm(size_t size) {
  size = (size + sizeof(size_t) - 1) &
         ~(sizeof(size_t) - 1); // Alignment to word size

  if (!last_allocated || (char *)last_allocated < (char *)memory_region ||
      (char *)last_allocated >= (char *)memory_region + region_size) {
    last_allocated = free_list; // Ensure last_allocated is within bounds
  }

  void *start = last_allocated;
  do {
    size_t current_size = *(size_t *)last_allocated;
    if (current_size >= size) {
      size_t remaining_size = current_size - size - sizeof(size_t);
      if (remaining_size > sizeof(size_t) * 2) {
        void *new_block = (char *)last_allocated + size + sizeof(size_t);
        *(size_t *)new_block = remaining_size;
        *(void **)((char *)new_block + sizeof(size_t)) =
            *(void **)((char *)last_allocated + sizeof(size_t));
        *(size_t *)last_allocated = size;
        last_allocated = new_block; // Moving to next free block
      } else {
        size = current_size; // Allocate the entire block if too small to split
      }
      return (char *)last_allocated + sizeof(size_t);
    }
    last_allocated = *(void **)((char *)last_allocated + sizeof(size_t));
    if (!last_allocated)
      last_allocated = free_list; // Wrap around
  } while (last_allocated != start);

  return NULL; // If no suitable block found, returning NULL
}