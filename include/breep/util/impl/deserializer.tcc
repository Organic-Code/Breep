///////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                               //
// Copyright 2017-2018 Lucas Lazare.                                                             //
// This file is part of Breep project which is released under the                                //
// European Union Public License v1.1. If a copy of the EUPL was                                 //
// not distributed with this software, you can obtain one at :                                   //
// https://joinup.ec.europa.eu/community/eupl/og_page/european-union-public-licence-eupl-v11     //
//                                                                                               //
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @file serializer.cpp
 * @author Lucas Lazare
 * @since 1.0.0
 */

#include <cmath>
#include <limits>
#include <tuple>
#include <utility>
#include <chrono>
#include <cassert>

#include "breep/util/tuple_iter.hpp"
#include "../deserializer.hpp" // Allows my IDE to work

namespace breep {

namespace detail {
	// The code from the following method was adapted from the public domain
	template <typename FloatType, typename InputType, unsigned int ExponentBits, unsigned int MantissaBits>
	constexpr FloatType fromIEEE(InputType value) {

		static_assert(std::numeric_limits<uint_fast8_t>::max() >= std::numeric_limits<InputType>::digits
				, "[unlikely]: will not able to store std::numeric_limits<InputType>::digitd in uint_fast8_t");

		FloatType result;
		InputType shift, bias;


		InputType exponentMax(0);
		for (uint_fast8_t i = 0 ; i < ExponentBits ; ++i) {
			exponentMax = (exponentMax << 1) + 1;
		}

		if ((value & (exponentMax << MantissaBits)) == (exponentMax << MantissaBits)) {
			InputType mantissaMax(0);
			for (uint_fast8_t i = 0 ; i < MantissaBits; ++i) {
				mantissaMax = (mantissaMax << 1) + 1;
			}

			if (value & mantissaMax) {
				return std::numeric_limits<FloatType>::quiet_NaN();
			} else {
				if (value & (InputType(1) << (MantissaBits + ExponentBits))) {
					return -std::numeric_limits<FloatType>::infinity();
				} else {
					return std::numeric_limits<FloatType>::infinity();
				}
			}
		}

		if (value == 0) {
			return FloatType(0.);
		} else if (value == (InputType(1) << (ExponentBits + MantissaBits))) {
			return FloatType(-0.);
		}

		result = FloatType((value & ((InputType(1) << MantissaBits) - 1)));
		result /= FloatType((InputType(1) << MantissaBits));
		result += FloatType(1.);

		bias = static_cast<InputType>((InputType(1) << (ExponentBits - 1)) - 1);
		shift = static_cast<InputType>(((value >> MantissaBits) & ((InputType(1) << ExponentBits) - 1)) - bias);

		if (shift > 0) {
            for (uint_fast8_t shift_val = std::numeric_limits<InputType>::digits ; --shift_val ;) {
                while (shift >= shift_val) {
                    result *= FloatType(InputType(1) << shift_val);
                    shift -= static_cast<InputType>(shift_val);
                }
            }
		} else {
            for (uint_fast8_t shift_val = std::numeric_limits<InputType>::digits ; --shift_val ;) {
                while (shift <= -shift_val) {
                    result /= FloatType(InputType(1) << shift_val);
                    shift += static_cast<InputType>(shift_val);
                }
            }

		}

		result *= FloatType((value >> (ExponentBits + MantissaBits)) & 1 ? -1.0: 1.0);

		return result;
	}
}

inline void read_size(deserializer& d, uint64_t& size) {
	uint8_t uint8(0);
	uint_fast8_t oct_to_read(0);
	d >> oct_to_read;
	while (oct_to_read--) {
		d >> uint8;
		size |= (static_cast<uint64_t>(uint8) << static_cast<uint64_t>(oct_to_read * 8));
	}
}

namespace detail { // namespace breep::detail
	
