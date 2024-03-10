#pragma once

#include "../../upstream/cpptask/src/async.hh"

namespace CORBA {

template <typename T = void>
using async = typename cppasync::async<T>;

template <typename K, typename V>
using interlock = typename cppasync::interlock<K, V>;

using signal = typename cppasync::signal;

}  // namespace CORBA
