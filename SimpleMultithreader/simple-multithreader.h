#include <iostream>
#include <pthread.h>
#include <vector>
#include <tuple>
#include <functional>
#include <chrono>
#include <algorithm>

// Wrapper struct for passing arguments to threads in 1D parallel_for
struct ThreadTask1D {
    int start, end;
    std::function<void(int)> lambda;
};

// Wrapper struct for passing arguments to threads in 2D parallel_for
struct ThreadTask2D {
    int start, end, low2, high2;
    std::function<void(int, int)> lambda;
};

/* Demonstration on how to pass lambda as parameter.
 * "&&" means r-value reference. You may read about it online.
 */
void demonstration(std::function<void()> &&lambda) {
    lambda();
}

/* 1D parallel_for Implementation */
void parallel_for(int low, int high, std::function<void(int)> &&lambda, int numThreads) {
    auto start_time = std::chrono::high_resolution_clock::now();

    // Compute chunk size
    int chunkSize = (high - low + numThreads - 1) / numThreads; // Ceiling division
    std::vector<pthread_t> threads(numThreads);
    std::vector<std::tuple<int, int, std::function<void(int)>>> tasks(numThreads);

    for (int t = 0; t < numThreads; ++t) {
        int start = low + t * chunkSize;
        int end = std::min(high, start + chunkSize);
        tasks[t] = std::make_tuple(start, end, lambda);
        pthread_create(&threads[t], nullptr, [](void* arg) -> void* {
            auto* task = static_cast<std::tuple<int, int, std::function<void(int)>>*>(arg);
            int start = std::get<0>(*task);
            int end = std::get<1>(*task);
            auto& lambda = std::get<2>(*task);
            for (int i = start; i < end; ++i) {
                lambda(i);
            }
            return nullptr;
        }, &tasks[t]);
    }

    for (pthread_t& thread : threads) {
        pthread_join(thread, nullptr);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end_time - start_time;
    std::cout << "1D parallel_for executed in " << duration.count() << " seconds.\n";
}

/* 2D parallel_for Implementation */
void parallel_for(int low1, int high1, int low2, int high2, std::function<void(int, int)> &&lambda, int numThreads) {
    auto start_time = std::chrono::high_resolution_clock::now();

    // Compute chunk size
    int chunkSize = (high1 - low1 + numThreads - 1) / numThreads; // Ceiling division
    std::vector<pthread_t> threads(numThreads);
    std::vector<std::tuple<int, int, int, int, std::function<void(int, int)>>> tasks(numThreads);

    for (int t = 0; t < numThreads; ++t) {
        int start = low1 + t * chunkSize;
        int end = std::min(high1, start + chunkSize);
        tasks[t] = std::make_tuple(start, end, low2, high2, lambda);
        pthread_create(&threads[t], nullptr, [](void* arg) -> void* {
            auto* task = static_cast<std::tuple<int, int, int, int, std::function<void(int, int)>>*>(arg);
            int start = std::get<0>(*task);
            int end = std::get<1>(*task);
            int low2 = std::get<2>(*task);
            int high2 = std::get<3>(*task);
            auto& lambda = std::get<4>(*task);
            for (int i = start; i < end; ++i) {
                for (int j = low2; j < high2; ++j) {
                    lambda(i, j);
                }
            }
            return nullptr;
        }, &tasks[t]);
    }

    for (pthread_t& thread : threads) {
        pthread_join(thread, nullptr);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end_time - start_time;
    std::cout << "2D parallel_for executed in " << duration.count() << " seconds.\n";
}

/* User-defined main function */
int user_main(int argc, char** argv);

/* Entry point */
int main(int argc, char** argv) {
    int x = 5, y = 1;

    // Sample lambda demonstration
    auto lambda1 = [x, &y]() {
        y = 5;
        std::cout << "====== Welcome to Assignment-" << y << " of the CSE231(A) ======\n";
    };
    demonstration(lambda1);

    int rc = user_main(argc, argv);

    auto lambda2 = []() {
        std::cout << "====== Hope you enjoyed CSE231(A) ======\n";
    };
    demonstration(lambda2);

    return rc;
}

#define main user_main
