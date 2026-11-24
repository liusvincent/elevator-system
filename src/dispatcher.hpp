#pragma once

#include "elevator.hpp"

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

class Dispatcher {
public:
  explicit Dispatcher(int num_elevators);

  void addHallCall(int floor, Direction dir);
  void removeHallCall(int floor, Direction dir);

  int cost(Elevator *elevator, int target_floor, Direction call_dir);
  void dispatch(int floor, Direction call_dir);

private:
  std::set<int> up_calls_;
  std::set<int> down_calls_;
  std::vector<std::unique_ptr<Elevator>> elevators;
  std::map<std::pair<int, Direction>, int> assigned_to_;
};
