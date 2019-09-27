// 可选择析构方式为join还是detach的thread_guard
#include <iostream>
#include <thread>
#include <utility>

class thread_guard {
public:
    enum class DtorAction { join, detach };
    thread_guard(std::thread&& t, DtorAction a) : action(a), t(std::move(t)) {}
    ~thread_guard()
    {
        if(t.joinable())
        {
            if(action == DtorAction::join) t.join();
            else t.detach();
        }
    }
    thread_guard(thread_guard&&) = default;
    thread_guard& operator=(thread_guard&&) = default;
    std::thread& get() { return t; }
private:
    DtorAction action;
    std::thread t;
};

void f()
{
    std::cout << "OK";
}

int main()
{
    thread_guard g(std::thread{ f }, thread_guard::DtorAction::join);
}
