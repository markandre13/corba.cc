#pragma once

#include "../../upstream/async.cc/src/async.hh"

namespace CORBA {

template <typename T = void>
using async = typename cppasync::async<T>;

template <typename K, typename V>
using interlock = typename cppasync::interlock<K, V>;

using signal = typename cppasync::signal;

}  // namespace CORBA