	// deserializing fundamental types
	deserializer& right_shift_op_impl(deserializer&, bool&);
	deserializer& right_shift_op_impl(deserializer&, char&);
	deserializer& right_shift_op_impl(deserializer&, uint16_t&);
	deserializer& right_shift_op_impl(deserializer&, uint32_t&);
	deserializer& right_shift_op_impl(deserializer&, uint64_t&);
	deserializer& right_shift_op_impl(deserializer&, int8_t&);
	deserializer& right_shift_op_impl(deserializer&, int16_t&);
	deserializer& right_shift_op_impl(deserializer&, int32_t&);
	deserializer& right_shift_op_impl(deserializer&, int64_t&);
	deserializer& right_shift_op_impl(deserializer&, float&);
	deserializer& right_shift_op_impl(deserializer&, double&);

	template <typename T>
	std::enable_if_t<breep::type_traits<T>::is_enum_class, deserializer&>
	right_shift_op_impl(deserializer&, T&);

	template <typename T>
	std::enable_if_t<breep::type_traits<T>::is_enum_plain, deserializer&>
    right_shift_op_impl(deserializer&, T&);

	// generic method deserializing containerd that support .push_back(value_type)
	template <typename PushableContainer>
	std::enable_if_t<!breep::type_traits<PushableContainer>::is_enum, deserializer&>
	right_shift_op_impl(deserializer&, PushableContainer&);

	// STL containers
	template <typename T>
	deserializer& right_shift_op_impl(deserializer&, std::forward_list<T>&);

	template <typename T>
	deserializer& right_shift_op_impl(deserializer&, std::vector<T>&);

	deserializer& right_shift_op_impl(deserializer&, std::vector<bool>&);

	template <typename T, std::size_t N>
	deserializer& right_shift_op_impl(deserializer&, std::array<T, N>&);

	template <typename T>
	deserializer& right_shift_op_impl(deserializer&, std::deque<T>&);

	template <typename T>
	deserializer& right_shift_op_impl(deserializer&, std::list<T>&);

	template <typename T, typename U, typename V, typename W>
	deserializer& right_shift_op_impl(deserializer&, std::map<T,U,V,W>&);

	template <typename T, typename U, typename V, typename W>
	deserializer& right_shift_op_impl(deserializer&, std::multimap<T,U,V,W>&);

	template <typename T, typename U, typename V, typename W, typename X>
	deserializer& right_shift_op_impl(deserializer&, std::unordered_map<T,U,V,W,X>&);

	template <typename T, typename U, typename V, typename W, typename X>
	deserializer& right_shift_op_impl(deserializer&, std::unordered_multimap<T,U,V,W,X>&);

	template <typename T, typename U, typename V>
	deserializer& right_shift_op_impl(deserializer&, std::set<T,U,V>&);


	template <typename T, typename U, typename V>
	deserializer& right_shift_op_impl(deserializer&, std::multiset<T,U,V>&);


	template <typename T, typename U, typename V, typename W>
	deserializer& right_shift_op_impl(deserializer&, std::unordered_set<T,U,V,W>&);


	template <typename T, typename U, typename V, typename W>
	deserializer& right_shift_op_impl(deserializer&, std::unordered_multiset<T,U,V,W>&);

	template <typename T, typename U>
	deserializer& right_shift_op_impl(deserializer&, std::stack<T,U>&);

	template <typename T, typename U>
	deserializer& right_shift_op_impl(deserializer&, std::queue<T,U>&);

	template <typename T, typename U, typename V>
	deserializer& right_shift_op_impl(deserializer&, std::priority_queue<T,U,V>&);

	// deserializing std::pair
	template <typename T, typename U>
	deserializer& right_shift_op_impl(deserializer&, std::pair<T,U>&);

	// deserializing std::tuple
	template <typename... T>
	deserializer& right_shift_op_impl(deserializer&, std::tuple<T...>&);

	// deserializing durations
	template <typename T, typename U>
	deserializer& right_shift_op_impl(deserializer&, std::chrono::duration<T,U>&);

