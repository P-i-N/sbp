#pragma once

#include <array>
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
	buffer() noexcept
	{
		_data = _stackBuffer;
		_readCursor = _writeCursor = _data;
		_endCap = _data + stack_buffer_capacity;
	}

	~buffer() { reset(); }

	uint8_t* data() noexcept { return _data; }
	const uint8_t* data() const noexcept { return _data; }
	size_t size() const noexcept { return static_cast<size_t>(_writeCursor - _data); }
	size_t capacity() const noexcept { return static_cast<size_t>(_endCap - _data); }
	error valid() const noexcept { return (_readCursor <= _writeCursor) ? error::none : error::unexpected_end; }

	void reset(bool freeMemory = true) noexcept
	{
		if (freeMemory)
		{
			if (_data != _stackBuffer)
				delete[] _data;

			_data = _stackBuffer;
			_endCap = _data + stack_buffer_capacity;
		}

		_readCursor = _writeCursor = _data;
	}

	void reset_read_pos() noexcept
	{
		_readCursor = _data;
	}

	void reserve(size_t newCapacity) noexcept
	{
		if (newCapacity > capacity())
		{
			auto readOffset = _readCursor - _data;
			auto writeOffset = _writeCursor - _data;

			auto* newData = new uint8_t[newCapacity];
			memcpy(newData, _data, writeOffset);

			if (_data != _stackBuffer)
				delete[] _data;

			_data = newData;
			_readCursor = _data + readOffset;
			_writeCursor = _data + writeOffset;
			_endCap = _data + newCapacity;
		}
	}

	buffer& write(const void* data, size_t numBytes) noexcept
	{
		ensure_capacity(numBytes);
		memcpy(_writeCursor, data, numBytes);
		_writeCursor += numBytes;
		return *this;
	}

	template <size_t NumBytes>
	buffer& write(const void* data) noexcept
	{
		if constexpr (NumBytes == 1)
		{
			if (_writeCursor == _endCap)
				reserve(capacity() * 2);

			*_writeCursor++ = *reinterpret_cast<const uint8_t*>(data);
		}
		else
		{
			ensure_capacity(NumBytes);
			memcpy(_writeCursor, data, NumBytes);
			_writeCursor += NumBytes;
		}

		return *this;
	}

	template <typename T>
	buffer& write(T&& value) noexcept { return write<sizeof(T)>(&value); }

	template <typename T>
	buffer& write(uint8_t header, T&& value) noexcept
	{
		ensure_capacity(1 + sizeof(T));
		*_writeCursor++ = header;
		memcpy(_writeCursor, &value, sizeof(T));
		_writeCursor += sizeof(T);
		return *this;
	}

	void read(void* data, size_t numBytes) noexcept
	{
		if (_readCursor + numBytes <= _writeCursor)
			memcpy(data, _readCursor, numBytes);

		_readCursor += numBytes;
	}

	template <typename T>
	T read() noexcept
	{
		_readCursor += sizeof(T);
		return (_readCursor <= _writeCursor) ? *reinterpret_cast<const T*>(_readCursor - sizeof(T)) : T(0);
	}

	template <typename RT, typename T>
	error read(T& result) noexcept
	{
		if constexpr (sizeof(T) < sizeof(RT))
			return error::corrupted_data;

		if (_readCursor + sizeof(RT) > _writeCursor)
			return error::unexpected_end;

		result = static_cast<T>(*reinterpret_cast<const RT*>(_readCursor));
		_readCursor += sizeof(RT);
		return error::none;
	}

	const void *skip(size_t numBytes) noexcept
	{
		const void* result = (_readCursor + numBytes <= _writeCursor) ? _readCursor : nullptr;
		_readCursor += numBytes;
		return result;
	}

private:
	void ensure_capacity(size_t NumBytes) noexcept
	{
		if (_writeCursor + NumBytes > _endCap)
		{
			auto cap1 = size() + NumBytes;
			auto cap2 = capacity() * 2;

			reserve((cap1 > cap2) ? cap1 : cap2);
		}
	}

	static constexpr size_t stack_buffer_capacity = 256;

	uint8_t* _writeCursor = nullptr;
	const uint8_t* _readCursor = nullptr;
	const uint8_t* _endCap = nullptr;

	uint8_t* _data = nullptr;
	uint8_t _stackBuffer[stack_buffer_capacity] = { };
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T, int8_t TypeID = 0>
class ext final
{
public:
	ext() noexcept = default;
	ext(const T &value) noexcept: _value(value) { }

