/** @file
 *
 * @ingroup utility_module
 *
 * @brief Repeat code N times, since the C++ standard range-for does not make it
 * easy to repeat a loop N times. The lambda or function object can access the iteration
 * count, if desired.
 *
 * This code is copied from Vittorio Romeo's blog at https://vittorioromeo.info/. It is 
 * the same as his, including the noexcept propagation, except that it has been 
 * simplified to not be templatized on the count type.
 * 
 * 
 * @authors Vittorio Romeo, Cliff Green
 * @date 2017
 * 
 */

#ifndef REPEAT_INCLUDED_H
#define REPEAT_INCLUDED_H

#include <type_traits>

namespace chops {

template <typename F>
constexpr auto repeat(int n, F&& f)
    noexcept(noexcept(f(n)))
    -> std::enable_if_t<std::is_invocable_v<F&&, int>>
{
    for(int i = 0; i < n; ++i)
    {
        f(i);
    }
}

template <typename F>
constexpr auto repeat(int n, F&& f)
    noexcept(noexcept(f()))
    -> std::enable_if_t<!std::is_invocable_v<F&&, int>>
{
    for(int i = 0; i < n; ++i)
    {
        f();
    }
}

} // end namespace

#endif