	inline deserializer& right_shift_op_impl(deserializer& d, uint8_t& val) {
		d.m_is.get(val);
		return d;
	}

	inline deserializer& right_shift_op_impl(deserializer& d, uint16_t& val) {
		uint8_t uint8(0);
		right_shift_op_impl(d, uint8);
		val = static_cast<uint16_t>(static_cast<uint16_t>(uint8) << 8);
		right_shift_op_impl(d, uint8);
		val = static_cast<uint16_t>(val | uint8);
		return d;
	}

	inline deserializer& right_shift_op_impl(deserializer& d, uint32_t& val) {
		uint16_t uint16(0);
		right_shift_op_impl(d, uint16);
        val = uint16 << 16u;
		right_shift_op_impl(d, uint16);
		val |= uint16;
		return d;
	}

	inline deserializer& right_shift_op_impl(deserializer& d, uint64_t& val) {
		uint32_t uint32(0);
		right_shift_op_impl(d, uint32);
		val = static_cast<uint64_t>(uint32) << 32;
		right_shift_op_impl(d, uint32);
		val |= uint32;
		return d;
	}

	inline deserializer& right_shift_op_impl(deserializer& d, int8_t& val) {
		uint8_t uint8(0);
		right_shift_op_impl(d, uint8);
		val = static_cast<int8_t>(uint8);
		return d;
	}

	inline deserializer& right_shift_op_impl(deserializer& d, int16_t& val) {
		uint16_t uint16(0);
		right_shift_op_impl(d, uint16);
		val = static_cast<int16_t>(uint16);
		return d;
	}

	inline deserializer& right_shift_op_impl(deserializer& d, int32_t& val) {
		uint32_t uint32(0);
		right_shift_op_impl(d, uint32);
		val = static_cast<int32_t>(uint32);
		return d;
	}

	inline deserializer& right_shift_op_impl(deserializer& d, int64_t& val) {
		uint64_t uint64(0);
		right_shift_op_impl(d, uint64);
		val = static_cast<int64_t>(uint64);
		return d;
	}

	inline deserializer& right_shift_op_impl(deserializer& d, bool& val) {
		uint8_t c;
		right_shift_op_impl(d, c);
		val = (c == static_cast<uint8_t>('1'));
		return d;
	}

	inline deserializer& right_shift_op_impl(deserializer& d, char& val) {
		if (std::numeric_limits<char>::min() < 0) {
			unsigned char unsigned_value(0);
			right_shift_op_impl(d, unsigned_value);
			val = unsigned_value;
		} else {
			signed char signed_value(0);
			right_shift_op_impl(d, signed_value);
			val = signed_value;
		}
		return d;
	}

	inline deserializer& right_shift_op_impl(deserializer& d, float& val) {
		uint32_t uint32(0);
		right_shift_op_impl(d, uint32);
		val = detail::fromIEEE<float, int32_t, 8, 23>(uint32);
		return d;
	}

	inline deserializer& right_shift_op_impl(deserializer& d, double& val) {
		uint64_t uint64(0);
		right_shift_op_impl(d, uint64);
		val = detail::fromIEEE<double, int64_t, 11, 52>(uint64);
		return d;
	}

	template <typename T>
	std::enable_if_t<breep::type_traits<T>::is_enum_class, deserializer&>
	right_shift_op_impl(deserializer& d, T& val) {
		std::underlying_type_t<T> value;
		d >> value;
		val = static_cast<T>(value);
		return d;
	}

	template <typename T>
	std::enable_if_t<breep::type_traits<T>::is_enum_plain, deserializer&>
	right_shift_op_impl(deserializer& d, T& val) {
		int64_t value;
		d >> value;
		val = static_cast<T>(value);
		return d;
	}

