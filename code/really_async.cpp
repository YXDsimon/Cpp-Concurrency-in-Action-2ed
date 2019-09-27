// 默认启动策略为std::launch::async的std::async
#include <iostream>
#include <future>
#include <type_traits>
#include <utility>

template<typename F, typename... Ts>
inline
std::future<std::invoke_result_t<F, Ts...>>
really_async(F&& f, Ts&&... args)
{
    return std::async(std::launch::async,
        std::forward<F>(f),
        std::forward<Ts>(args)...);
}

int f()
{
    return 1;
}

int main()
{
    std::future<int> ft = really_async(f);
    std::cout << ft.get();
}