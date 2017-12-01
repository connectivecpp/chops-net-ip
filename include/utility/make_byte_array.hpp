/** @file
 *
 * @ingroup utility_module
 *
 * @brief A utility function that constructs a std::array of std::byte objects.
 *
 * A @c std::byte is defined as an enum and there are no implicit conversions from int
 * to an enum class. Instead of writing:
 *
 * @code
 * std::array<std::byte, 5> arr = 
 *   { std::byte{0x36}, std::byte{0xd0}, std::byte{0x42}, std::byte{0xbe}, std::byte{0xef} };
 * @endcode
 * it is easier (specially for larger arrays) to write:
 * @code
 * auto arr = make_byte_array(0x36, 0xd0, 0x42, 0xbe, 0xef);
 * @endcode
 *
 * This code is taken from an example by Blitz Rakete from an answer on Stackoverflow 
 * (Blitz is user Rakete1111 on SO).
 * 
 * @authors Blitz Rakete, Cliff Green
 * @date 2017
 * @copyright Cliff Green, MIT License
 * 
 */

#ifndef MAKE_BYTE_ARRAY_HPP_INCLUDED
#define MAKE_BYTE_ARRAY_HPP_INCLUDED

#include <array>
#include <cstddef> // std::byte

namespace chops {

template<typename... Ts>
std::array<std::byte, sizeof...(Ts)> make_byte_array(Ts&&... args) noexcept {
    return { std::byte{std::forward<Ts>(args)}... };
}

} // end namespace

#endif

