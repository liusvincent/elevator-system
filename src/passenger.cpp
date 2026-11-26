#include "passenger.hpp"

#include <stdexcept>

Passenger::Passenger(PassengerId id, int originFloor, int destinationFloor,
                     std::uint64_t spawnedAtTick)
    : id_(id), originFloor_(originFloor), destinationFloor_(destinationFloor),
      spawnedAtTick_(spawnedAtTick) {
  if (originFloor < MIN_FLOOR || originFloor > MAX_FLOOR) {
    throw std::out_of_range("Passenger origin is outside the building");
  }
  if (destinationFloor < MIN_FLOOR || destinationFloor > MAX_FLOOR) {
    throw std::out_of_range("Passenger destination is outside the building");
  }
  if (originFloor == destinationFloor) {
    throw std::invalid_argument(
        "Passenger origin and destination must differ");
  }
}

PassengerId Passenger::id() const { return id_; }

int Passenger::originFloor() const { return originFloor_; }

int Passenger::destinationFloor() const { return destinationFloor_; }

Direction Passenger::direction() const {
  return destinationFloor_ > originFloor_ ? Direction::Up : Direction::Down;
}

PassengerState Passenger::state() const { return state_; }

std::uint64_t Passenger::spawnedAtTick() const { return spawnedAtTick_; }

std::optional<std::uint64_t> Passenger::boardedAtTick() const {
  return boardedAtTick_;
}

std::optional<std::uint64_t> Passenger::arrivedAtTick() const {
  return arrivedAtTick_;
}

void Passenger::board(std::uint64_t currentTick) {
  if (state_ != PassengerState::Waiting) {
    throw std::logic_error("Only a waiting passenger can board");
  }

  state_ = PassengerState::Riding;
  boardedAtTick_ = currentTick;
}

void Passenger::arrive(std::uint64_t currentTick) {
  if (state_ != PassengerState::Riding) {
    throw std::logic_error("Only a riding passenger can arrive");
  }

  state_ = PassengerState::Arrived;
  arrivedAtTick_ = currentTick;
}
