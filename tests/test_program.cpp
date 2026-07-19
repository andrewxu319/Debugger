#include <stdio.h>
#include <thread>
#include <chrono>

using namespace std::chrono_literals;

int main() {
    int counter{};
    while (true) {
        printf("%d\n", counter);
        std::this_thread::sleep_for(1s);
        counter++;
    }
}