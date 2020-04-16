#include <exception>
#include <memory>
#include <utility>
#include <mutex>
#include <stack>

struct emptyStack : std::exception
{
    const char* what() const noexcept
    {
        return "empty stack!";
    }
};

template<typename T>
class thread_safe_stack {
    std::stack<T> s;
    mutable std::mutex m;
public:
    thread_safe_stack() : s(std::stack<T>()) {}
    
    thread_safe_stack(const thread_safe_stack& rhs)
    {
        std::lock_guard<std::mutex> l(rhs.m);
        s = rhs.s;
    }
    
    thread_safe_stack& operator=(const thread_safe_stack&) = delete;
    
    void push(T n)
    {
        std::lock_guard<std::mutex> l(m);
        s.push(std::move(n));
    }
    
    std::shared_ptr<T> pop() // 返回一个指向栈顶元素的指针
    {
        std::lock_guard<std::mutex> l(m);
        if (s.empty()) throw emptyStack();
        const std::shared_ptr<T> res(std::make_shared<T>(std::move(s.top())));
        s.pop();
        return res;
    }
    
    void pop(T& n) // 传引用获取结果
    {
        std::lock_guard<std::mutex> l(m);
        if (s.empty()) throw emptyStack();
        n = std::move(s.top());
        s.pop();
    }
    
    bool empty() const
    {
        std::lock_guard<std::mutex> l(m);
        return s.empty();
    }
};