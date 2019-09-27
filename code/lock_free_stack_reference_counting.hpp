#include <atomic>
#include <memory>

template<typename T>
class lock_free_stack {
    struct node;
    struct cntPtr {
        int exCnt; // 外部计数
        node* p;
    };
    struct node {
        std::shared_ptr<T> val;
        std::atomic<int> inCnt; // 内部计数
        cntPtr next;
        node(const T& x): val(std::make_shared<T>(x)), inCnt(0) {}
    };
    std::atomic<cntPtr> head;
    
    void increaseHeadCount(cntPtr& oldCnt)
    {
        cntPtr newCnt;
        do {
            newCnt = oldCnt;
            ++newCnt.exCnt; // 访问head时递增外部计数，表示该节点正被使用
        } while(!head.compare_exchange_strong(oldCnt, newCnt));
        oldCnt.exCnt = newCnt.exCnt;
    }
public:
    void push(const T& x)
    {
        cntPtr newNode;
        newNode.p = new node(x);
        newNode.exCnt = 1;
        newNode.p->next = head.load();
        while(!head.compare_exchange_weak(newNode.p->next, newNode));
    }
    std::shared_ptr<T> pop()
    {
        cntPtr oldHead = head.load();
        for(;;)
        {
            increaseHeadCount(oldHead); // 外部计数递增表示该节点正被使用
            node* const p = oldHead.p; // 因此可以安全地访问
            if(!p) return std::shared_ptr<T>();
            if(head.compare_exchange_strong(oldHead, p->next))
            {
                std::shared_ptr<T> res;
                res.swap(p->val);
                // 再将外部计数减2加到内部计数，减2是因为，
                // 节点被删除减1，该线程无法再次访问此节点再减1
                const int increaseCount = oldHead.exCnt - 2;
                if(p->inCnt.fetch_add(increaseCount) == -increaseCount)
                { // 如果内部计数加上increaseCount为0（相加前为-increaseCount）
                    delete p;
                }
                return res;
            }
            else if(p->inCnt.fetch_sub(1) == 1) delete p;
        }
    }
    ~lock_free_stack()
    {
        while(pop());
    }
};