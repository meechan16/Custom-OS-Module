### Assignment 5 Summary

Aaman Sheikh
Ansh Varma

Group-74

This C++ program demonstrates the use of multithreading to parallelize computations using the POSIX threads (pthreads) library. It provides two main functionalities: a 1D parallel loop and a 2D parallel loop, both of which execute a given lambda function concurrently across multiple threads.

### Key Components:

1. **Lambda Demonstration**:
- The demonstration function shows how to pass and execute a lambda function. It takes an r-value reference to a lambda and executes it.

2. **1D Parallel Loop (parallel_for)**:
- This function divides a range [low, high) into chunks, each processed by a separate thread.
- It uses ceiling division to ensure all elements are covered, even if the range size isn't perfectly divisible by the number of threads.
- Each thread executes the provided lambda function on its assigned range.

3. **2D Parallel Loop (parallel_for)**:
- Similar to the 1D version, but operates over a 2D range defined by [low1, high1) and [low2, high2).
- Each thread processes a sub-range in the first dimension and iterates fully over the second dimension for each element in its sub-range.

4. **Thread Management**:
- Threads are created using pthread_create and joined using pthread_join.
- Tasks for each thread are stored in a vector of tuples, which hold the start and end indices and the lambda function.

5. **Performance Measurement**:
- The execution time of the parallel loops is measured using std::chrono and printed to the console.

6. **Main Function**:
- The main function is redefined as user_main to allow for custom user-defined logic.
- It demonstrates the use of lambda functions and calls the user_main function, which is intended for user-specific code.

### Usage:
- The program is structured to allow users to define their own user_main function, where they can utilize the parallel loop functionalities.
- The sample lambdas in the main function demonstrate basic usage and output messages to the console.

Github Link: https://github.com/AamanPrime/OSAssignment5