	template<typename PushableContainer>
	std::enable_if_t<!breep::type_traits<PushableContainer>::is_enum, deserializer&>
	right_shift_op_impl(deserializer& d, PushableContainer& val) {
		uint64_t size = 0;
		read_size(d, size);
		while (size--) {
			typename PushableContainer::value_type value;
			d >> value;
			val.push_back(std::move(value));
		}
		return d;
	}

	template<typename T>
	deserializer& right_shift_op_impl(deserializer& d, std::forward_list<T>& val) {
		std::vector<T> tmp;
		d >> tmp;
		for (auto it = tmp.rbegin(), end = tmp.rend() ; it != end ; ++it) {
			val.emplace_front(std::move(*it));
		}
		return d;
	}

	template <typename T>
	deserializer& right_shift_op_impl(deserializer& d, std::vector<T>& vector) {
		uint64_t size = 0;
		read_size(d, size);
		vector.reserve(size + vector.size());
		while(size--) {
			T value;
			d >> value;
			vector.emplace_back(std::move(value));
		}
		return d;
	}

    inline deserializer& right_shift_op_impl(deserializer& d, std::vector<bool>& vector) {
        uint8_t mask;
        uint64_t size = 0;
        read_size(d, size);
        vector.reserve(size + vector.size());
        for (uint64_t i = 0 ; i < size ; i += std::numeric_limits<uint8_t>::digits) {
            d >> mask;
            for (uint8_t j = 0 ; j < std::numeric_limits<uint8_t>::digits && i+j < size ; ++j) {
                vector.push_back((mask & (1 << j)) != 0);
            }
        }
        return d;
    }

	template <typename T, std::size_t N>
	deserializer& right_shift_op_impl(deserializer& d, std::array<T, N>& array) {
		uint64_t size = 0;
		read_size(d, size);
		for (uint_fast64_t i = 0 ; i < size ; ++i) {
			d >> array[i];
		}
		return d;
	}

	template <typename T>
	deserializer& right_shift_op_impl(deserializer& d, std::deque<T>& deque) {
		uint64_t size = 0;
		read_size(d, size);
		while (size--) {
			T value;
			d >> value;
			deque.emplace_back(std::move(value));
		}
		return d;
	}

	template <typename T>
	deserializer& right_shift_op_impl(deserializer& d, std::list<T>& list) {
		uint64_t size = 0;
		read_size(d, size);
		while (size--) {
			T value;
			d >> value;
			list.emplace_back(std::move(value));
		}
		return d;
	}

	template <typename T, typename U, typename V, typename W>
	deserializer& right_shift_op_impl(deserializer& d, std::map<T,U,V,W>& map) {
		uint64_t size = 0;
		read_size(d, size);
		while (size--) {
			T key;
			U value;
			d >> key >> value;
			map.emplace_hint(map.cend(), std::make_pair(std::move(key), std::move(value)));
		}
		return d;
	}

	template <typename T, typename U, typename V, typename W>
	deserializer& right_shift_op_impl(deserializer& d, std::multimap<T,U,V,W>& map) {
		uint64_t size = 0;
		read_size(d, size);
		while (size--) {
			T key;
			U value;
			d >> key >> value;
			map.emplace_hint(map.cend(), std::make_pair(std::move(key), std::move(value)));
		}
		return d;
	}


	template <typename T, typename U, typename V, typename W, typename X>
	deserializer& right_shift_op_impl(deserializer& d, std::unordered_map<T,U,V,W,X>& map) {
		uint64_t size = 0;
		read_size(d, size);
		while (size--) {
			T key;
			U value;
			d >> key >> value;
			map.emplace_hint(map.cend(), std::make_pair(std::move(key), std::move(value)));
		}
		return d;
	}


	template <typename T, typename U, typename V, typename W, typename X>
	deserializer& right_shift_op_impl(deserializer& d, std::unordered_multimap<T,U,V,W,X>& map) {
		uint64_t size = 0;
		read_size(d, size);
		while (size--) {
			T key;
			U value;
			d >> key >> value;
			map.emplace_hint(map.cend(), std::make_pair(std::move(key), std::move(value)));
		}
		return d;
	}

