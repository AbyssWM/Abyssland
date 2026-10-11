#pragma once

#include <abyssutils/memory/WeakPtr.hpp>
#include <abyssutils/memory/Atomic.hpp>

using namespace Abyssutils::Memory;

template <typename T>
using SP = Abyssutils::Memory::CSharedPointer<T>;
template <typename T>
using WP = Abyssutils::Memory::CWeakPointer<T>;
template <typename T>
using UP = Abyssutils::Memory::CUniquePointer<T>;
template <typename T>
using ASP = Abyssutils::Memory::CAtomicSharedPointer<T>;
