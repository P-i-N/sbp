# Structured Bindings Pack
Serialize C++ structs into [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) binary form with minimal boilerplate code.

## Example
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

// Deserialize back into second instance
UserData ud2;
sbp::read(buff, ud2);
```

#### Serialized UserData in binary:
```cpp
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

## TODO
- error handling
- better extensibility
- performance
- more tests

## Credits
- structured binding code originally taken from [avakar/destructure](https://github.com/avakar/destructure)
- binary serialization follows [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) format (to some extent)
