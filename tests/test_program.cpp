#include <stdio.h>
#include <thread>
#include <chrono>

using namespace std::chrono_literals;

namespace test_namespace {
    void count(int& counter) {
        printf("%d\n", counter);
        std::this_thread::sleep_for(1s);
        counter++;
    }
}

int main() {
    int counter{};
    while (true) {
        test_namespace::count(counter);
    }
}