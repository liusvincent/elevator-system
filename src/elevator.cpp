#include "elevator.hpp"

#include <cstdlib>
#include <limits>
#include <ostream>
#include <stdexcept>

Elevator::Elevator()
    : curr_floor_(MIN_FLOOR), state_(State::DOOR_CLOSE),
      direction_(Direction::IDLE) {}

int Elevator::getCurrFloor() const { return curr_floor_; }

Direction Elevator::getDirection() const { return direction_; }

int Elevator::getFirstRequest(Direction direction) const {
  const std::set<int> *temp_set = nullptr;
  if (direction == Direction::UP)
    temp_set = &external_up_;
  else if (direction == Direction::DOWN)
    temp_set = &external_down_;
  if (temp_set == nullptr || temp_set->empty()) {
    throw std::logic_error("No requests in set");
  }
  return *temp_set->begin();
}

int Elevator::getFinalRequest(Direction direction) const {
  const std::set<int> *temp_set = nullptr;
  if (direction == Direction::UP)
    temp_set = &external_up_;
  else if (direction == Direction::DOWN)
    temp_set = &external_down_;
  if (temp_set == nullptr || temp_set->empty()) {
    throw std::logic_error("No requests in set");
  }
  return *temp_set->rbegin();
}

void Elevator::addInternalRequest(int floor) {
  // if (floor == curr_floor_) serviceCurrFloor(); open door not service floor
  // or do i handle in step look
  if (floor <= 0 || floor > MAX_FLOOR) {
    throw std::out_of_range("Invalid Floor Request: Out of Floor Range");
  }
  internal_requests_.insert(floor);
}

void Elevator::addExternalCall(int floor, Direction direction) {
  // if (floor == curr_floor_) serviceCurrFloor(); open door not service floor
  // or do i handle in step look
  if (floor <= 0 || floor > MAX_FLOOR) {
    throw std::out_of_range("Invalid Floor Request: Out of Floor Range");
  }
  if (direction == Direction::IDLE) {
    throw std::invalid_argument(
        "Invalid Floor Request: External Call Can't be IDLE");
  } else if (direction == Direction::UP) {
    external_up_.insert(floor);
  } else if (direction == Direction::DOWN) {
    external_down_.insert(floor);
  }
}

void Elevator::removeRequest(std::set<int> &input_set, int floor) {
  auto it = input_set.find(floor);
  if (it != input_set.end())
    input_set.erase(it);
}

ServiceEvent Elevator::serviceCurrFloor() {
  ServiceEvent event{false, curr_floor_, direction_, false};

  state_ = State::STOPPED;

  // remove request
  if (direction_ == Direction::UP && external_up_.count(curr_floor_)) {
    removeRequest(external_up_, curr_floor_);
    event.served = true;
    event.was_hall = true;
  }
  if (direction_ == Direction::DOWN && external_down_.count(curr_floor_)) {
    removeRequest(external_down_, curr_floor_);
    event.served = true;
    event.was_hall = true;
  }
  if (internal_requests_.count(curr_floor_)) {
    removeRequest(internal_requests_, curr_floor_);
    event.served = true;
  }

  // handleDoors()

  return event;
}

void Elevator::checkClosest(const std::set<int> &input_set, int &closest,
                            int &closest_floor) const {
  // to help decide direction
  for (int floor : input_set) {
    int distance = std::abs(curr_floor_ - floor);
    if (distance <= closest) {
      closest = distance;
      closest_floor = floor;
    }
  }
}

Direction Elevator::decideDirection() {
  /*
  Decide on direction by checking which call/request is the closest to current
  floor when idle
  */
  int closest_floor = -1;
  int closest = std::numeric_limits<int>::max();

  if (internal_requests_.empty() && external_down_.empty() &&
      external_up_.empty()) {
    return Direction::IDLE;
  } else {
    if (!internal_requests_.empty())
      checkClosest(internal_requests_, closest, closest_floor);
    if (!external_down_.empty())
      checkClosest(external_down_, closest, closest_floor);
    if (!external_up_.empty())
      checkClosest(external_up_, closest, closest_floor);
    if (closest_floor > curr_floor_)
      return Direction::UP;
    else if (closest_floor < curr_floor_)
      return Direction::DOWN;
    else {
      if (external_up_.count(closest_floor))
        return Direction::UP;
      if (external_down_.count(closest_floor))
        return Direction::DOWN;
      return Direction::IDLE;
    }
  }
}

bool Elevator::hasRequestAtCurrFloor() const {
  if (internal_requests_.count(curr_floor_))
    return true;
  if ((direction_ == Direction::UP || direction_ == Direction::IDLE) &&
      external_up_.count(curr_floor_))
    return true;
  if ((direction_ == Direction::DOWN || direction_ == Direction::IDLE) &&
      external_down_.count(curr_floor_))
    return true;
  return false;
}

bool Elevator::hasRequestAhead() const {
  if (direction_ == Direction::UP) {
    for (int floor : internal_requests_) {
      if (floor > curr_floor_)
        return true;
    }
    for (int floor : external_up_) {
      if (floor > curr_floor_)
        return true;
    }
  } else if (direction_ == Direction::DOWN) {
    for (int floor : internal_requests_) {
      if (floor < curr_floor_)
        return true;
    }
    for (int floor : external_down_) {
      if (floor < curr_floor_)
        return true;
    }
  }
  return false;
}

ServiceEvent Elevator::stepLOOK() {
  ServiceEvent event{false, curr_floor_, direction_, false};

  if (direction_ == Direction::IDLE) {
    direction_ = decideDirection();
  }
  if (hasRequestAtCurrFloor()) {
    event = serviceCurrFloor();
    return event;
  }
  if (hasRequestAhead()) {
    if (direction_ == Direction::UP) {
      if (curr_floor_ != MAX_FLOOR) {
        curr_floor_++;
        state_ = State::MOVING;
      }
    } else if (direction_ == Direction::DOWN) {
      if (curr_floor_ != MIN_FLOOR) {
        curr_floor_--;
        state_ = State::MOVING;
      }
    }
  } else {
    state_ = State::STOPPED;
    direction_ = Direction::IDLE;
  }

  return event;
}

std::ostream &operator<<(std::ostream &os, const Elevator &elevator) {
  // I just want to see a snapshot of the elevator to see if it's working
  os << "Direction: "
     << (elevator.direction_ == Direction::UP
             ? "UP"
             : (elevator.direction_ == Direction::DOWN ? "DOWN" : "IDLE"))
     << std::endl;
  os << "State: ";
  switch (elevator.state_) {
    using enum State;
  case (DOOR_OPEN):
    os << "DOOR_OPEN";
    break;
  case (DOOR_CLOSE):
    os << "DOOR_CLOSE";
    break;
  case (DOOR_OPENING):
    os << "DOOR_OPENING";
    break;
  case (DOOR_CLOSING):
    os << "DOOR_CLOSING";
    break;
  case (MOVING):
    os << "MOVING";
    break;
  case (STOPPED):
    os << "STOPPED";
    break;
  }
  os << std::endl;
  os << "Current Floor: " << elevator.curr_floor_ << std::endl;
  os << "Internal Request: ";
  for (const auto &i : elevator.internal_requests_) {
    os << i << " ";
  }
  os << std::endl;
  os << "External Up: ";
  for (const auto &i : elevator.external_up_) {
    os << i << " ";
  }
  os << std::endl;
  os << "External Down: ";
  for (const auto &i : elevator.external_down_) {
    os << i << " ";
  }
  os << std::endl;
  return os;
}
