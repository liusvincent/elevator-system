#include "dispatcher.hpp"

#include <cstdlib>
#include <limits>
#include <stdexcept>

Dispatcher::Dispatcher(int num_elevators) {
  elevators.reserve(num_elevators);
  for (int i = 0; i < num_elevators; i++) {
    elevators.push_back(std::make_unique<Elevator>());
  }
}

void Dispatcher::addHallCall(int floor, Direction dir) {
  /*
  Simply add the hall calls into its rightful set (up or down calls)
  */
  if (floor <= 0 || floor > MAX_FLOOR)
    throw std::out_of_range("Invalid Floor Request: Out of Floor Range");
  if (dir == Direction::IDLE)
    throw std::invalid_argument(
        "Invalid Floor Request: Hall Call Can't be IDLE");
  else if (dir == Direction::UP)
    up_calls_.insert(floor);
  else if (dir == Direction::DOWN)
    down_calls_.insert(floor);

  dispatch(floor, dir); // assign the call to an elevator
}

void Dispatcher::removeHallCall(int floor, Direction dir) {
  if (dir == Direction::UP)
    up_calls_.erase(floor);
  else if (dir == Direction::DOWN)
    down_calls_.erase(floor);
  assigned_to_.erase({floor, dir});
}

int Dispatcher::cost(Elevator *elevator, int target_floor,
                     Direction call_dir) {
  /*
  Calculate and return how many floors it will take for that elevator to reach
  the target floor
  1. cost is calculated from how many floors it will take to reach
  target_floor
  */
  const int curr_floor = elevator->getCurrFloor();
  const Direction dir = elevator->getDirection();

  int cost = 0;

  if (dir == Direction::IDLE) {
    cost += std::abs(target_floor - curr_floor);
  } else if (dir == Direction::UP) {
    bool on_the_way =
        (call_dir == Direction::UP) && (target_floor >= curr_floor);
    if (!on_the_way) {
      int top;
      try {
        top = elevator->getFinalRequest(Direction::UP);
      } catch (const std::logic_error &) {
        top = curr_floor;
      }
      cost += (top - curr_floor) + (top - target_floor);
    } else {
      cost += (target_floor - curr_floor);
    }
  } else { // elevator->getDirection() == Direction::DOWN
    bool on_the_way =
        (call_dir == Direction::DOWN) && (target_floor <= curr_floor);
    if (!on_the_way) {
      int bottom;
      try {
        bottom = elevator->getFirstRequest(Direction::DOWN);
      } catch (const std::logic_error &) {
        bottom = curr_floor;
      }
      cost += (curr_floor - bottom) + (target_floor - bottom);
    } else {
      cost += (curr_floor - target_floor);
    }
  }

  // add other costs in the future
  return cost;
}

void Dispatcher::dispatch(int floor, Direction call_dir) {
  /*
  Find the most optimal elevator to assign the calls
  1. If the elevator is on its way (the closest elevator)
  2. If the elevator is IDLE
  */
  std::pair<int, Direction> key{floor, call_dir};
  if (assigned_to_.find(key) !=
      assigned_to_.end()) // if call is already assigned, ignore
    return;

  int best_idx = -1;
  int best_cost = std::numeric_limits<int>::max();

  for (std::size_t i = 0; i < elevators.size();
       i++) { // loop through each elevator to find best cost and best index
    Elevator *e = elevators[i].get();
    int c = cost(e, floor, call_dir);
    if (c < best_cost) {
      best_cost = c;
      best_idx = i;
    } else if (c == best_cost) { // if both elevators have same cost check if
                                 // either is IDLE
      if (best_idx != -1 &&
          elevators[best_idx]->getDirection() != Direction::IDLE &&
          e->getDirection() == Direction::IDLE) {
        best_idx = i;
      }
    }
  }

  if (best_idx == -1)
    return;

  elevators[best_idx]->addExternalCall(floor, call_dir);
  assigned_to_[key] = best_idx;
}
