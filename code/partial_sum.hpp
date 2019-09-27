#include <iterator>
#include <utility>

template<class InputIt, class OutputIt, class BinaryOperation>
OutputIt partial_sum(InputIt first, InputIt last, OutputIt d_first, BinaryOperation op)
{
    if(first == last) return d_first;
    typename std::iterator_traits<InputIt>::value_type sum = *first;
    *d_first = sum;
    while(++first != last)
    {
        sum = op(std::move(sum), *first);
        *++d_first = sum;
    }
    return ++d_first;
}