# static_reflect

Small, header-only C++17 reflection metadata with macro-driven distributed field declarations.

## Distributed declaration style

```cpp
#include <static_reflect/reflect.hpp>
#include <string>

enum Tags : static_reflect::TagMask { Save = 1, Editable = 2 };

struct Person {
    REFLECTABLE(Person)
    REFLECT_MEMBER_INIT(int, age, 30, Save | Editable);
    REFLECT_MEMBER_INIT_UNTAGGED(std::string, name, "Ada");
    END_REFLECTABLE();
};
REGISTER_REFLECT(Person)
```

`REFLECT_MEMBER` declares the member and its reflection metadata in one place. The declarations can be distributed through the class body. `__COUNTER__` assigns a relative slot to each reflected declaration; the recursive `StaticReflectDescriptor` primary template bridges slots consumed by unrelated macros. The final slot is captured by `END_REFLECTABLE()`.

Unlike a static-registration-list implementation, this design does not construct per-field registration objects or mutate a registration head during static initialization. The descriptors are assembled in declaration order on the first call to `static_reflect::type<T>()` and then cached.

Available field macros:

- `REFLECT_MEMBER(Type, name, tags)`
- `REFLECT_MEMBER_INIT(Type, name, initializer, tags)`
- `REFLECT_MEMBER_UNTAGGED(Type, name)`
- `REFLECT_MEMBER_INIT_UNTAGGED(Type, name, initializer)`

For existing classes with members declared elsewhere, the legacy centralized `BEGIN_REFLECT` / `REFLECT_FIELD` / `END_REFLECT()` form remains available.

## Querying metadata

```cpp
const auto& meta = static_reflect::type<Person>();
for (const auto& field : meta) {
    // field.name, field.value_type, field.tags
}
if (const auto* age = meta.find_field("age"))
    age->set(person, 42); // exact-type assignment
```

Tag masks are user-defined `std::uint64_t` values. `FieldDescriptor` exposes typed `get`, `get_const`, and `set` helpers. `REGISTER_REFLECT(T)` enables the `static_reflect::type<T>()` convenience function.

## Build

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Notes and limitations

- Requires C++17 and compiler support for `__COUNTER__` (supported by MSVC, GCC, and Clang, but not standardized by C++17).
- `REFLECTABLE` and `END_REFLECTABLE()` must appear once per reflected class, in that order.
- A reflected member macro must appear after `REFLECTABLE` and before `END_REFLECTABLE()`.
- The generated metadata uses a function-local static vector, initialized once in a thread-safe manner; member declarations themselves do not perform runtime registration.
- This initial core reflects explicitly registered data members; inherited-field enumeration and getter/setter properties are not yet automatic.
