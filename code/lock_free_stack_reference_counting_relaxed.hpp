#include <atomic>
#include <memory>

template<typename T>
class lock_free_stack {
    struct node;
    struct cntPtr {
        int exCnt;
        node* p;
    };
    struct node {
        std::shared_ptr<T> val;
        std::atomic<int> inCnt;
        cntPtr next;
        node(const T& x): val(std::make_shared<T>(x)), inCnt(0) {}
    };
    std::atomic<cntPtr> head;

    void increaseHeadCount(cntPtr& oldCnt)
    {
        cntPtr newCnt;
        do { // 比较失败不改变当前值，并可以继续循环，因此可以选择relaxed
            newCnt = oldCnt;
            ++newCnt.exCnt;
        } while (!head.compare_exchange_strong(oldCnt, newCnt,
            std::memory_order_acquire, std::memory_order_relaxed));
        oldCnt.exCnt = newCnt.exCnt;
    }
public:
    void push(const T& x)
    {
        cntPtr newNode;
        newNode.p = new node(x);
        newNode.exCnt = 1;
        // 下面比较中release保证之前的语句都先执行，因此load可以使用relaxed
        newNode.p->next = head.load(std::memory_order_relaxed);
        // 比较失败不改变当前值，并可以继续循环，因此可以选择relaxed
        while (!head.compare_exchange_weak(newNode.p->next, newNode,
            std::memory_order_release, std::memory_order_relaxed));
    }
    std::shared_ptr<T> pop()
    {
        cntPtr oldHead = head.load(std::memory_order_relaxed);
        for (;;)
        {
            increaseHeadCount(oldHead); // acquire
            node* const p = oldHead.p;
            if (!p) return std::shared_ptr<T>();
            if (head.compare_exchange_strong(oldHead, p->next, std::memory_order_relaxed))
            {
                std::shared_ptr<T> res;
                res.swap(p->val);
                const int increaseCount = oldHead.exCnt - 2;
                // swap要先于delete，因此使用release
                if (p->inCnt.fetch_add(increaseCount, std::memory_order_release)
                    == -increaseCount)
                {
                    delete p;
                }
                return res;
            }
            else if (p->inCnt.fetch_add(-1, std::memory_order_relaxed) == 1)
            {
                p->inCnt.load(std::memory_order_acquire); // 只是用acquire来同步
                // acquire保证delete在之后执行
                delete p;
            }
        }
    }
    ~lock_free_stack()
    {
        while (pop());
    }
};