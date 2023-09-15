#include <iostream>
#include "ThreadPool.h"
int main() {
    ThreadPool threadPool;
    threadPool.enqueue([] {
        std::cout << "hello " << std::endl;
    });
    auto future = threadPool.enqueue([](std::string str) { return str; }, "word ");
    std::cout << future.get() << std::endl;

    return 0;
}
