#include <static_reflect/reflect.hpp>
#include <iostream>
#include <string>
#include <vector>

enum PersonTags : static_reflect::TagMask {
    Serializable = 1u << 0,
    Editable = 1u << 1,
    Hidden = 1u << 2
};

struct Person {
    REFLECTABLE(Person)

    REFLECT_MEMBER_INIT(int, age, 30, Serializable | Editable);

    // Deliberately consumes a counter value: gaps must be harmless.
    static constexpr int unrelated_counter_use = __COUNTER__;

    REFLECT_MEMBER_INIT_UNTAGGED(std::string, name, "Ada");
    REFLECT_MEMBER(std::vector<int>, scores, Serializable);

    END_REFLECTABLE();
};
REGISTER_REFLECT(Person)

int main() {
    Person person;
    const auto& meta = static_reflect::type<Person>();
    std::cout << meta.name << ": " << meta.size() << " fields\n";
    for (const auto& field : meta)
        std::cout << "  " << field.name << " tags=" << field.tags << '\n';

    if (const auto* age = meta.find_field("age"))
        age->set(person, 42);
    std::cout << "age = " << person.age << '\n';
}
