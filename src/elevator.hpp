#pragma once

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
  Elevator();

  void addInternalRequest(int floor);
  void assignHallCall(HallCall call);

  std::optional<ServiceEvent> tick();

  ElevatorSnapshot snapshot() const;
  bool hasPendingRequests() const;

private:
  std::optional<ServiceEvent> chooseNextAction();
  std::optional<ServiceEvent> advanceMovement();
  std::optional<ServiceEvent> beginService();

  void beginMoving();
  void advanceDoors();
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
  std::set<int> internalRequests_;
  std::set<int> upHallCalls_;
  std::set<int> downHallCalls_;
};
