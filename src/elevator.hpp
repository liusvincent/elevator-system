#pragma once

#include <optional>
#include <set>

inline constexpr int MAX_FLOOR = 5;
inline constexpr int MIN_FLOOR = 1;

enum class Direction { Down = -1, Idle = 0, Up = 1 };

enum class ElevatorState { Idle, Moving, DoorsOpen };

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
  bool hasPendingRequests;
};

class Elevator {
public:
  Elevator();

  void addInternalRequest(int floor);
  void assignHallCall(HallCall call);

  std::optional<ServiceEvent> step();

  ElevatorSnapshot snapshot() const;
  bool hasPendingRequests() const;

private:
  bool hasRequestAtCurrentFloor() const;
  bool hasRequestAbove() const;
  bool hasRequestBelow() const;
  bool hasRequestInCurrentDirection() const;

  void chooseDirection();
  void moveOneFloor();
  std::optional<ServiceEvent> serviceCurrentFloor();

  int currentFloor_;
  Direction direction_;
  ElevatorState state_;
  std::set<int> internalRequests_;
  std::set<int> upHallCalls_;
  std::set<int> downHallCalls_;
};
