#include <static_reflect/reflect.hpp>
#include <cassert>
#include <string>
#include <type_traits>

enum Tags : static_reflect::TagMask { Save = 1, Edit = 2, Secret = 4 };

struct Sample {
    REFLECTABLE(Sample)
    REFLECT_MEMBER_INIT(int, first, 1, Save | Edit);
    static constexpr int gap = __COUNTER__;
    REFLECT_MEMBER_INIT_UNTAGGED(std::string, second, "two");
    REFLECT_MEMBER_INIT(double, third, 3.5, Save);
    END_REFLECTABLE();
};
REGISTER_REFLECT(Sample)

struct Annotated {
    REFLECTABLE(Annotated)
    inline static constexpr static_reflect::Annotation label_meta[] = {
        {"label", "Display label"}, {"category", "General"}
    };
    REFLECT_MEMBER_WITH_ANNOTATIONS(std::string, label, "hello", Save, label_meta);
    END_REFLECTABLE();
};
REGISTER_REFLECT(Annotated)

struct Empty {
    REFLECTABLE(Empty)
    END_REFLECTABLE();
};
REGISTER_REFLECT(Empty)

struct Base {
    int base = 7;
};
struct Derived : Base {
    REFLECTABLE(Derived)
    REFLECT_MEMBER_INIT(int, own, 9, Save);
    END_REFLECTABLE();
};
REGISTER_REFLECT(Derived)

int main() {
    Sample s;
    const auto& t = static_reflect::type<Sample>();
    assert(t.name == "Sample");
    assert(t.size() == 3);
    assert(t.begin()[0].name == "first");
    assert(t.begin()[1].name == "second");
    assert(t.begin()[2].name == "third");
    assert(t.begin()[0].has(Save | Edit));
    assert(!t.begin()[1].has(Save));

    auto* first = t.find_field("first");
    assert(first && first->get<int>(s) && *first->get<int>(s) == 1);
    assert(first->set(s, 10));
    assert(s.first == 10);
    assert(!first->set(s, 1.0));
    assert(t.find_field("missing") == nullptr);

    const Sample& cs = s;
    assert(*t.find_field("second")->get_const<std::string>(cs) == "two");

    const auto& annotated = static_reflect::type<Annotated>();
    assert(annotated.size() == 1);
    const auto* label = annotated.find_field("label");
    assert(label && label->annotation_count == 2);
    assert(label->find_annotation("label"));
    assert(label->find_annotation("label")->value == "Display label");

    const auto& empty = static_reflect::type<Empty>();
    assert(empty.empty());

    Derived d;
    const auto& dt = static_reflect::type<Derived>();
    assert(dt.size() == 1);
    assert(dt.find_field("own")->set(d, 11));
    assert(d.own == 11);
}
