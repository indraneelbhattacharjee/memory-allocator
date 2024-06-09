#include "umem.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define UMEM_ERROR -1

// Test function declarations
void test_first_fit();
void test_first_fit_allocate_free_sequence();
void test_first_fit_fragmentation();
void test_first_fit_reuse_freed_space();
void test_best_fit();
void test_best_fit_optimal_fit();
void test_best_fit_edge_fit();
void test_best_fit_mix_size();
void test_worst_fit();
void test_worst_fit_large_block_fragmentation();
void test_worst_fit_extreme_sizes();
void test_worst_fit_sequential_fragment();
void test_next_fit();
void test_next_fit_cyclic_allocation();
void test_next_fit_random_allocation();
void test_next_fit_sequential_fit();
void test_random_first_fit();
void test_random_best_fit();
void test_random_worst_fit();
void test_random_next_fit();

typedef void (*TestFunc)();
/*
 * Each test function is executed within its own subprocess to ensure
 * that 'umeminit' must be called only once per process.
 * By using 'fork()', each test is isolated in a separate process environment.
 * This isolation guarantees that each invocation of 'umeminit' occurs
 * independently and exactly once per process lifecycle. After executing
 * 'umeminit' and the associated test operations, each subprocess terminates,
 * ensuring no subsequent calls to 'umeminit' can occur within the same
 * process.
 */

int run_test_in_subprocess(TestFunc func, const char *test_name) {
  int status;
  pid_t pid = fork();

  if (pid == -1) {
    perror("fork");
    return EXIT_FAILURE;
  } else if (pid == 0) {
    // Child process executes the test
    printf("\n\nRunning %s...\n\n", test_name);
    func();
    exit(EXIT_SUCCESS);
  } else {
    // Parent process waits for the child to complete
    waitpid(pid, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS) {
      printf("\n\nTest %s passed\n", test_name);
    } else {
      printf("\n\nTest %s failed\n", test_name);
    }
  }

  return WEXITSTATUS(status);
}

int main() {
  TestFunc tests[] = {test_first_fit,
                      test_first_fit_allocate_free_sequence,
                      test_first_fit_fragmentation,
                      test_first_fit_reuse_freed_space,
                      test_best_fit,
                      test_best_fit_optimal_fit,
                      test_best_fit_edge_fit,
                      test_best_fit_mix_size,
                      test_worst_fit,
                      test_worst_fit_large_block_fragmentation,
                      test_worst_fit_extreme_sizes,
                      test_worst_fit_sequential_fragment,
                      test_next_fit,
                      test_next_fit_cyclic_allocation,
                      test_next_fit_random_allocation,
                      test_next_fit_sequential_fit,
                      test_random_first_fit,
                      test_random_best_fit,
                      test_random_worst_fit,
                      test_random_next_fit};

  const char *test_names[] = {"First Fit Basic",
                              "First Fit Allocate and Free in Sequence",
                              "First Fit Fragmentation",
                              "First Fit Reuse of Freed Space",
                              "Best Fit Basic",
                              "Best Fit Optimal Fit",
                              "Best Fit Edge Fit",
                              "Best Fit Mix Size Allocation and Free",
                              "Worst Fit Basic",
                              "Worst Fit Large Block Fragmentation",
                              "Worst Fit Testing with Extreme Sizes",
                              "Worst Fit Sequential Fragment Creation",
                              "Next Fit Basic",
                              "Next Fit Cyclic Allocation",
                              "Next Fit Random Allocation",
                              "Next Fit Sequential Fit ",
                              "Random Allocations Test - First Fit",
                              "Random Allocations Test - Best Fit",
                              "Random Allocations Test - Worst Fit",
                              "Random Allocations Test - Next Fit"};

  int num_tests = sizeof(tests) / sizeof(tests[0]);
  for (int i = 0; i < num_tests; i++) {
    run_test_in_subprocess(tests[i], test_names[i]);
    if (i < num_tests - 1) {
      printf("\n\nMoving to the next test case...\n");
    }
  }
  printf("\nAll test cases completed.\n");
  return 0;
}
// Tests the First Fit allocation strategy with a simple allocation and free.
void test_first_fit() {
  printf("Initializing memory with First Fit strategy...\n");
  if (umeminit(4096, FIRST_FIT) == UMEM_ERROR) {
    printf("Error: Memory initialization failed.\n");
    return;
  }
  void *ptr = umalloc(100);
  if (!ptr) {
    printf("Error: Allocation failed.\n");
    return;
  }
  printf("Allocation successful at address: %p\n", ptr);
  if (ufree(ptr) == UMEM_ERROR) {
    printf("Error: Freeing memory failed.\n");
    return;
  }
  printf("Memory freed successfully.\n");
  umemdump();
}

