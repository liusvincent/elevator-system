#include "elevator.hpp"

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

void testStartsIdleAtMinimumFloor() {
  Elevator elevator;
  const ElevatorSnapshot snapshot = elevator.snapshot();

  expect(snapshot.currentFloor == MIN_FLOOR,
         "Elevator should start at the minimum floor");

  expect(snapshot.direction == Direction::Idle,
         "Elevator should start idle");

  expect(snapshot.state == ElevatorState::Idle,
         "Elevator should start in the idle state");

  expect(!snapshot.hasPendingRequests,
         "Elevator should start without pending requests");
}

void testMovesTowardInternalRequest() {
  Elevator elevator;
  elevator.addInternalRequest(3);

  const std::optional<ServiceEvent> event = elevator.step();
  const ElevatorSnapshot snapshot = elevator.snapshot();

  expect(snapshot.currentFloor == 2,
         "Elevator should move one floor upward");

  expect(snapshot.direction == Direction::Up,
         "Elevator should travel upward");

  expect(!event,
         "The request should not be served before reaching floor 3");
}

void testServicesRequestAtCurrentFloor() {
  Elevator elevator;
  elevator.addInternalRequest(MIN_FLOOR);

  const std::optional<ServiceEvent> event = elevator.step();

  expect(event.has_value(), "The current-floor request should be served");

  expect(event->floor == MIN_FLOOR,
         "The service event should contain the serviced floor");

  expect(event->servedInternalRequest,
         "The event should identify the internal request");

  expect(elevator.snapshot().currentFloor == MIN_FLOOR,
         "Elevator should not move for a current-floor request");
}

void testRejectsFloorAboveMaximum() {
  Elevator elevator;

  bool threw_expected_exception = false;

  try {
    elevator.addInternalRequest(MAX_FLOOR + 1);
  } catch (const std::out_of_range &) {
    threw_expected_exception = true;
  }

  expect(threw_expected_exception,
         "Request above MAX_FLOOR should throw std::out_of_range");
}

void testEventuallyServicesOppositeDirectionCallAbove() {
  Elevator elevator;
  elevator.assignHallCall({MAX_FLOOR, Direction::Down});

  bool call_was_served = false;

  for (int step = 0; step < 20; ++step) {
    const std::optional<ServiceEvent> event = elevator.step();

    if (event && event->floor == MAX_FLOOR &&
        event->servedHallCall == Direction::Down) {
      call_was_served = true;
      break;
    }
  }

  expect(call_was_served,
         "Elevator should eventually serve a DOWN call above it");
}

void testDefersOppositeDirectionCallUntilReturnTrip() {
  Elevator elevator;
  elevator.assignHallCall({3, Direction::Down});
  elevator.addInternalRequest(5);

  elevator.step();
  elevator.step();

  expect(elevator.snapshot().currentFloor == 3,
         "Elevator should reach the opposite-direction call floor");

  const std::optional<ServiceEvent> ascendingEvent = elevator.step();

  expect(!ascendingEvent,
         "Elevator should not serve a DOWN call while traveling upward");
  expect(elevator.snapshot().currentFloor == 4,
         "Elevator should continue toward the request above");

  bool downCallWasServed = false;
  for (int step = 0; step < 10; ++step) {
    const std::optional<ServiceEvent> event = elevator.step();
    if (event && event->floor == 3 &&
        event->servedHallCall == Direction::Down) {
      downCallWasServed = true;
      break;
    }
  }

  expect(downCallWasServed,
         "Elevator should serve the DOWN call on its return trip");
}

} // namespace

int main() {
  int failures = 0;

  failures += !runTest("starts idle at minimum floor",
                       testStartsIdleAtMinimumFloor);

  failures += !runTest("moves toward internal request",
                       testMovesTowardInternalRequest);

  failures += !runTest("services request at current floor",
                       testServicesRequestAtCurrentFloor);

  failures += !runTest("rejects floor above maximum",
                       testRejectsFloorAboveMaximum);

  failures += !runTest("eventually services opposite-direction call above",
                       testEventuallyServicesOppositeDirectionCallAbove);

  failures += !runTest("defers opposite-direction call until return trip",
                       testDefersOppositeDirectionCallUntilReturnTrip);

  if (failures == 0) {
    std::cout << "All elevator tests passed\n";
    return 0;
  }

  std::cerr << failures << " elevator test(s) failed\n";
  return 1;
}
