#include "passenger.hpp"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

void expect(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

template <typename TestFunction>
bool runTest(std::string_view name, TestFunction test) {
  try {
    test();
    std::cout << "[PASS] " << name << '\n';
    return true;
  } catch (const std::exception &error) {
    std::cerr << "[FAIL] " << name << ": " << error.what() << '\n';
    return false;
  } catch (...) {
    std::cerr << "[FAIL] " << name << ": unknown exception\n";
    return false;
  }
}

void testStartsWaitingWithTripInformation() {
  const Passenger passenger{7, 2, 6, 10};

  expect(passenger.id() == PassengerId{7}, "Passenger should retain its ID");
  expect(passenger.originFloor() == 2,
         "Passenger should retain its origin floor");
  expect(passenger.destinationFloor() == 6,
         "Passenger should retain its destination floor");
  expect(passenger.state() == PassengerState::Waiting,
         "Passenger should start waiting");
  expect(passenger.spawnedAtTick() == 10,
         "Passenger should retain its spawn tick");
  expect(!passenger.boardedAtTick().has_value(),
         "Waiting passenger should not have a boarding tick");
  expect(!passenger.arrivedAtTick().has_value(),
         "Waiting passenger should not have an arrival tick");
}

void testDerivesTripDirection() {
  const Passenger upward{0, 2, 7, 0};
  const Passenger downward{1, 7, 2, 0};

  expect(upward.direction() == Direction::Up,
         "Higher destination should produce an upward direction");
  expect(downward.direction() == Direction::Down,
         "Lower destination should produce a downward direction");
}

void testRejectsOriginOutsideBuilding() {
  bool threwExpectedException = false;

  try {
    Passenger passenger{0, MIN_FLOOR - 1, MAX_FLOOR, 0};
  } catch (const std::out_of_range &) {
    threwExpectedException = true;
  }

  expect(threwExpectedException,
         "Passenger should reject an origin outside the building");
}

void testRejectsDestinationOutsideBuilding() {
  bool threwExpectedException = false;

  try {
    Passenger passenger{0, MIN_FLOOR, MAX_FLOOR + 1, 0};
  } catch (const std::out_of_range &) {
    threwExpectedException = true;
  }

  expect(threwExpectedException,
         "Passenger should reject a destination outside the building");
}

void testRejectsTripToOriginFloor() {
  bool threwExpectedException = false;

  try {
    Passenger passenger{0, 4, 4, 0};
  } catch (const std::invalid_argument &) {
    threwExpectedException = true;
  }

  expect(threwExpectedException,
         "Passenger should reject identical origin and destination floors");
}

void testBoardsAndRecordsTick() {
  Passenger passenger{0, 1, 5, 3};

  passenger.board(12);

  expect(passenger.state() == PassengerState::Riding,
         "Boarded passenger should be riding");
  expect(passenger.boardedAtTick() == 12,
         "Boarded passenger should record the boarding tick");
}

void testArrivesAndRecordsTick() {
  Passenger passenger{0, 1, 5, 3};
  passenger.board(12);

  passenger.arrive(30);

  expect(passenger.state() == PassengerState::Arrived,
         "Passenger should be arrived after completing the trip");
  expect(passenger.arrivedAtTick() == 30,
         "Arrived passenger should record the arrival tick");
}

void testRejectsInvalidStateTransitions() {
  Passenger waitingPassenger{0, 1, 5, 0};
  bool rejectedEarlyArrival = false;

  try {
    waitingPassenger.arrive(1);
  } catch (const std::logic_error &) {
    rejectedEarlyArrival = true;
  }

  Passenger ridingPassenger{1, 1, 5, 0};
  ridingPassenger.board(1);
  bool rejectedSecondBoarding = false;

  try {
    ridingPassenger.board(2);
  } catch (const std::logic_error &) {
    rejectedSecondBoarding = true;
  }

  expect(rejectedEarlyArrival,
         "Waiting passenger should not arrive before boarding");
  expect(rejectedSecondBoarding,
         "Riding passenger should not board a second time");
}

} // namespace

int main() {
  int failures = 0;

  failures += !runTest("starts waiting with trip information",
                       testStartsWaitingWithTripInformation);
  failures += !runTest("derives trip direction", testDerivesTripDirection);
  failures += !runTest("rejects origin outside building",
                       testRejectsOriginOutsideBuilding);
  failures += !runTest("rejects destination outside building",
                       testRejectsDestinationOutsideBuilding);
  failures += !runTest("rejects trip to origin floor",
                       testRejectsTripToOriginFloor);
  failures += !runTest("boards and records tick", testBoardsAndRecordsTick);
  failures += !runTest("arrives and records tick", testArrivesAndRecordsTick);
  failures += !runTest("rejects invalid state transitions",
                       testRejectsInvalidStateTransitions);

  if (failures == 0) {
    std::cout << "All passenger tests passed.\n";
  }

  return failures == 0 ? 0 : 1;
}
