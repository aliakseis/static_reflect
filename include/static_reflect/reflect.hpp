#pragma once

// static_reflect: header-only, macro-assisted C++17 reflection metadata.
// Distributed declarations use __COUNTER__ to specialize a recursive descriptor
// chain. No per-field static registration or global linked list is involved.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <vector>

namespace static_reflect {

using TagMask = std::uint64_t;

struct Annotation {
    std::string_view key;
    std::string_view value;
};

struct FieldDescriptor {
    std::string_view name;
    std::type_index value_type;
    TagMask tags;
    void* (*get_mutable)(void* object);
    const void* (*get_const_fn)(const void* object);
    bool (*assign)(void* object, const void* value);
    const Annotation* annotations;
    std::size_t annotation_count;

    bool has(TagMask mask) const noexcept { return (tags & mask) == mask; }

    const Annotation* find_annotation(std::string_view key) const noexcept {
        for (std::size_t i = 0; i < annotation_count; ++i)
            if (annotations[i].key == key)
                return annotations + i;
        return nullptr;
    }

    template<class T, class C>
    T* get(C& object) const {
        static_assert(!std::is_const<C>::value,
                      "Use get_const() with a const object");
        if (value_type != std::type_index(typeid(T)) || !get_mutable)
            return nullptr;
        return static_cast<T*>(get_mutable(static_cast<void*>(&object)));
    }

    template<class T, class C>
    const T* get_const(const C& object) const {
        if (value_type != std::type_index(typeid(T)) || !get_const_fn)
            return nullptr;
        return static_cast<const T*>(get_const_fn(static_cast<const void*>(&object)));
    }

    template<class T, class C>
    bool set(C& object, const T& value) const {
        if (value_type != std::type_index(typeid(T)) || !assign)
            return false;
        return assign(static_cast<void*>(&object), static_cast<const void*>(&value));
    }
};

struct TypeDescriptor {
    std::string_view name;
    const FieldDescriptor* data;
    std::size_t field_count;

    const FieldDescriptor* begin() const noexcept { return data; }
    const FieldDescriptor* end() const noexcept { return field_count ? data + field_count : data; }
    std::size_t size() const noexcept { return field_count; }
    bool empty() const noexcept { return field_count == 0; }

    const FieldDescriptor* find_field(std::string_view field_name) const noexcept {
        for (const auto& field : *this)
            if (field.name == field_name)
                return &field;
        return nullptr;
    }
};

template<class> struct member_pointer_traits;
template<class C, class M>
struct member_pointer_traits<M C::*> {
    using class_type = C;
    using member_type = M;
};

// Owner is the class whose object is passed to the accessors. Member may be
// declared by Owner or one of its bases.
template<class Owner, auto Member>
FieldDescriptor make_field_for(std::string_view name, TagMask tags,
                               const Annotation* annotations = nullptr,
                               std::size_t annotation_count = 0) {
    using Traits = member_pointer_traits<decltype(Member)>;
    using M = typename Traits::member_type;
    using Value = typename std::remove_cv<M>::type;

    FieldDescriptor result{
        name,
        std::type_index(typeid(Value)),
        tags,
        [](void* object) -> void* {
            if constexpr (std::is_const<M>::value) {
                (void)object;
                return nullptr;
            } else {
                auto* instance = static_cast<Owner*>(object);
                return std::addressof(instance->*Member);
            }
        },
        [](const void* object) -> const void* {
            const auto* instance = static_cast<const Owner*>(object);
            return std::addressof(instance->*Member);
        },
        nullptr,
        annotations,
        annotation_count
    };

    if constexpr (!std::is_const<M>::value && std::is_copy_assignable<Value>::value) {
        result.assign = [](void* object, const void* value) -> bool {
            auto* instance = static_cast<Owner*>(object);
            instance->*Member = *static_cast<const Value*>(value);
            return true;
        };
    }
    return result;
}

template<auto Member>
FieldDescriptor make_field(std::string_view name, TagMask tags,
                           const Annotation* annotations = nullptr,
                           std::size_t annotation_count = 0) {
    using Owner = typename member_pointer_traits<decltype(Member)>::class_type;
    return make_field_for<Owner, Member>(name, tags, annotations, annotation_count);
}

template<class T>
struct reflected { static constexpr bool enabled = false; };

template<class T>
const TypeDescriptor& type() {
    static_assert(reflected<T>::enabled,
                  "Type has no static_reflect metadata; use REGISTER_REFLECT(T)");
    return reflected<T>::descriptor();
}

} // namespace static_reflect

#define STATIC_REFLECT_DETAIL_CAT2(a, b) a##b
#define STATIC_REFLECT_DETAIL_CAT(a, b) STATIC_REFLECT_DETAIL_CAT2(a, b)

// Each declaration specializes StaticReflectDescriptor at its relative counter
// position. The primary template is a no-op link, so unrelated __COUNTER__ uses
// create harmless gaps in the chain.
#define STATIC_REFLECT_DETAIL_FIELD(Member, TagBits, N) \
    template<int _sr_id> struct StaticReflectDescriptor< \
        _sr_id, (N) - static_reflect_counter_base> { \
        static void append(std::vector<::static_reflect::FieldDescriptor>& out) { \
            StaticReflectDescriptor<_sr_id - 1>::append(out); \
            out.push_back(::static_reflect::make_field_for<StaticReflectSelf, \
                &StaticReflectSelf::Member>(#Member, \
                static_cast<::static_reflect::TagMask>(TagBits))); \
        } \
    }

