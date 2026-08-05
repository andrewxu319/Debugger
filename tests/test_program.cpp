#include <stdio.h>
#include <thread>
#include <chrono>

using namespace std::chrono_literals;

namespace namespace_0 {
    void count(int& counter) {
        printf("namespace_0::count, %d\n", counter);
        std::this_thread::sleep_for(0.5s);
        counter++;
        counter++;
        counter++;
        counter++;
        printf("ok\n");
    }
}

// namespace namespace_1 {
//     void count(int& counter) {
//         printf("namespace_1::count, %d\n", -counter);
//         std::this_thread::sleep_for(0.5s);
//         counter++;
//     }
// }

int main() {
    int counter{};
    while (true) {
        namespace_0::count(counter);
        printf("hi\n");
        // namespace_1::count(counter);
    }
}