// Tests the First Fit algorithm by sequentially allocating and freeing memory
// to check proper handling of free space.
void test_first_fit_allocate_free_sequence() {
  printf("\n\nTesting First Fit Allocate and Free in Sequence...\n\n");
  umeminit(4096, FIRST_FIT);
  void *ptr1 = umalloc(100);
  printf("Allocated 100 bytes at address: %p\n", ptr1);
  ufree(ptr1);
  printf("Freed memory at address: %p\n", ptr1);
  void *ptr2 = umalloc(100);
  printf("Re-allocated 100 bytes at address: %p\n", ptr2);
  ufree(ptr2);
  printf("Freed memory at address: %p\n", ptr2);
  umemdump();
}

// Tests the First Fit algorithm with multiple allocations to create
// fragmentation, and checks allocation among fragments.
void test_first_fit_fragmentation() {
  printf("Testing First Fit with intentional fragmentation...\n");
  umeminit(4096, FIRST_FIT);
  void *ptr1 = umalloc(200);
  void *ptr2 = umalloc(300);
  void *ptr3 = umalloc(400);
  printf("Allocated 200, 300, 400 bytes at addresses: %p, %p, %p\n", ptr1, ptr2,
         ptr3);
  ufree(ptr2);
  printf("Freed middle block at address: %p\n", ptr2);
  void *ptr4 = umalloc(250);
  printf("Allocated 250 bytes at address: %p in fragmented space\n", ptr4);
  ufree(ptr1);
  ufree(ptr3);
  ufree(ptr4);
  umemdump();
}

// Tests the ability of the First Fit algorithm to reuse freed space
// effectively.
void test_first_fit_reuse_freed_space() {
  printf("Testing First Fit reuse of freed space...\n");
  umeminit(4096, FIRST_FIT);
  void *ptr1 = umalloc(300);
  printf("Allocated 300 bytes at address: %p\n", ptr1);
  ufree(ptr1);
  printf("Freed block at address: %p\n", ptr1);
  void *ptr2 = umalloc(150);
  printf("Reused freed space and allocated 150 bytes at address: %p\n", ptr2);
  ufree(ptr2);
  umemdump();
}

// Tests basic memory allocation using the Best Fit strategy to ensure it
// chooses the smallest suitable free block.
void test_best_fit() {
  printf("Initializing memory with Best Fit strategy...\n");
  if (umeminit(4096, BEST_FIT) == UMEM_ERROR) {
    printf("Error: Memory initialization failed.\n");
    return;
  }
  void *ptr = umalloc(100);
  if (!ptr) {
    printf("Error: Allocation failed.\n");
    return;
  }
  printf("Allocation successful at address: %p\n", ptr);
  if (ufree(ptr) == UMEM_ERROR) {
    printf("Error: Freeing memory failed.\n");
    return;
  }
  printf("Memory freed successfully.\n");
  umemdump();
}

