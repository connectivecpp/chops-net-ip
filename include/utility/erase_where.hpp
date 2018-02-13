/** @file
 *
 * @ingroup utility_module
 *
 * @brief Convenience functions for the erase-remove idiom in C++.
 *
 * It's a common error to forget to erase an element from a container after
 * calling @c remove. This wraps the two calls together.
 *
 * @note Thanks goes to Richard Hodges. Most of this code is copied directly 
 * from a post of his on StackOverflow.
 *
 * @authors Richard Hodges, Cliff Green
 * @date 2018
 * @copyright Cliff Green, MIT License
 * 
 */

#ifndef ERASE_WHERE_HPP_INCLUDED
#define ERASE_WHERE_HPP_INCLUDED

#include <algorithm>
#include <utility> // std::forward

namespace chops {

template <typename C>
auto erase_where(C& c, const typename C::value_type& val) {
  return c.erase(std::remove(c.begin(), c.end(), val), c.end());
}

template<typename C, typename F>
auto erase_where_if(C& c, F&& f) {
  return c.erase(std::remove_if(c.begin(), c.end(), std::forward<F>(f)),
    c.end());    
}

} // end namespace

#endif

