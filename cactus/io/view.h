#pragma once

#include <cstddef>
#include <span>
#include <string_view>
#include <type_traits>
#include <algorithm>
#include <ranges>

namespace cactus {

/**
 * MutableView - задаёт невладеющую ссылку на непрерывный участок памяти.
 * Буфер задаётся адресом начала участка памяти и размером в байтах.
 *
 *     MutableView buf;
 *     std::byte* data = buf.data();
 *     size_t size = buf.size();
 *
 * У MutableView есть метод subspan(offset, count),
 * возвращающий MutableView на часть исходного буфера.
 * Его удобно использовать в циклах:
 *
 *     void ReadAll(MutableView buf) {
 *         while (buf.size()) {
 *             buf = buf.subspan(Read(buf));
 *         }
 *     }
 */
using MutableView = std::span<std::byte>;

/**
 * ConstView - аналог MutableView, запрещающий менять память, на которую ссылается.
 * Используется в случае, когда нужно передать участок памяти внутрь функции
 * только для чтения, например:
 *
 *     void Write(ConstView buf);
 *
 * Неявно конструируется из MutableView.
 */
using ConstView = std::span<const std::byte>;

template <class T>
concept IsPod = std::is_standard_layout_v<std::remove_reference_t<T>> &&
                std::is_trivially_copyable_v<std::remove_reference_t<T>> &&
                !std::ranges::contiguous_range<std::remove_reference_t<T>>;

template <class T>
concept IsMutablePod = IsPod<T> && !std::is_const_v<std::remove_reference_t<T>>;

template <class T>
concept IsConstPod = IsPod<T> && std::is_const_v<std::remove_reference_t<T>>;

template <class R>
using RangeRef = std::ranges::range_reference_t<R>;

template <class R>
concept IsMutableRange = std::ranges::contiguous_range<R> && IsMutablePod<RangeRef<R>>;

template <class R>
concept IsConstRange = std::ranges::contiguous_range<R> && IsConstPod<RangeRef<R>>;

/**
 * View - создаёт буфер, ссылающийся на объект.
 *
 *     int i;
 *     auto buf = View(i); // Создаёт MutableView ссылающийся на 4 байта переменной i.
 *
 *     std::vector<char> data(1024);
 *     auto buf = View(data); // Создаёт буфер ссылающийся на 1024 байта массива data.
 *
 *     struct Header {
 *         int ip;
 *         char flag;
 *     } __attribute__((packed)) header;
 *     auto buf = View(header);
 */
MutableView View(IsMutablePod auto&& pod) {
    return std::as_writable_bytes(std::span{std::addressof(pod), 1});
}

MutableView View(IsMutableRange auto&& range) {
    return std::as_writable_bytes(std::span{std::ranges::data(range), std::ranges::size(range)});
}

/**
 * View перегружен для const объектов.
 *
 *     const int i = 42;
 *     auto buf = View(i); // buf имеет тип ConstView.
 */
ConstView View(IsConstPod auto&& pod) {
    return std::as_bytes(std::span{std::addressof(pod), 1});
}

ConstView View(IsConstRange auto&& range) {
    return std::as_bytes(std::span{std::ranges::data(range), std::ranges::size(range)});
}

/**
 * View можно создать из пары (указатель, длина).
 */
MutableView View(IsMutablePod auto* data, size_t size) {
    return std::as_writable_bytes(std::span{data, size});
}

/**
 * View создаст ConstView, если ему передать указатель на const.
 */
ConstView View(IsConstPod auto* data, size_t size) {
    return std::as_bytes(std::span{data, size});
}

/**
 * Copy копирует содержимое in в out.
 *
 * Функция возвращает количество скопированых байт, равное минимуму из in.size() и out.size().
 */
inline size_t Copy(MutableView out, ConstView in) {
    auto n = std::min(out.size(), in.size());
    std::ranges::copy(in.first(n), out.begin());
    return n;
}

}  // namespace cactus