// Tests the Best Fit allocation strategy's ability to find the optimal fitting
// block for reducing wasted space.
void test_best_fit_optimal_fit() {
  printf("Testing Best Fit optimal space utilization...\n");
  umeminit(4096, BEST_FIT);
  void *ptr1 = umalloc(500);
  printf("Allocated 500 bytes at address: %p\n", ptr1);
  ufree(ptr1);
  printf("Freed block at address: %p\n", ptr1);
  void *ptr2 = umalloc(300);
  printf("Re-allocated 300 bytes into optimal fit space at address: %p\n",
         ptr2);
  ufree(ptr2);
  umemdump();
}

// Tests Best Fit allocation edge cases to ensure it handles boundary conditions
// correctly.
void test_best_fit_edge_fit() {
  printf("Testing Best Fit edge cases...\n");
  umeminit(4096, BEST_FIT);
  void *ptr1 = umalloc(400);
  void *ptr2 = umalloc(200);
  printf("Allocated 400 and 200 bytes at addresses: %p, %p\n", ptr1, ptr2);
  ufree(ptr1);
  printf("Freed block at address: %p\n", ptr1);
  void *ptr3 = umalloc(350);
  printf("Allocated 350 bytes, testing edge fit at address: %p\n", ptr3);
  ufree(ptr2);
  ufree(ptr3);
  umemdump();
}

// Tests the Best Fit algorithm with a mix of different size allocations and
// frees to examine space utilization.
void test_best_fit_mix_size() {
  printf("Testing Best Fit with mixed size allocations...\n");
  umeminit(4096, BEST_FIT);
  void *ptr1 = umalloc(150);
  void *ptr2 = umalloc(250);
  void *ptr3 = umalloc(100);
  printf("Allocated 150, 250, 100 bytes at addresses: %p, %p, %p\n", ptr1, ptr2,
         ptr3);
  ufree(ptr2);
  printf("Freed 250 bytes at address: %p\n", ptr2);
  void *ptr4 = umalloc(200);
  printf("Allocated 200 bytes into freed space at address: %p\n", ptr4);
  ufree(ptr1);
  ufree(ptr3);
  ufree(ptr4);
  umemdump();
}

// Tests basic functionality of the Worst Fit allocation strategy which aims to
// reduce fragmentation.
void test_worst_fit() {
  printf("Initializing memory with Worst Fit strategy...\n");
  if (umeminit(4096, WORST_FIT) == UMEM_ERROR) {
    printf("Error: Memory initialization failed.\n");
    return;
  }
  void *ptr = umalloc(100);
  if (!ptr) {
    printf("Error: Allocation failed.\n");
    return;
  }
  printf("Allocation successful at address: %p\n", ptr);
  if (ufree(ptr) == UMEM_ERROR) {
    printf("Error: Freeing memory failed.\n");
    return;
  }
  printf("Memory freed successfully.\n");
  umemdump();
}

// Tests the Worst Fit strategy by creating large block fragmentation and
// verifying allocation within these spaces.
void test_worst_fit_large_block_fragmentation() {
  printf("Testing Worst Fit with intentional large block fragmentation...\n");
  umeminit(4096, WORST_FIT);
  void *ptr1 = umalloc(1000);
  void *ptr2 = umalloc(1000);
  printf("Allocated two 1000-byte blocks at addresses: %p, %p\n", ptr1, ptr2);
  ufree(ptr1);
  printf("Freed first block at address: %p\n", ptr1);
  void *ptr3 = umalloc(500);
  printf("Allocated 500 bytes into a large freed space at address: %p\n", ptr3);
  ufree(ptr2);
  ufree(ptr3);
  umemdump();
}