	operator T* () const { return &_value; }
	T* operator->() noexcept { return &_value; }

private:
	T _value;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace detail {

template <size_t>
struct any { template <typename T> operator T() const; };

template <typename T, typename In, typename = void>
struct has_n_members : std::false_type { };

template <typename T, size_t... In>
struct has_n_members<T, std::index_sequence<In...>, std::void_t<decltype(T{ any<In>()... })>> : std::true_type { };

template <typename T, typename In>
struct has_n_members<T&, In> : has_n_members<T, In> { };

template <typename T, typename In>
struct has_n_members<T&&, In> : has_n_members<T, In> { };

template <typename T, size_t N>
constexpr bool has_n_members_v = has_n_members<T, std::make_index_sequence<N>>::value;

template <typename T>
auto destructure(T& t) noexcept
{
#define _SBP_TIE(_Count, ...) \
	if constexpr (has_n_members_v<T, _Count>) { \
		auto &&[__VA_ARGS__] = std::forward<T>(t); \
		return std::tie(__VA_ARGS__); } else

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
	{ return std::tuple(); }

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
		b.write(0xd0u, int8_t(value));
	else if (value <= nl<int16_t>::max() && value >= nl<int16_t>::min())
		b.write(0xd1u, int16_t(value));
	else if (value <= nl<int32_t>::max() && value >= nl<int32_t>::min())
		b.write(0xd2u, int32_t(value));
	else
		b.write(0xd3u, value);
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
		b.write(0xccu, uint8_t(value));
	else if (value <= nl<uint16_t>::max())
		b.write(0xcdu, uint16_t(value));
	else if (value <= nl<uint32_t>::max())
		b.write(0xceu, uint32_t(value));
	else
		b.write(0xcfu, value);
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
		b.write(0xd9u, uint8_t(length));
	else if (length <= nl<uint16_t>::max())
		b.write(0xdau, uint16_t(length));
	else
		b.write(0xdbu, uint32_t(length));

	b.write(value, length);
}

//---------------------------------------------------------------------------------------------------------------------
void write(buffer& b, const std::string& value) noexcept { write_str(b, value.c_str(), value.length()); }
void write(buffer& b, const char* value) noexcept { write_str(b, value, value ? (strlen(value) + 1) : 0); }

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
		b.write(0xdcu, uint16_t(numValues));
	else
		b.write(0xddu, uint32_t(numValues));

	for (size_t i = 0; i < numValues; ++i)
		write(b, values[i]);
}

//---------------------------------------------------------------------------------------------------------------------
template <size_t NumValues, typename T>
void write_array_fixed(buffer& b, const T* values) noexcept
{
	if constexpr (NumValues <= 15)
		b.write(uint8_t(uint8_t(0b10010000u) | static_cast<uint8_t>(NumValues)));
	else if constexpr (NumValues <= nl<uint16_t>::max())
		b.write(0xdcu, uint16_t(NumValues));
	else
		b.write(0xddu, uint32_t(NumValues));

	for (size_t i = 0; i < NumValues; ++i)
		write(b, values[i]);
}

//---------------------------------------------------------------------------------------------------------------------
template <typename T, typename A>
void write(buffer& b, const std::vector<T, A>& value) noexcept { write_array(b, value.data(), value.size()); }

template <typename T, size_t NumValues>
void write(buffer& b, const T (&value)[NumValues]) noexcept { write_array(b, value, NumValues); }

template <typename T, size_t NumValues>
void write(buffer& b, const std::array<T, NumValues>& value) noexcept { write_array(b, value.data(), NumValues); }

//---------------------------------------------------------------------------------------------------------------------
template <typename T>
void write_map(buffer& b, const T& value) noexcept
{
	auto numValues = value.size();

	if (numValues <= 15)
		b.write(uint8_t(uint8_t(0b10000000u) | static_cast<uint8_t>(numValues)));
	else if (numValues <= nl<uint16_t>::max())
		b.write(0xdeu, uint16_t(numValues));
	else
		b.write(0xdfu, uint32_t(numValues));

	for (const auto& kvp : value)
	{
		write(b, kvp.first);
		write(b, kvp.second);
	}
}

//---------------------------------------------------------------------------------------------------------------------
template <typename K, typename T, typename P, typename A>
void write(buffer& b, const std::map<K, T, P, A>& value) noexcept { write_map(b, value); }

template <typename K, typename T, typename H, typename EQ, typename A>
void write(buffer& b, const std::unordered_map<K, T, H, EQ, A>& value) noexcept { write_map(b, value); }

//---------------------------------------------------------------------------------------------------------------------
void write_bin(buffer& b, const void* data, size_t numBytes) noexcept
{
	if (numBytes <= nl<uint8_t>::max())
		b.write(0xc4u, uint8_t(numBytes));
	else if (numBytes <= nl<uint16_t>::max())
		b.write(0xc5u, uint16_t(numBytes));
	else
		b.write(0xc6u, uint32_t(numBytes));

	b.write(data, numBytes);
}

//---------------------------------------------------------------------------------------------------------------------
template <size_t NumBytes>
void write_ext(buffer& b, int8_t type, const void* data) noexcept
{
	if constexpr (NumBytes == 1)
		b.write(0xd4u, type);
	else if constexpr (NumBytes == 2)
		b.write(0xd5u, type);
	else if constexpr (NumBytes == 4)
		b.write(0xd6u, type);
	else if constexpr (NumBytes == 8)
		b.write(0xd7u, type);
	else if constexpr (NumBytes == 16)
		b.write(0xd8u, type);
	else if constexpr (NumBytes <= nl<uint8_t>::max())
		b.write(0xc7u, uint8_t(NumBytes)).write(type);
	else if constexpr (NumBytes <= nl<uint16_t>::max())
		b.write(0xc8u, uint16_t(NumBytes)).write(type);
	else
		b.write(0xc9u, uint32_t(NumBytes)).write(type);

	b.write(data, NumBytes);
}

//---------------------------------------------------------------------------------------------------------------------
void write_ext(buffer& b, int8_t type, const void* data, size_t numBytes) noexcept
{
	if (numBytes == 1)
		b.write(0xd4u, type);
	else if (numBytes == 2)
		b.write(0xd5u, type);
	else if (numBytes == 4)
		b.write(0xd6u, type);
	else if (numBytes == 8)
		b.write(0xd7u, type);
	else if (numBytes == 16)
		b.write(0xd8u, type);
	else if (numBytes <= nl<uint8_t>::max())
		b.write(0xc7u, uint8_t(numBytes)).write(type);
	else if (numBytes <= nl<uint16_t>::max())
		b.write(0xc8u, uint16_t(numBytes)).write(type);
	else
		b.write(0xc9u, uint32_t(numBytes)).write(type);

	b.write(data, numBytes);
}

//---------------------------------------------------------------------------------------------------------------------
template <typename T, int8_t TypeID>
void write(buffer& b, const ext<T, TypeID>& value) noexcept { write_ext<sizeof(T)>(b, TypeID, &value); }

//---------------------------------------------------------------------------------------------------------------------
template <size_t I = 0, typename... Types>
void write(buffer& b, const std::tuple<Types...>& t) noexcept
{
	const auto& value = std::get<I>(t);
	using Type = std::remove_reference_t<decltype(value)>;

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
	{
		value = static_cast<T>(header);
		return b.valid();
	}
	else if ((header & 0b11100000u) == 0b11100000u)
	{
		value = static_cast<T>(static_cast<int8_t>(header));
		return b.valid();
	}
	else if (header == 0xd0u)
		return b.read<int8_t>(value);
	else if (header == 0xd1u)
		return b.read<int16_t>(value);
	else if (header == 0xd2u)
		return b.read<int32_t>(value);
	else if (header == 0xd3u)
		return b.read<int64_t>(value);

	return error::corrupted_data;
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
	{
		value = header;
		return b.valid();
	}
	else if (header == 0xccu)
		return b.read<uint8_t>(value);
	else if (header == 0xcdu)
		return b.read<uint16_t>(value);
	else if (header == 0xceu)
		return b.read<uint32_t>(value);
	else if (header == 0xcfu)
		return b.read<uint64_t>(value);

	return error::corrupted_data;
}

//---------------------------------------------------------------------------------------------------------------------
error read(buffer& b, uint8_t&  value) noexcept { return read_uint(b, value); }
error read(buffer& b, uint16_t& value) noexcept { return read_uint(b, value); }
error read(buffer& b, uint32_t& value) noexcept { return read_uint(b, value); }
error read(buffer& b, uint64_t& value) noexcept { return read_uint(b, value); }

//---------------------------------------------------------------------------------------------------------------------
error read_string_length(buffer& b, size_t& value) noexcept
{
	if (auto header = b.read<uint8_t>(); (header & 0b11100000u) == 0b10100000u)
	{
		value = header & 0b00011111u;
		return b.valid();
	}
	else if (header == 0xd9u)
		return b.read<uint8_t>(value);
	else if (header == 0xdau)
		return b.read<uint16_t>(value);
	else if (header == 0xdbu)
		return b.read<uint32_t>(value);

	return error::corrupted_data;
}

//---------------------------------------------------------------------------------------------------------------------
error read(buffer& b, std::string& value) noexcept
{
	size_t length = 0;
	if (auto err = read_string_length(b, length); !!err)
		return err;

	value.resize(length);
	b.read(value.data(), length);
	return b.valid();
}

//---------------------------------------------------------------------------------------------------------------------
error read(buffer& b, const char*& value) noexcept
{
	size_t length = 0;
	if (auto err = read_string_length(b, length); !!err)
		return err;

	value = reinterpret_cast<const char *>(b.skip(length));
	return b.valid();
}

//---------------------------------------------------------------------------------------------------------------------
error read(buffer& b, float& value) noexcept
{
	if (b.read<uint8_t>() != 0xcau)
		return error::corrupted_data;

	return b.read<float>(value);
}

//---------------------------------------------------------------------------------------------------------------------
error read(buffer& b, double& value) noexcept
{
	if (b.read<uint8_t>() != 0xcbu)
		return error::corrupted_data;

	return b.read<double>(value);
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
	{
		value = header & 0b00001111u;
		return error::none;
	}
	else if (header == 0xdcu)
		return b.read<uint16_t>(value);
	else if (header == 0xddu)
		return b.read<uint32_t>(value);

	return error::corrupted_data;
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
template <typename T, size_t NumValues>
error read(buffer& b, std::array<T, NumValues>& value) noexcept
{
	size_t numValues = 0;
	if (auto err = read_array_length(b, numValues); !!err)
		return err;

	if (numValues != NumValues)
		return error::corrupted_data;

	for (size_t i = 0; i < NumValues; ++i)
	{
		if (auto err = read(b, value[i]); !!err)
			return err;
	}

	return b.valid();
}

//---------------------------------------------------------------------------------------------------------------------
error read_map_length(buffer& b, size_t& value) noexcept
{
	if (auto header = b.read<uint8_t>(); (header & 0b11110000u) == 0b10000000u)
	{
		value = header & 0b00001111u;
		return error::none;
	}
	else if (header == 0xdeu)
		return b.read<uint16_t>(value);
	else if (header == 0xdfu)
		return b.read<uint32_t>(value);

	return error::corrupted_data;
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
error read(buffer& b, std::map<K, T, P, A>& value) noexcept { return read_map(b, value); }

template <typename K, typename T, typename H, typename EQ, typename A>
error read(buffer& b, std::unordered_map<K, T, H, EQ, A>& value) noexcept { return read_map(b, value); }

//---------------------------------------------------------------------------------------------------------------------
template <size_t NumBytes>
error read_ext(buffer& b, int8_t& type, const void*& value) noexcept
{
	auto header = b.read<uint8_t>();
	if constexpr (NumBytes == 1)
	{
		if (header != 0xd4u)
			return error::corrupted_data;
	}
	else if constexpr (NumBytes == 2)
	{
		if (header != 0xd5u)
			return error::corrupted_data;
	}
	else if constexpr (NumBytes == 4)
	{
		if (header != 0xd6u)
			return error::corrupted_data;
	}
	else if constexpr (NumBytes == 8)
	{
		if (header != 0xd7u)
			return error::corrupted_data;
	}
	else if constexpr (NumBytes == 16)
	{
		if (header != 0xd8u)
			return error::corrupted_data;
	}
	else if constexpr (NumBytes <= nl<uint8_t>::max())
	{
		if (header != 0xc7u || b.read<uint8_t>() != NumBytes)
			return error::corrupted_data;
	}
	else if constexpr (NumBytes <= nl<uint16_t>::max())
	{
		if (header != 0xc8u || b.read<uint16_t>() != NumBytes)
			return error::corrupted_data;
	}
	else
	{
		if (header != 0xc9u || b.read<uint32_t>() != NumBytes)
			return error::corrupted_data;
	}

	if (auto err = b.read<int8_t>(type); !!err)
		return err;

	value = b.skip(NumBytes);
	return b.valid();
}

//---------------------------------------------------------------------------------------------------------------------
template <typename T, int8_t TypeID>
error read(buffer& b, ext<T, TypeID>& value) noexcept
{
	int8_t type = 0;
	const void* data = nullptr;
	if (auto err = read_ext<sizeof(T)>(b, type, data); !!err)
		return err;

	if (type != TypeID)
		return error::corrupted_data;

	memcpy(&value, data, sizeof(T));
	return b.valid();
}

//---------------------------------------------------------------------------------------------------------------------
template <size_t I = 0, typename... Types>
error read(buffer& b, std::tuple<Types...>& t) noexcept
{
	auto& value = std::get<I>(t);
	using Type = std::remove_reference_t<decltype(value)>;

	if constexpr (std::is_enum_v<Type>)
	{
		auto underlyingValue = std::underlying_type_t<Type>(0);
		if (auto err = read(b, underlyingValue); !!err)
			return err;

		value = static_cast<Type>(underlyingValue);
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
	return detail::read(b, detail::destructure(msg));
}

} // namespace sbp
