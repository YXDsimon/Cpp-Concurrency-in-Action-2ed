#include <memory>
#include <utility>
#include <mutex>

template<typename T>
class thread_safe_list {
    struct node {
        std::mutex m;
        std::shared_ptr<T> data;
        std::unique_ptr<node> next;
        node() : next() {}
        node(const T& x) : data(std::make_shared<T>(x)) {}
    };
    node head;
public:
    thread_safe_list() {}
    ~thread_safe_list() { remove_if([](const node&) { return true; }); }
    thread_safe_list(const thread_safe_list&) = delete;
    thread_safe_list& operator=(const thread_safe_list&) = delete;
    void push_front(const T& x)
    {
        std::unique_ptr<node> newNode(new node(x));
        std::lock_guard<std::mutex> l(head.m);
        newNode->next = std::move(head.next);
        head.next = std::move(newNode);
    }
    
    template<typename F>
    void for_each(F f)
    {
        node* cur = &head;
        std::unique_lock<std::mutex> l(head.m);
        while (node* const next = cur->next.get())
        {
            std::unique_lock<std::mutex> nextLock(next->m);
            l.unlock(); // 锁住了下一节点，因此可以释放上一节点的锁
            f(*next->data);
            cur = next; // 当前节点指向下一节点
            l = std::move(nextLock); // 转交下一节点锁的所有权，循环上述过程
        }
    }
    
    template<typename F>
    std::shared_ptr<T> find_first_if(F f)
    {
        node* cur = &head;
        std::unique_lock<std::mutex> l(head.m);
        while (node* const next = cur->next.get())
        {
            std::unique_lock<std::mutex> nextLock(next->m);
            l.unlock();
            if (f(*next->data)) return next->data; // f返回true时则返回目标值，无需继续查找
            cur = next;
            l = std::move(nextLock);
        }
        return std::shared_ptr<T>();
    }
    
    template<typename F>
    void remove_if(F f)
    {
        node* cur = &head;
        std::unique_lock<std::mutex> l(head.m);
        while (node* const next = cur->next.get())
        {
            std::unique_lock<std::mutex> nextLock(next->m);
            if (f(*next->data))
            { // f为true时则移除下一节点
                std::unique_ptr<node> oldNext = std::move(cur->next);
                cur->next = std::move(next->next); // 下一节点设为下下节点
                nextLock.unlock();
            }
            else
            { // 否则继续转至下一节点
                l.unlock();
                cur = next;
                l = std::move(nextLock);
            }
        }
    }
};