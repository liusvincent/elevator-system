#pragma once

#include <iosfwd>
#include <set>

inline constexpr int MAX_FLOOR = 5;
inline constexpr int MIN_FLOOR = 1;

enum class Direction { IDLE = 0, DOWN = -1, UP = 1 };

enum class State {
  DOOR_OPEN,
  DOOR_CLOSE,
  DOOR_OPENING,
  DOOR_CLOSING,
  STOPPED,
  MOVING
};

struct ServiceEvent {
  bool served;
  int floor;
  Direction direction;
  bool was_hall;
};

class Elevator {
  friend std::ostream &operator<<(std::ostream &os, const Elevator &elevator);

public:
  Elevator();

  int getCurrFloor() const;
  Direction getDirection() const;

  int getFirstRequest(Direction direction) const;
  int getFinalRequest(Direction direction) const;

  void addInternalRequest(int floor);
  void addExternalCall(int floor, Direction direction);
  void removeRequest(std::set<int> &input_set, int floor);

  ServiceEvent serviceCurrFloor();

  void checkClosest(const std::set<int> &input_set, int &closest,
                    int &closest_floor) const;

  Direction decideDirection();
  bool hasRequestAtCurrFloor() const;
  bool hasRequestAhead() const;
  ServiceEvent stepLOOK();

private:
  int curr_floor_;
  State state_;
  Direction direction_;
  std::set<int> internal_requests_;
  std::set<int> external_up_;
  std::set<int> external_down_;
};
