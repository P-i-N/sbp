#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <typeinfo>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sbp {

enum class error
{
	none = 0,
	corrupted_data,
	unexpected_end
};

constexpr auto operator!(error err) { return err == error::none; }

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class buffer final
{
public:
	buffer() noexcept { _data = _stackBuffer; }

	~buffer() { reset(); }

	void* data() noexcept { return _data; }
	const void* data() const noexcept { return _data; }

	size_t size() const noexcept { return _writePos; }

	error valid() const noexcept { return (_readPos <= _writePos) ? error::none : error::unexpected_end; }

	void reset() noexcept
	{
		if (_data != _stackBuffer)
			delete[] _data;

		_data = _stackBuffer;
		_capacity = stack_buffer_capacity;
		_writePos = _readPos = 0;
	}

	buffer& write(const void* data, size_t numBytes) noexcept
	{
		if ((_writePos += numBytes) >= _capacity)
		{
			auto* newData = new uint8_t[_capacity *= 2];
			memcpy(newData, _data, _writePos - numBytes);

			if (_data != _stackBuffer)
				delete[] _data;

			_data = newData;
		}

		memcpy(_data + _writePos - numBytes, data, numBytes);
		return *this;
	}

	template <typename T>
	buffer& write(T value) noexcept { return write(&value, sizeof(T)); }
	
	void read(void* data, size_t numBytes) noexcept
	{
		if (_readPos + numBytes <= _writePos)
			memcpy(data, _data + _readPos, numBytes);

		_readPos += numBytes;
	}

	template <typename T>
	T read() noexcept
	{
		_readPos += sizeof(T);
		return (_readPos <= _writePos) ? *reinterpret_cast<const T*>(_data + _readPos - sizeof(T)) : T(0);
	}

private:
	static constexpr size_t stack_buffer_capacity = 256;

	size_t _writePos = 0;
	size_t _readPos = 0;
	uint8_t* _data = nullptr;
	size_t _capacity = stack_buffer_capacity;
	uint8_t _stackBuffer[stack_buffer_capacity] = { };
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace detail {

template <size_t>
struct any { template <typename T> operator T() const; };

template <typename T, typename In, typename = void>
struct has_n_members : std::false_type { };

template <typename T, typename In>
struct has_n_members<T&, In> : has_n_members<T, In> { };

template <typename T, typename In>
struct has_n_members<T&&, In> : has_n_members<T, In> { };

template <typename T, size_t... In>
struct has_n_members < T, std::index_sequence<In...>, std::void_t<decltype(T{ any<In>()... }) >> : std::true_type { };

template <typename T, size_t N>
constexpr bool has_n_members_v = has_n_members<T, std::make_index_sequence<N>>::value;

template <typename T>
auto destructure(T& t)
{
#define _SBP_TIE(_Count, ...) \
	else if constexpr (has_n_members_v<T, _Count>) { \
		auto &&[__VA_ARGS__] = std::forward<T>(t); \
		return std::tie(__VA_ARGS__); }

	if (false) { }
	_SBP_TIE(16, m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15)
	_SBP_TIE(15, m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14)
	_SBP_TIE(14, m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13)
	_SBP_TIE(13, m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12)
	_SBP_TIE(12, m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11)
	_SBP_TIE(11, m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10)
	_SBP_TIE(10, m0, m1, m2, m3, m4, m5, m6, m7, m8, m9)
	_SBP_TIE( 9, m0, m1, m2, m3, m4, m5, m6, m7, m8)
	_SBP_TIE( 8, m0, m1, m2, m3, m4, m5, m6, m7)
	_SBP_TIE( 7, m0, m1, m2, m3, m4, m5, m6)
	_SBP_TIE( 6, m0, m1, m2, m3, m4, m5)
	_SBP_TIE( 5, m0, m1, m2, m3, m4)
	_SBP_TIE( 4, m0, m1, m2, m3)
	_SBP_TIE( 3, m0, m1, m2)
	_SBP_TIE( 2, m0, m1)
	_SBP_TIE( 1, m0)
	else { return std::tuple(); }

#undef _SBP_TIE
}

template <typename T> using nl = std::numeric_limits<T>;

//---------------------------------------------------------------------------------------------------------------------
void write_int(buffer& b, int64_t value) noexcept
{
	if (value >= 0 && value <= nl<int8_t>::max())
		b.write(static_cast<int8_t>(value));
	else if (value < 0 && value >= -32)
		b.write(static_cast<int8_t>(value));
	else if (value <= nl<int8_t>::max() && value >= nl<int8_t>::min())
		b.write(uint8_t(0xd0u)).write(int8_t(value));
	else if (value <= nl<int16_t>::max() && value >= nl<int16_t>::min())
		b.write(uint8_t(0xd1u)).write(int16_t(value));
	else if (value <= nl<int32_t>::max() && value >= nl<int32_t>::min())
		b.write(uint8_t(0xd2u)).write(int32_t(value));
	else
		b.write(uint8_t(0xd3u)).write(value);
}

//---------------------------------------------------------------------------------------------------------------------
void write(buffer& b, int8_t  value) noexcept { write_int(b, value); }
void write(buffer& b, int16_t value) noexcept { write_int(b, value); }
void write(buffer& b, int32_t value) noexcept { write_int(b, value); }
void write(buffer& b, int64_t value) noexcept { write_int(b, value); }

//---------------------------------------------------------------------------------------------------------------------
void write_uint(buffer& b, uint64_t value) noexcept
{
	if (value <= nl<int8_t>::max())
		b.write(static_cast<uint8_t>(value));
	else if (value <= nl<uint8_t>::max())
		b.write(uint8_t(0xccu)).write(static_cast<uint8_t>(value));
	else if (value <= nl<uint16_t>::max())
		b.write(uint8_t(0xcdu)).write(static_cast<uint16_t>(value));
	else if (value <= nl<uint32_t>::max())
		b.write(uint8_t(0xceu)).write(static_cast<uint32_t>(value));
	else
		b.write(uint8_t(0xcfu)).write(value);
}

//---------------------------------------------------------------------------------------------------------------------
void write(buffer& b, uint8_t  value) noexcept { write_uint(b, value); }
void write(buffer& b, uint16_t value) noexcept { write_uint(b, value); }
void write(buffer& b, uint32_t value) noexcept { write_uint(b, value); }
void write(buffer& b, uint64_t value) noexcept { write_uint(b, value); }

//---------------------------------------------------------------------------------------------------------------------
void write_str(buffer& b, const char* value, size_t length) noexcept
{
	if (length <= 31)
		b.write(uint8_t(uint8_t(0b10100000u) | static_cast<uint8_t>(length)));
	else if (length <= nl<uint8_t>::max())
		b.write(uint8_t(0xd9u)).write(static_cast<uint8_t>(length));
	else if (length <= nl<uint16_t>::max())
		b.write(uint8_t(0xdau)).write(static_cast<uint16_t>(length));
	else
		b.write(uint8_t(0xdbu)).write(static_cast<uint32_t>(length));

	b.write(value, length);
}

//---------------------------------------------------------------------------------------------------------------------
void write(buffer& b, const std::string& value) noexcept { write_str(b, value.c_str(), value.length()); }

//---------------------------------------------------------------------------------------------------------------------
void write(buffer& b, float value) noexcept { b.write(uint8_t(0xca)).write(value); }
void write(buffer& b, double value) noexcept { b.write(uint8_t(0xcb)).write(value); }

//---------------------------------------------------------------------------------------------------------------------
void write(buffer& b, bool value) noexcept { b.write(value ? uint8_t(0xc3u) : uint8_t(0xc2u)); }

//---------------------------------------------------------------------------------------------------------------------
template <typename T>
void write_array(buffer& b, const T* values, size_t numValues) noexcept
{
	if (numValues <= 15)
		b.write(uint8_t(uint8_t(0b10010000u) | static_cast<uint8_t>(numValues)));
	else if (numValues <= nl<uint16_t>::max())
		b.write(uint8_t(0xdcu)).write(uint16_t(numValues));
	else
		b.write(uint8_t(0xddu)).write(uint32_t(numValues));

	for (size_t i = 0; i < numValues; ++i)
		write(b, values[i]);
}

//---------------------------------------------------------------------------------------------------------------------
template <typename T, typename A>
void write(buffer& b, const std::vector<T, A>& value) noexcept
{
	write_array(b, value.data(), value.size());
}

//---------------------------------------------------------------------------------------------------------------------
template <typename T>
void write_map(buffer& b, const T& value) noexcept
{
	auto numValues = value.size();

	if (numValues <= 15)
		b.write(uint8_t(uint8_t(0b10000000u) | static_cast<uint8_t>(numValues)));
	else if (numValues <= nl<uint16_t>::max())
		b.write(uint8_t(0xdeu)).write(uint16_t(numValues));
	else
		b.write(uint8_t(0xdfu)).write(uint32_t(numValues));

	for (const auto& kvp : value)
	{
		write(b, kvp.first);
		write(b, kvp.second);
	}
}

//---------------------------------------------------------------------------------------------------------------------
template <typename K, typename T, typename P, typename A>
void write(buffer& b, const std::map<K, T, P, A>& value) { write_map(b, value); }

template <typename K, typename T, typename H, typename EQ, typename A>
void write(buffer& b, const std::unordered_map<K, T, H, EQ, A>& value) { write_map(b, value); }

//---------------------------------------------------------------------------------------------------------------------
template <size_t I = 0, typename... Types>
void write(buffer& b, const std::tuple<Types...>& t) noexcept
{
	const auto& value = std::get<I>(t);
	using Type = std::decay_t<decltype(value)>;

	if constexpr (std::is_enum_v<Type>)
		write(b, std::underlying_type_t<Type>(value));
	else
		write(b, value);

	if constexpr (I + 1 != sizeof...(Types))
		write<I + 1>(b, t);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
template <typename T>
error read_int(buffer& b, T& value) noexcept
{
	if (auto header = b.read<uint8_t>(); (header & 0b10000000u) == 0)
		value = static_cast<T>(header);
	else if ((header & 0b11100000u) == 0b11100000u)
	{
		// TODO: Convert this properly
	}
	else if (header == 0xd0u)
		value = static_cast<T>(b.read<int8_t>());
	else if (header == 0xd1u)
		value = static_cast<T>(b.read<int16_t>());
	else if (header == 0xd2u)
		value = static_cast<T>(b.read<int32_t>());
	else if (header == 0xd3u)
		value = static_cast<T>(b.read<int64_t>());
	else
		return error::corrupted_data;

	return b.valid();
}

//---------------------------------------------------------------------------------------------------------------------
error read(buffer& b, int8_t&  value) noexcept { return read_int(b, value); }
error read(buffer& b, int16_t& value) noexcept { return read_int(b, value); }
error read(buffer& b, int32_t& value) noexcept { return read_int(b, value); }
error read(buffer& b, int64_t& value) noexcept { return read_int(b, value); }

//---------------------------------------------------------------------------------------------------------------------
template <typename T>
error read_uint(buffer& b, T& value) noexcept
{
	if (auto header = b.read<uint8_t>(); (header & 0b10000000u) == 0)
		value = header;
	else if (header == 0xccu)
		value = b.read<uint8_t>();
	else if (header == 0xcdu)
	{
		if (sizeof(T) < 2)
			return error::corrupted_data;

		value = static_cast<T>(b.read<uint16_t>());
	}
	else if (header == 0xceu)
	{
		if (sizeof(T) < 4)
			return error::corrupted_data;

		value = static_cast<T>(b.read<uint32_t>());
	}
	else if (header == 0xcfu)
	{
		if (sizeof(T) < 8)
			return error::corrupted_data;

		value = static_cast<T>(b.read<uint64_t>());
	}
	else
		return error::corrupted_data;

	return b.valid();
}

//---------------------------------------------------------------------------------------------------------------------
error read(buffer& b, uint8_t&  value) noexcept { return read_uint(b, value); }
error read(buffer& b, uint16_t& value) noexcept { return read_uint(b, value); }
error read(buffer& b, uint32_t& value) noexcept { return read_uint(b, value); }
error read(buffer& b, uint64_t& value) noexcept { return read_uint(b, value); }

//---------------------------------------------------------------------------------------------------------------------
error read(buffer& b, std::string& value) noexcept
{
	size_t length = 0;

	if (auto header = b.read<uint8_t>(); (header & 0b11100000u) == 0b10100000u)
		length = header & 0b00011111u;
	else if (header == 0xd9u)
		length = b.read<uint8_t>();
	else if (header == 0xdau)
		length = b.read<uint16_t>();
	else if (header == 0xdbu)
		length = b.read<uint32_t>();
	else
		return error::corrupted_data;

	value.resize(length);
	b.read(value.data(), length);
	return b.valid();
}

//---------------------------------------------------------------------------------------------------------------------
error read(buffer& b, float& value) noexcept
{
	if (b.read<uint8_t>() != 0xcau)
		return error::corrupted_data;

	value = b.read<float>();
	return b.valid();
}

//---------------------------------------------------------------------------------------------------------------------
error read(buffer& b, double& value) noexcept
{
	if (b.read<uint8_t>() != 0xcbu)
		return error::corrupted_data;

	value = b.read<double>();
	return b.valid();
}

//---------------------------------------------------------------------------------------------------------------------
error read(buffer& b, bool& value) noexcept
{
	if (auto header = b.read<uint8_t>(); header == 0xc2u || header == 0xc3u)
	{
		value = (header == 0xc3u);
		return error::none;
	}

	return error::corrupted_data;
}

//---------------------------------------------------------------------------------------------------------------------
error read_array_length(buffer& b, size_t& value) noexcept
{
	if (auto header = b.read<uint8_t>(); (header & 0b11110000u) == 0b10010000u)
		value = header & 0b00001111u;
	else if (header == 0xdcu)
		value = b.read<uint16_t>();
	else if (header == 0xddu)
		value = b.read<uint32_t>();
	else
		return error::corrupted_data;
	
	return b.valid();
}

//---------------------------------------------------------------------------------------------------------------------
template <typename T, typename A>
error read(buffer& b, std::vector<T, A>& value) noexcept
{
	size_t numValues = 0;
	if (auto err = read_array_length(b, numValues); !!err)
		return err;

	value.clear();
	value.reserve(value.size() + numValues);
	for (size_t i = 0; i < numValues; ++i)
	{
		if (auto err = read(b, value.emplace_back()); !!err)
			return err;
	}

	return b.valid();
}

//---------------------------------------------------------------------------------------------------------------------
error read_map_length(buffer& b, size_t& value) noexcept
{
	if (auto header = b.read<uint8_t>(); (header & 0b11110000u) == 0b10000000u)
		value = header & 0b00001111u;
	else if (header == 0xdeu)
		value = b.read<uint16_t>();
	else if (header == 0xdfu)
		value = b.read<uint32_t>();
	else
		return error::corrupted_data;

	return b.valid();
}

//---------------------------------------------------------------------------------------------------------------------
template <typename T>
error read_map(buffer& b, T& value) noexcept
{
	size_t numValues = 0;
	if (auto err = read_map_length(b, numValues); !!err)
		return err;

	std::pair<typename T::key_type, typename T::mapped_type> kvp;

	value.clear();
	for (size_t i = 0; i < numValues; ++i)
	{
		if (auto err = read(b, kvp.first); !!err)
			return err;

		if (auto err = read(b, kvp.second); !!err)
			return err;

		value.emplace(std::move(kvp));
	}

	return b.valid();
}

//---------------------------------------------------------------------------------------------------------------------
template <typename K, typename T, typename P, typename A>
error read(buffer& b, std::map<K, T, P, A>& value)
{
	return read_map(b, value);
}

//---------------------------------------------------------------------------------------------------------------------
template <typename K, typename T, typename H, typename EQ, typename A>
error read(buffer& b, std::unordered_map<K, T, H, EQ, A>& value)
{
	return read_map(b, value);
}

//---------------------------------------------------------------------------------------------------------------------
template <size_t I = 0, typename... Types>
error read(buffer& b, std::tuple<Types...>& t) noexcept
{
	auto& value = std::get<I>(t);
	using Type = std::decay_t<decltype(value)>;

	if constexpr (std::is_enum_v<Type>)
	{
		auto underlyingType = std::underlying_type_t<Type>(0);
		if (auto err = read(b, underlyingType); !!err)
			return err;

		value = static_cast<Type>(underlyingType);
	}
	else
	{
		if (auto err = read(b, value); !!err)
			return err;
	}

	if constexpr (I + 1 != sizeof...(Types))
	{
		if (auto err = read<I + 1>(b, t); !!err)
			return err;
	}

	return b.valid();
}

} // namespace detail

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
template <typename T>
void write(buffer& b, const T& msg) noexcept
{
	detail::write(b, detail::destructure(msg));
}

//---------------------------------------------------------------------------------------------------------------------
template <typename T>
error read(buffer& b, T& msg) noexcept
{
	if (auto err = detail::read(b, detail::destructure(msg)); !!err)
	{
		// TODO: Return proper reason
		return err;
	}

	return error::none;
}

} // namespace sbp
