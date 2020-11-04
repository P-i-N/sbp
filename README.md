# Structured Bindings Pack
Header only library for serializing C++ structs into [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) binary form with minimal boilerplate code. Requires C++17 and MSVC compiler (but should probably work with other compilers as well). Obviously, this is just a fun project, highly experimental, avoid using it during _rocket surgeries_.

## What is this good for?
If you need to serialize small non-POD structures (i.e., you can't just use plain `memcpy`), this library might provide good quick solution. There is no need to manually write serialization and deserialization code for each and every struct you have. There aren't even any message definition files (like `.proto` files in Google's [Protocol Buffers](https://developers.google.com/protocol-buffers)). Everything is resolved during compilation.

## Quick example
```cpp
struct UserData final
{
	int age;
	float height;
	std::string name;
	uint64_t password_hash;
	std::vector<int> lucky_numbers;
};

// Fill UserData struct with some random data
UserData ud;
ud.age = 32;
ud.height = 1.75f;
ud.name = "Jeff";
ud.password_hash = 0xDEADBEEFDEADC0DEull;
ud.lucky_numbers = { 69, 420, 1984 };

// Serialize it into a buffer
sbp::buffer buff;
sbp::write(buff, ud);
```
That's it! As long as your structs are simple enough (see limitations below), there is no need to write any extra serialization or deserialization code.

How does serialized `UserData` instance look like in memory:
```
'20 ca 00 00 e0 3f a4 4a 65 66 66 cf de c0 ad de ef be ad de 93 45 d1 a4 01 d1 c0 07'

20                      : 7-bit positive integer (32)
ca                      : 32-bit floating point number
00 00 e0 3f             : 1.75f
a4                      : 4 bytes string
4a 65 66 66             : "Jeff"
cf                      : 64-bit unsigned integer
de c0 ad de ef be ad de : 0xDEADBEEFDEADC0DE
93                      : array of 3 elements
45                      : 7-bit positive integer (69)
d1                      : 16-bit signed integer
a4 01                   : 420
d1                      : 16-bit signed integer
c0 07                   : 1984
```

## Supported primitive types
- `bool`
- `int8_t`, `int16_t`, `int32_t`, `int64_t`
- `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`
- `float`, `double`
- `const char *`
  - to simplify deserialization, raw C-strings are serialized WITH null terminator. This goes against [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) rules and might break interoperability with other libraries. If you do not like this behaviour, use `std::string` or `std::string_view` instead!

## Supported STL containers
Unless you are using MSVC compiler, support for individual STL container types must be manually enabled:

- `std::array`
  - define `SBP_STL_ARRAY`
- `std::string`
  - define `SBP_STL_STRING`
- `std::string_view`
  - define `SBP_STL_STRING_VIEW`
- `std::vector`
  - define `SBP_STL_VECTOR`
- `std::map`
  - define `SBP_STL_MAP`
- `std::unordered_map`
  - define `SBP_STL_UNORDERED_MAP`

## Adding custom types
```cpp
struct Person final
{
	std::string name;
	int age;
	float weight;
};

namespace sbp::detail {

void write(buffer &b, const Person &value) SBP_NOEXCEPT
{
	write_multiple(b, name, age, weight);
}

error read(buffer &b, Person &value) SBP_NOEXCEPT
{
	return read_multiple(b, name, age, weight);
}

} // namespace sbp::detail

struct NuclearFamily final
{
	std::array<Person, 2> parents;
	std::vector<Person> children;
};

NuclearFamily fam = ...;

sbp::buffer buff;
sbp::write(buff, fam);
```

Simple POD types can be stored in [ext format](https://github.com/msgpack/msgpack/blob/master/spec.md#ext-format-family), which is basically just a byte type ID followed by binary data. Use `SBP_EXTENSION` macro to automatically generate code of `sbp::detail::write` and `sbp::detail::read` functions for your types:
```cpp
struct Vector2D final { float x, y; };
struct Vector3D final { float x, y, z; };
struct Vector4D final { float x, y, z, w; };

SBP_EXTENSION(Vector2D, 1)
SBP_EXTENSION(Vector3D, 2)
SBP_EXTENSION(Vector4D, 3)

struct Mesh final
{
	std::string name;
	Vector4D diffuseColor;
	Vector3D aabbMin, aabbMax;
	std::vector<Vector3D> vertices;
	std::vector<Vector2D> texCoords;
};

Mesh m = ...;

sbp::buffer buff;
sbp::write(buff, m);
```

## Limitations
- library does not care about endianness
- no STL streams or allocators support (but feel free to roll your own `sbp::buffer` implementation)
- error reporting is very primitive, no exceptions used
- inheritance does not work, use composition instead
- C-style arrays do not work, use `std::array` instead
- if you really want to use inheritance (or even C-style arrays), you have to provide template specializations for `std::tuple_size`, `std::tuple_element` and `std::get`

## Credits
- structured binding code idea originally taken from [avakar/destructure](https://github.com/avakar/destructure)
- binary serialization follows [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) format (to some extent)
