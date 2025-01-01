#include <atomic>
#include <iostream>
#include <linux/futex.h>
#include <shared_mutex>
#include <sys/syscall.h>
#include <thread>
#include <unistd.h>

class mysem {
public:
    mysem(uint32_t init_value) : _counter(init_value){};
    void acquire() {
        bool swapped = false;
        while (not swapped) {
            auto old = _counter.load();
            if (old > 0) {
                swapped = _counter.compare_exchange_strong(old, old - 1);
            }
        }
        _counter--;
    };
    void release() {
        _counter++;
    };

private:
    std::atomic<uint32_t> _counter;
};

class mysem_hybrid {
public:
    explicit mysem_hybrid(uint32_t init_value) : _counter(init_value){};
    void acquire() {
        bool swapped = false;
        int i = 100;
        while (not swapped) {
            auto old = _counter.load();
            if (old > 0) {
                swapped = _counter.compare_exchange_strong(old, old - 1);
            }
            i--;
            if (i == 0) {
                auto *counter_ptr = reinterpret_cast<uint32_t *>(&_counter);
                syscall(SYS_futex, counter_ptr, FUTEX_WAIT, 0);
            }
        }
        _counter--;
    };
    void release() {
        _counter++;
        auto *counter_ptr = reinterpret_cast<uint32_t *>(&_counter);
        syscall(SYS_futex, counter_ptr, FUTEX_WAKE, 1);
    };

private:
    std::atomic<uint32_t> _counter;
};

int main(int argc, char **argv) {
    mysem_hybrid s(2);
    std::thread t1([&]() {
        s.acquire();
        std::cout << 1;
        for (int i = 0; i < 1000000; i++) {}
        std::cout << 1;
        s.release();
    });
    std::thread t2([&]() {
        s.acquire();
        std::cout << 2;
        for (int i = 0; i < 1000000; i++) {}
        std::cout << 2;
        s.release();
    });
    t1.join();
    t2.join();
    std::cout << std::endl;
}
