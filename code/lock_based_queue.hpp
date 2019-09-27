#include <memory>
#include <utility>
#include <mutex>
#include <condition_variable>
#include <queue>

template<typename T>
class thread_safe_queue {
    mutable std::mutex m;
    std::queue<std::shared_ptr<T>> q;
    std::condition_variable cv;
public:
    thread_safe_queue() {}
    thread_safe_queue(const thread_safe_queue& rhs)
    {
        std::lock_guard<std::mutex> l(rhs.m);
        q = rhs.q;
    }
    
    void push(T x)
    {
        std::shared_ptr<T> data(std::make_shared<T>(std::move(x)));
        std::lock_guard<std::mutex> l(m);
        q.push(data);
        cv.notify_one();
    }
    
    void wait_and_pop(T& x)
    {
        std::unique_lock<std::mutex> l(m);
        cv.wait(l, [this] { return !q.empty(); });
        x = std::move(*q.front());
        q.pop();
    }
    
    std::shared_ptr<T> wait_and_pop()
    {
        std::unique_lock<std::mutex> l(m);
        cv.wait(l, [this] { return !q.empty(); });
        std::shared_ptr<T> res = q.front();
        q.pop();
        return res;
    }
    
    bool try_pop(T& x)
    {
        std::lock_guard<std::mutex> l(m);
        if(q.empty()) return false;
        x = std::move(*q.front());
        q.pop();
        return true;
    }
    
    std::shared_ptr<T> try_pop()
    {
        std::lock_guard<std::mutex> l(m);
        if(q.empty()) return std::shared_ptr<T>();
        std::shared_ptr<T> res = q.front();
        q.pop();
        return res;
    }
    
    bool empty() const
    {
        std::lock_guard<std::mutex> l(m);
        return q.empty();
    }
};