# Structured Bindings Pack
Serialize C++ structs into `MessagePack`-binary form with minimal boilerplate code.

## Example:
```cpp
struct UserData final
{
	int age = 35;
	float height = 1.75f;
	std::string name = "Jeff";
	uint64_t password_hash = 0;
	std::vector<int> lucky_numbers = { 69, 420 };
};

UserData ud;

sbp::buffer buff;
sbp::write(buff, ud);
```

## TODO:
```cpp
/* lazy Saturday, yo... */
```

## Credits:
- structured binding code originally taken from [avakar/destructure](https://github.com/avakar/destructure)
- binary serialization follows [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) format (to some extent)
