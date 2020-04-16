#include <thread>
#include <vector>

class threads_guard {
    std::vector<std::thread>& threads;
public:
    explicit threads_guard(std::vector<std::thread>& t): threads(t) {}
    ~threads_guard()
    {
        for (auto& x : threads)
        {
            if (x.joinable()) x.join();
        }
    }
};