// Tests Worst Fit allocation with extreme size variations to check its
// effectiveness in space management.
void test_worst_fit_extreme_sizes() {
  printf("Testing Worst Fit with extreme sizes...\n");
  umeminit(4096, WORST_FIT);
  void *ptr1 = umalloc(2000);
  void *ptr2 = umalloc(50);
  printf("Allocated 2000 and 50 bytes at addresses: %p, %p\n", ptr1, ptr2);
  ufree(ptr1);
  printf("Freed large block at address: %p\n", ptr1);
  void *ptr3 = umalloc(1500);
  printf("Allocated 1500 bytes into largest available space at address: %p\n",
         ptr3);
  ufree(ptr2);
  ufree(ptr3);
  umemdump();
}

// Tests the Worst Fit strategy's handling of sequential fragmentation and
// allocation in fragmented spaces.
void test_worst_fit_sequential_fragment() {
  printf("Testing Worst Fit with sequential fragmentation...\n");
  umeminit(4096, WORST_FIT);
  void *ptr1 = umalloc(800);
  void *ptr2 = umalloc(800);
  printf("Allocated two 800-byte blocks at addresses: %p, %p\n", ptr1, ptr2);
  ufree(ptr1);
  printf("Freed one block to create fragmentation at address: %p\n", ptr1);
  void *ptr3 = umalloc(600);
  printf("Allocated 600 bytes into a fragmented space at address: %p\n", ptr3);
  ufree(ptr2);
  ufree(ptr3);
  umemdump();
}

// Tests the Next Fit allocation strategy's basic functionality by allocating
// and freeing a small block.
void test_next_fit() {
  printf("Initializing memory with Next Fit strategy...\n");
  if (umeminit(4096, NEXT_FIT) == UMEM_ERROR) {
    printf("Error: Memory initialization failed.\n");
    return;
  }
  void *ptr = umalloc(100);
  if (!ptr) {
    printf("Error: Allocation failed.\n");
    return;
  }
  printf("Allocation successful at address: %p\n", ptr);
  if (ufree(ptr) == UMEM_ERROR) {
    printf("Error: Freeing memory failed.\n");
    return;
  }
  printf("Memory freed successfully.\n");
  umemdump();
}

// Tests the Next Fit strategy's behavior in a cyclic environment where it must
// wrap around to find free space.
void test_next_fit_cyclic_allocation() {
  printf("Testing Next Fit cyclic allocation...\n");
  umeminit(4096, NEXT_FIT);
  void *ptr1 = umalloc(100);
  void *ptr2 = umalloc(200);
  printf("Allocated 100 and 200 bytes at addresses: %p, %p\n", ptr1, ptr2);
  ufree(ptr1);
  printf("Freed first allocation at address: %p\n", ptr1);
  void *ptr3 = umalloc(150);
  printf("Allocated 150 bytes, testing cyclic behavior at address: %p\n", ptr3);
  ufree(ptr2);
  ufree(ptr3);
  umemdump();
}

// Tests random size allocations in Next Fit to verify that it properly
// continues from the last point of allocation.
void test_next_fit_random_allocation() {
  printf("Testing Next Fit with random allocation...\n");
  umeminit(4096, NEXT_FIT);
  void *ptr1 = umalloc(123);
  void *ptr2 = umalloc(234);
  void *ptr3 = umalloc(345);
  printf("Allocated 123, 234, 345 bytes at addresses: %p, %p, %p\n", ptr1, ptr2,
         ptr3);
  ufree(ptr2);
  printf("Freed middle allocation at address: %p\n", ptr2);
  void *ptr4 = umalloc(222);
  printf("Allocated 222 bytes, testing random fit at address: %p\n", ptr4);
  ufree(ptr1);
  ufree(ptr3);
  ufree(ptr4);
  umemdump();
}

// Tests sequential fitting in Next Fit, emphasizing its process of continuing
// searches from the last allocation point.
void test_next_fit_sequential_fit() {
  printf("Testing Next Fit sequential fitting...\n");
  umeminit(4096, NEXT_FIT);
  void *ptr1 = umalloc(100);
  void *ptr2 = umalloc(200);
  printf("Allocated 100 and 200 bytes at addresses: %p, %p\n", ptr1, ptr2);
  ufree(ptr1);
  printf("Freed first allocation at address: %p\n", ptr1);
  void *ptr3 = umalloc(100);
  printf("Reallocated 100 bytes, testing sequential fitting at address: %p\n",
         ptr3);
  ufree(ptr2);
  ufree(ptr3);
  umemdump();
}

