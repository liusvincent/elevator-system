#pragma once

#include <cstddef>
#include <optional>
#include <set>

inline constexpr int MAX_FLOOR = 8;
inline constexpr int MIN_FLOOR = 1;

enum class Direction { Down = -1, Idle = 0, Up = 1 };

enum class ElevatorState {
  Idle,
  Moving,
  DoorOpening,
  DoorOpen,
  DoorClosing
};

struct HallCall {
  int floor;
  Direction direction;
};

struct ServiceEvent {
  int floor;
  bool servedInternalRequest;
  std::optional<Direction> servedHallCall;
};

struct ElevatorSnapshot {
  int currentFloor;
  Direction direction;
  ElevatorState state;
  float stateProgress;
  bool hasPendingRequests;
};

class Elevator {
public:
  static constexpr std::size_t DEFAULT_CAPACITY = 6;

  explicit Elevator(std::size_t capacity = DEFAULT_CAPACITY);

  void addInternalRequest(int floor);
  void assignHallCall(HallCall call);

  std::optional<ServiceEvent> tick();

  ElevatorSnapshot snapshot() const;
  bool hasPendingRequests() const;
  std::size_t capacity() const;

private:
  std::optional<ServiceEvent> chooseNextAction();
  std::optional<ServiceEvent> advanceMovement();
  std::optional<ServiceEvent> advanceDoors();

  void beginService();
  void beginMoving();
  void chooseDirection();

  bool hasRequestAtCurrentFloor() const;
  bool hasRequestAbove() const;
  bool hasRequestBelow() const;
  bool hasRequestInCurrentDirection() const;
  float stateProgress() const;

  int currentFloor_;
  Direction direction_;
  ElevatorState state_;
  int stateTicksElapsed_;
  std::size_t capacity_;
  std::optional<ServiceEvent> serviceWhenDoorsOpen_;
  std::set<int> internalRequests_;
  std::set<int> upHallCalls_;
  std::set<int> downHallCalls_;
};