#define STATIC_REFLECT_DETAIL_FIELD_ANNOTATED(Member, TagBits, Annotations, N) \
    template<int _sr_id> struct StaticReflectDescriptor< \
        _sr_id, (N) - static_reflect_counter_base> { \
        static void append(std::vector<::static_reflect::FieldDescriptor>& out) { \
            StaticReflectDescriptor<_sr_id - 1>::append(out); \
            out.push_back(::static_reflect::make_field_for<StaticReflectSelf, \
                &StaticReflectSelf::Member>(#Member, \
                static_cast<::static_reflect::TagMask>(TagBits), \
                StaticReflectSelf::AnnotationArray, \
                sizeof(StaticReflectSelf::AnnotationArray) / sizeof(::static_reflect::Annotation))); \
        } \
    }

// Put REFLECTABLE at the beginning of a class and END_REFLECTABLE at its end.
// REFLECT_MEMBER declares the data member itself, keeping declaration and tags
// together. The END macro captures the last counter value for this class.
#define REFLECTABLE(TypeName) \
public: \
    using StaticReflectSelf = TypeName; \
    static constexpr int static_reflect_counter_base = __COUNTER__; \
    static constexpr std::string_view static_reflect_type_name = #TypeName; \
    template<int _sr_id, int _sr_slot = _sr_id> struct StaticReflectDescriptor { \
        static void append(std::vector<::static_reflect::FieldDescriptor>& out) { \
            StaticReflectDescriptor<_sr_id - 1>::append(out); \
        } \
    }; \
    template<int _sr_id> struct StaticReflectDescriptor<_sr_id, 0> { \
        static void append(std::vector<::static_reflect::FieldDescriptor>&) {} \
    };

#define REFLECT_MEMBER(Type, Member, TagBits) \
    Type Member; \
    STATIC_REFLECT_DETAIL_FIELD(Member, TagBits, __COUNTER__)

#define REFLECT_MEMBER_INIT(Type, Member, Initializer, TagBits) \
    Type Member = Initializer; \
    STATIC_REFLECT_DETAIL_FIELD(Member, TagBits, __COUNTER__)

#define REFLECT_MEMBER_UNTAGGED(Type, Member) \
    REFLECT_MEMBER(Type, Member, 0)

#define REFLECT_MEMBER_INIT_UNTAGGED(Type, Member, Initializer) \
    REFLECT_MEMBER_INIT(Type, Member, Initializer, 0)

// Optional annotation form. Declare a static array named Annotations before this
// macro, e.g. inline static constexpr Annotation name_meta[] = {{"label", "Name"}}.
// This helper takes the annotation-array identifier as an explicit argument.
#define REFLECT_MEMBER_WITH_ANNOTATIONS(Type, Member, Initializer, TagBits, AnnotationArray) \
    Type Member = Initializer; \
    template<int _sr_id> struct StaticReflectDescriptor< \
        _sr_id, __COUNTER__ - static_reflect_counter_base> { \
        static void append(std::vector<::static_reflect::FieldDescriptor>& out) { \
            StaticReflectDescriptor<_sr_id - 1>::append(out); \
            out.push_back(::static_reflect::make_field_for<StaticReflectSelf, \
                &StaticReflectSelf::Member>(#Member, \
                static_cast<::static_reflect::TagMask>(TagBits), \
                StaticReflectSelf::AnnotationArray, \
                sizeof(StaticReflectSelf::AnnotationArray) / sizeof(::static_reflect::Annotation))); \
        } \
    }

#define END_REFLECTABLE() \
    static constexpr int static_reflect_counter_end = __COUNTER__; \
    static constexpr int static_reflect_field_limit = \
        static_reflect_counter_end - static_reflect_counter_base - 1; \
    static const ::static_reflect::TypeDescriptor& static_reflect_descriptor() { \
        static const std::vector<::static_reflect::FieldDescriptor> fields = [] { \
            std::vector<::static_reflect::FieldDescriptor> result; \
            result.reserve(static_cast<std::size_t>(static_reflect_field_limit)); \
            StaticReflectDescriptor<static_reflect_field_limit>::append(result); \
            return result; \
        }(); \
        static const ::static_reflect::TypeDescriptor descriptor{ \
            static_reflect_type_name, fields.data(), fields.size()}; \
        return descriptor; \
    }

// Legacy centralized-list API is retained for convenience.
#define BEGIN_REFLECT(TypeName) \
public: \
    static const ::static_reflect::TypeDescriptor& static_reflect_descriptor() { \
        using StaticReflectSelf = TypeName; \
        static constexpr std::string_view static_reflect_name = #TypeName; \
        static const std::vector<::static_reflect::FieldDescriptor> static_reflect_fields = {

#define REFLECT_FIELD(Member, TagBits) \
    ::static_reflect::make_field<&StaticReflectSelf::Member>(#Member, \
        static_cast<::static_reflect::TagMask>(TagBits)),
#define REFLECT_FIELD_UNTAGGED(Member) REFLECT_FIELD(Member, 0)
#define REFLECT_FIELD_ANNOTATED(Member, TagBits, AnnotationArray) \
    ::static_reflect::make_field<&StaticReflectSelf::Member>(#Member, \
        static_cast<::static_reflect::TagMask>(TagBits), (AnnotationArray).data(), \
        (AnnotationArray).size()),
#define END_REFLECT() \
        }; \
        static const ::static_reflect::TypeDescriptor static_reflect_type{ \
            static_reflect_name, static_reflect_fields.data(), static_reflect_fields.size()}; \
        return static_reflect_type; \
    }

#define REGISTER_REFLECT(TypeName) \
    namespace static_reflect { template<> struct reflected<TypeName> { \
        static constexpr bool enabled = true; \
        static const ::static_reflect::TypeDescriptor& descriptor() { \
            return TypeName::static_reflect_descriptor(); \
        } \
    }; }
