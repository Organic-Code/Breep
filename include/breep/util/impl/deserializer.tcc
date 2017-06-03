///////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                               //
// Copyright 2017 Lucas Lazare.                                                                  //
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

#include "../deserializer.hpp" // Allows my IDE to work

namespace breep {

	namespace detail {
		// The code from the following method was adapted from the public domain
		template <typename FloatType, typename InputType, unsigned int ExponentBits, unsigned int MantissaBits>
		FloatType fromIEEE(InputType value) {
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

	void read_size(deserializer& s, uint64_t& size) {
		uint8_t uint8(0);
		uint_fast8_t oct_to_read(0);
		s >> oct_to_read;
		while (oct_to_read--) {
			s >> uint8;
			size |= (static_cast<uint64_t>(uint8) << static_cast<uint64_t>(oct_to_read * 8));
		}
	}

	deserializer& operator>>(deserializer& s, uint8_t& val) {
		s.m_is.get(val);
		return s;
	}

	deserializer& operator>>(deserializer& s, bool& val) {
		uint8_t c;
		s >> c;
		val = (c == static_cast<uint8_t>('1'));
		return s;
	}

	deserializer& operator>>(deserializer& s, char& val) {
		if (std::numeric_limits<char>::min() < 0) {
			unsigned char unsigned_value(0);
			s >> unsigned_value;
			val = unsigned_value;
		} else {
			signed char signed_value(0);
			s >> signed_value;
			val =  signed_value;
		}
		return s;
	}

	deserializer& operator>>(deserializer& s, uint16_t& val) {
		uint8_t uint8(0);
		s >> uint8;
		val = static_cast<uint16_t>(static_cast<uint16_t>(uint8) << 8);
		s >> uint8;
		val = static_cast<uint16_t>(val | uint8);
		return s;
	}

	deserializer& operator>>(deserializer& s, uint32_t& val) {
		uint16_t uint16(0);
		s >> uint16;
		val = uint16 << 16;
		s >> uint16;
		val |= uint16;
		return s;
	}

	deserializer& operator>>(deserializer& s, uint64_t& val) {
		uint32_t uint32(0);
		s >> uint32;
		val = static_cast<uint64_t>(uint32) << 32;
		s >> uint32;
		val |= uint32;
		return s;
	}

	deserializer& operator>>(deserializer& s, int8_t& val) {
		uint8_t uint8(0);
		s >> uint8;
		val = static_cast<int8_t>(uint8);
		return s;
	}

	deserializer& operator>>(deserializer& s, int16_t& val) {
		uint16_t uint16(0);
		s >> uint16;
		val = static_cast<int16_t>(uint16);
		return s;
	}

	deserializer& operator>>(deserializer& s, int32_t& val) {
		uint32_t uint32(0);
		s >> uint32;
		val = static_cast<int32_t>(uint32);
		return s;
	}

	deserializer& operator>>(deserializer& s, int64_t& val) {
		uint64_t uint64(0);
		s >> uint64;
		val = static_cast<int64_t>(uint64);
		return s;
	}

	deserializer& operator>>(deserializer& s, float& val) {
		uint32_t uint32(0);
		s >> uint32;
		val = detail::fromIEEE<float, int32_t, 8, 23>(uint32);
		return s;
	}

	deserializer& operator>>(deserializer& s, double& val) {
		uint64_t uint64(0);
		s >> uint64;
		val = detail::fromIEEE<double, int64_t, 11, 52>(uint64);
		return s;
	}

	template<typename PushableContainer>
	deserializer& operator>>(deserializer& s, PushableContainer& val) {
		uint64_t size = 0;
		read_size(s, size);
		while (size--) {
			typename PushableContainer::value_type value;
			s >> value;
			val.push_back(std::move(value));
		}
		return s;
	}

	template<typename T>
	deserializer& operator>>(deserializer& s, std::forward_list<T>& val) {
		std::vector<T> tmp;
		s >> tmp;
		for (auto it = tmp.rbegin(), end = tmp.rend() ; it != end ; ++it) {
			val.emplace_front(std::move(*it));
		}
		return s;
	}

	template <typename T>
	deserializer& operator>>(deserializer& s, std::vector<T>& vector) {
		uint64_t size = 0;
		read_size(s, size);
		vector.reserve(size + vector.size());
		while(size--) {
			T value;
			s >> value;
			vector.emplace_back(std::move(value));
		}
		return s;
	}

    deserializer& operator>>(deserializer& s, std::vector<bool>& vector) {
        uint8_t mask;
        uint64_t size = 0;
        read_size(s, size);
        vector.reserve(size + vector.size());
        for (uint64_t i = 0 ; i < size ; i += std::numeric_limits<uint8_t>::digits) {
            s >> mask;
            for (uint8_t j = 0 ; j < std::numeric_limits<uint8_t>::digits && i+j < size ; ++j) {
                vector.push_back((mask & (1 << j)) != 0);
            }
        }
        return s;
    }

	template <typename T, std::size_t N>
	deserializer& operator>>(deserializer& s, std::array<T, N>& array) {
		uint64_t size = 0;
		read_size(s, size);
		for (uint_fast64_t i = 0 ; i < size ; ++i) {
			s >> array[i];
		}
		return s;
	}

	template <typename T>
	deserializer& operator>>(deserializer& s, std::deque<T>& deque) {
		uint64_t size = 0;
		read_size(s, size);
		while (size--) {
			T value;
			s >> value;
			deque.emplace_back(std::move(value));
		}
		return s;
	}

	template <typename T>
	deserializer& operator>>(deserializer& s, std::list<T>& list) {
		uint64_t size = 0;
		read_size(s, size);
		while (size--) {
			T value;
			s >> value;
			list.emplace_back(std::move(value));
		}
		return s;
	}

	template <typename T, typename U, typename V, typename W>
	deserializer& operator>>(deserializer& s, std::map<T,U,V,W>& map) {
		uint64_t size = 0;
		read_size(s, size);
		while (size--) {
			T key;
			U value;
			s >> key >> value;
			map.emplace_hint(map.cend(), std::make_pair(std::move(key), std::move(value)));
		}
		return s;
	}

	template <typename T, typename U, typename V, typename W>
	deserializer& operator>>(deserializer& s, std::multimap<T,U,V,W>& map) {
		uint64_t size = 0;
		read_size(s, size);
		while (size--) {
			T key;
			U value;
			s >> key >> value;
			map.emplace_hint(map.cend(), std::make_pair(std::move(key), std::move(value)));
		}
		return s;
	}


	template <typename T, typename U, typename V, typename W, typename X>
	deserializer& operator>>(deserializer& s, std::unordered_map<T,U,V,W,X>& map) {
		uint64_t size = 0;
		read_size(s, size);
		while (size--) {
			T key;
			U value;
			s >> key >> value;
			map.emplace_hint(map.cend(), std::make_pair(std::move(key), std::move(value)));
		}
		return s;
	}


	template <typename T, typename U, typename V, typename W, typename X>
	deserializer& operator>>(deserializer& s, std::unordered_multimap<T,U,V,W,X>& map) {
		uint64_t size = 0;
		read_size(s, size);
		while (size--) {
			T key;
			U value;
			s >> key >> value;
			map.emplace_hint(map.cend(), std::make_pair(std::move(key), std::move(value)));
		}
		return s;
	}

	template <typename T, typename U, typename V>
	deserializer& operator>>(deserializer& s, std::set<T,U,V>& set) {
		uint64_t size = 0;
		read_size(s, size);
		while (size--) {
			T value;
			s >> value;
			set.emplace_hint(set.cend(), std::move(value));
		}
		return s;
	}

	template <typename T, typename U, typename V>
	deserializer& operator>>(deserializer& s, std::multiset<T,U,V>& multiset) {
		uint64_t size = 0;
		read_size(s, size);
		while (size--) {
			T value;
			s >> value;
			multiset.emplace_hint(multiset.cend(), std::move(value));
		}
		return s;
	}

	template <typename T, typename U, typename V, typename W>
	deserializer& operator>>(deserializer& s, std::unordered_set<T,U,V,W>& unordered_set) {
		uint64_t size = 0;
		read_size(s, size);
		while (size--) {
			T value;
			s >> value;
			unordered_set.emplace_hint(unordered_set.cend(), std::move(value));
		}
		return s;
	}

	template <typename T, typename U, typename V, typename W>
	deserializer& operator>>(deserializer& s, std::unordered_multiset<T,U,V,W>& unordered_multiset){
		uint64_t size = 0;
		read_size(s, size);
		while (size--) {
			T value;
			s >> value;
			unordered_multiset.emplace_hint(unordered_multiset.cend(), std::move(value));
		}
		return s;
	}

	template <typename T, typename U>
	deserializer& operator>>(deserializer& s, std::stack<T,U>& stack) {
		std::vector<T> tmp;
		s >> tmp;
		for (auto it = tmp.rbegin(), end = tmp.rend() ; it != end ; ++it) {
			stack.emplace(std::move(*it));
		}
		return s;
	}

	template <typename T, typename U>
	deserializer& operator>>(deserializer& s, std::queue<T,U>& queue) {
		uint64_t size = 0;
		read_size(s, size);
		while(size--) {
			T value;
			s >> value;
			queue.emplace(std::move(value));
		}
		return s;
	}

	template <typename T, typename U, typename V>
	deserializer& operator>>(deserializer& s, std::priority_queue<T,U,V>& priority_queue) {
		uint64_t size = 0;
		read_size(s, size);
		while(size--) {
			T value;
			s >> value;
			priority_queue.emplace(std::move(value));
		}
		return s;
	}

	template<typename T, typename U>
	deserializer& operator>>(deserializer& s, std::pair<T, U>& val) {
		s >> val.first >> val.second;
		return s;
	}

	template<typename T>
	deserializer& operator>>(deserializer& s, std::tuple<T>& val) {
		s >> std::get<0>(val);
		return s;
	}

	template<typename T, typename U>
	deserializer& operator>>(deserializer& s, std::tuple<T, U>& val) {
		s >> std::get<0>(val) << std::get<1>(val);
		return s;
	}

	template<typename T, typename U, typename V>
	deserializer& operator>>(deserializer& s, std::tuple<T, U, V>& val) {
		s >> std::get<0>(val) << std::get<1>(val) << std::get<2>(val);
		return s;
	}

	template<typename T, typename U, typename V, typename W>
	deserializer& operator>>(deserializer& s, std::tuple<T, U, V, W>& val) {
		s >> std::get<0>(val) << std::get<1>(val) << std::get<2>(val) << std::get<3>(val);
		return s;
	}

	template<typename T, typename U, typename V, typename W, typename X>
	deserializer& operator>>(deserializer& s, std::tuple<T, U, V, W, X>& val) {
		s >> std::get<0>(val) << std::get<1>(val) << std::get<2>(val) << std::get<3>(val) << std::get<4>(val);
		return s;
	}

	template<typename T, typename U, typename V, typename W, typename X, typename Y>
	deserializer& operator>>(deserializer& s, std::tuple<T, U, V, W, X, Y>& val) {
		s >> std::get<0>(val) << std::get<1>(val) << std::get<2>(val) << std::get<3>(val) << std::get<4>(val) << std::get<5>(val);
		return s;
	}

	template<typename T, typename U, typename V, typename W, typename X, typename Y, typename Z>
	deserializer& operator>>(deserializer& s, std::tuple<T, U, V, W, X, Y>& val) {
		s >> std::get<0>(val) << std::get<1>(val) << std::get<2>(val) << std::get<3>(val) << std::get<4>(val) << std::get<5>(val) << std::get<6>(val);
		return s;
	}

// deserializing durations
	template<typename T, typename U>
	deserializer& operator>>(deserializer& s, std::chrono::duration<T, U>& val) {
		uint64_t uint64;
		s >> uint64;
		val = std::chrono::duration<T,U>(std::chrono::nanoseconds(uint64));
		return s;
	}
}
