// SPDX-FileCopyrightText: 2018 Florent Rougon
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file
 * @brief Pointer traits classes
 */

#pragma once

#include <memory>
#include <utility>

#include <simgear/structure/SGSharedPtr.hxx>

namespace flightgear
{

namespace addons
{

template <typename T>
struct shared_ptr_traits;

template <typename T>
struct shared_ptr_traits<SGSharedPtr<T>>
{
  using element_type = T;
  using strong_ref = SGSharedPtr<T>;

  template <typename ...Args>
  static strong_ref makeStrongRef(Args&& ...args)
  {
    return strong_ref(new T(std::forward<Args>(args)...));
  }
};

template <typename T>
struct shared_ptr_traits<std::shared_ptr<T>>
{
  using element_type = T;
  using strong_ref = std::shared_ptr<T>;

  template <typename ...Args>
  static strong_ref makeStrongRef(Args&& ...args)
  {
    return std::make_shared<T>(std::forward<Args>(args)...);
  }
};

} // of namespace addons

} // namespace flightgear