	template <typename T, typename U, typename V>
	deserializer& right_shift_op_impl(deserializer& d, std::set<T,U,V>& set) {
		uint64_t size = 0;
		read_size(d, size);
		while (size--) {
			T value;
			d >> value;
			set.emplace_hint(set.cend(), std::move(value));
		}
		return d;
	}

	template <typename T, typename U, typename V>
	deserializer& right_shift_op_impl(deserializer& d, std::multiset<T,U,V>& multiset) {
		uint64_t size = 0;
		read_size(d, size);
		while (size--) {
			T value;
			d >> value;
			multiset.emplace_hint(multiset.cend(), std::move(value));
		}
		return d;
	}

	template <typename T, typename U, typename V, typename W>
	deserializer& right_shift_op_impl(deserializer& d, std::unordered_set<T,U,V,W>& unordered_set) {
		uint64_t size = 0;
		read_size(d, size);
		while (size--) {
			T value;
			d >> value;
			unordered_set.emplace_hint(unordered_set.cend(), std::move(value));
		}
		return d;
	}

	template <typename T, typename U, typename V, typename W>
	deserializer& right_shift_op_impl(deserializer& d, std::unordered_multiset<T,U,V,W>& unordered_multiset){
		uint64_t size = 0;
		read_size(d, size);
		while (size--) {
			T value;
			d >> value;
			unordered_multiset.emplace_hint(unordered_multiset.cend(), std::move(value));
		}
		return d;
	}

	template <typename T, typename U>
	deserializer& right_shift_op_impl(deserializer& d, std::stack<T,U>& stack) {
		std::vector<T> tmp;
		d >> tmp;
		for (auto it = tmp.rbegin(), end = tmp.rend() ; it != end ; ++it) {
			stack.emplace(std::move(*it));
		}
		return d;
	}

	template <typename T, typename U>
	deserializer& right_shift_op_impl(deserializer& d, std::queue<T,U>& queue) {
		uint64_t size = 0;
		read_size(d, size);
		while(size--) {
			T value;
			d >> value;
			queue.emplace(std::move(value));
		}
		return d;
	}

	template <typename T, typename U, typename V>
	deserializer& right_shift_op_impl(deserializer& d, std::priority_queue<T,U,V>& priority_queue) {
		uint64_t size = 0;
		read_size(d, size);
		while(size--) {
			T value;
			d >> value;
			priority_queue.emplace(std::move(value));
		}
		return d;
	}

	template<typename T, typename U>
	deserializer& right_shift_op_impl(deserializer& d, std::pair<T, U>& val) {
		d >> val.first >> val.second;
		return d;
	}

	template<typename... T>
	deserializer& right_shift_op_impl(deserializer& d, std::tuple<T...>& values) {
		for_each(values, [&d](auto&& val) -> void {
			d >> val;
		});
		return d;
	}

	template<typename T, typename U>
	deserializer& right_shift_op_impl(deserializer& d, std::chrono::duration<T, U>& val) {
		uint64_t uint64;
		right_shift_op_impl(d, uint64);
		val = std::chrono::duration<T,U>(std::chrono::nanoseconds(uint64));
		return d;
	}
	
} // namespace detail

	template <typename T>
	std::enable_if_t<!type_traits<T>::is_any_ptr, deserializer&>
	operator>>(deserializer& d, T&& value) {
		return detail::right_shift_op_impl(d, value);
	}

	template <typename T>
	std::enable_if_t<type_traits<T>::is_any_ptr, deserializer&>
	operator>>(deserializer& d, T&& value) {
		assert(value);
		return d << *value;
	}

	template <typename T>
	deserializer& operator>>(deserializer& d, const std::unique_ptr<T>& value) {
		return d >> value.get();
	}

	template <typename T>
	deserializer& operator>>(deserializer& d, const std::shared_ptr<T>& value) {
		return d >> value.get();
	}


} // namespace breep
