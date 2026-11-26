#pragma once

#include "elevator.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

using PassengerId = std::size_t;

enum class PassengerState { Waiting, Riding, Arrived };

class Passenger {
public:
  Passenger(PassengerId id, int originFloor, int destinationFloor,
            std::uint64_t spawnedAtTick);

  PassengerId id() const;
  int originFloor() const;
  int destinationFloor() const;
  Direction direction() const;
  PassengerState state() const;

  std::uint64_t spawnedAtTick() const;
  std::optional<std::uint64_t> boardedAtTick() const;
  std::optional<std::uint64_t> arrivedAtTick() const;

  void board(std::uint64_t currentTick);
  void arrive(std::uint64_t currentTick);

private:
  PassengerId id_;
  int originFloor_;
  int destinationFloor_;
  PassengerState state_ = PassengerState::Waiting;
  std::uint64_t spawnedAtTick_;
  std::optional<std::uint64_t> boardedAtTick_;
  std::optional<std::uint64_t> arrivedAtTick_;
};