void test_random_first_fit() {
  printf("Running Random Allocations Test - First Fit...\n");

  umeminit(4096, FIRST_FIT); // Initialize memory with First Fit strategy
  for (int i = 0; i < 10; i++) {
    size_t size =
        (rand() % 100) + 1; // Random allocation size between 1 and 100
    void *ptr = umalloc(size);
    if (ptr) {
      printf("Allocated %zu bytes at address: %p\n", size, ptr);
      ufree(ptr);
      printf("Freed memory at address: %p\n", ptr);
    } else {
      printf("Allocation failed for size %zu\n", size);
    }
  }
  umemdump(); // Show memory state after test
}

// This function simulates random allocations and deallocations using the Best
// Fit strategy
void test_random_best_fit() {
  printf("Running Random Allocations Test for Best Fit...\n");

  const int memory_size = 4096;
  if (umeminit(memory_size, BEST_FIT) == UMEM_ERROR) {
    printf("Error: Memory initialization failed.\n");
    return;
  }

  // Array to keep track of allocated pointers for random deallocation
  void *pointers[100];
  int allocated_sizes[100];
  int ptr_count = 0;

  for (int i = 0; i < 100; i++) {
    int size = (rand() % 100) + 1; // Random size between 1 and 100
    void *ptr = umalloc(size);
    if (ptr) {
      printf("Allocated %d bytes at address: %p\n", size, ptr);
      pointers[ptr_count] = ptr;
      allocated_sizes[ptr_count++] = size;
    } else {
      printf("Allocation failed for size %d\n", size);
    }

    // Randomly free a block from those that have been allocated
    if (ptr_count > 0 &&
        (rand() % 4) == 0) { // Approximately 25% chance to free
      int index_to_free = rand() % ptr_count;
      printf("Freeing block at address: %p with size %d\n",
             pointers[index_to_free], allocated_sizes[index_to_free]);
      ufree(pointers[index_to_free]);
      pointers[index_to_free] =
          pointers[ptr_count - 1]; // Move last pointer to freed spot
      allocated_sizes[index_to_free] = allocated_sizes[ptr_count - 1];
      ptr_count--;
    }
  }

  // Free any remaining pointers
  for (int i = 0; i < ptr_count; i++) {
    printf("Cleaning up: freeing block at address: %p with size %d\n",
           pointers[i], allocated_sizes[i]);
    ufree(pointers[i]);
  }

  umemdump(); // Show final memory state
}

void test_random_worst_fit() {
  printf("Running Random Allocations Test - Worst Fit...\n");

  umeminit(4096, WORST_FIT);
  for (int i = 0; i < 10; i++) {
    size_t size = (rand() % 100) + 1;
    void *ptr = umalloc(size);
    if (ptr) {
      printf("Allocated %zu bytes at address: %p\n", size, ptr);
      ufree(ptr);
      printf("Freed memory at address: %p\n", ptr);
    } else {
      printf("Allocation failed for size %zu\n", size);
    }
  }
  umemdump();
}

void test_random_next_fit() {
  printf("Running Random Allocations Test - Next Fit...\n");

  umeminit(4096, NEXT_FIT);
  for (int i = 0; i < 10; i++) {
    size_t size = (rand() % 100) + 1;
    void *ptr = umalloc(size);
    if (ptr) {
      printf("Allocated %zu bytes at address: %p\n", size, ptr);
      ufree(ptr);
      printf("Freed memory at address: %p\n", ptr);
    } else {
      printf("Allocation failed for size %zu\n", size);
    }
  }
  umemdump();
}
