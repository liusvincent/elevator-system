#include "elevator.hpp"
#include "simulation_timing.hpp"

#include <cmath>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

void expect(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

void expectNear(float actual, float expected, std::string_view message) {
  if (std::abs(actual - expected) > 0.0001F) {
    throw std::runtime_error(std::string(message));
  }
}

void tick(Elevator &elevator, int count) {
  for (int i = 0; i < count; ++i) {
    elevator.tick();
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
  expectNear(snapshot.stateProgress, 0.0F,
             "Idle elevator should have zero progress");
  expect(!snapshot.hasPendingRequests,
         "Elevator should start without pending requests");
}

void testBeginsMovingWithoutChangingFloor() {
  Elevator elevator;
  elevator.addInternalRequest(3);

  const std::optional<ServiceEvent> event = elevator.tick();
  const ElevatorSnapshot snapshot = elevator.snapshot();

  expect(!event, "Beginning movement should not service the request");
  expect(snapshot.currentFloor == MIN_FLOOR,
         "Beginning movement should not change floors");
  expect(snapshot.direction == Direction::Up,
         "Elevator should travel upward");
  expect(snapshot.state == ElevatorState::Moving,
         "Elevator should enter the moving state");
  expectNear(snapshot.stateProgress, 0.0F,
             "New movement should begin at zero progress");
}

void testTravelsOneFloorInConfiguredDuration() {
  Elevator elevator;
  elevator.addInternalRequest(3);
  elevator.tick();

  tick(elevator, simulation_timing::TRAVEL_TICKS_PER_FLOOR - 1);

  expect(elevator.snapshot().currentFloor == MIN_FLOOR,
         "Elevator should not arrive before travel duration completes");

  const float expectedProgress =
      static_cast<float>(simulation_timing::TRAVEL_TICKS_PER_FLOOR - 1) /
      simulation_timing::TRAVEL_TICKS_PER_FLOOR;
  expectNear(elevator.snapshot().stateProgress, expectedProgress,
             "Movement progress should match elapsed travel ticks");

  const std::optional<ServiceEvent> event = elevator.tick();
  const ElevatorSnapshot snapshot = elevator.snapshot();

  expect(!event, "Floor 2 should not service the floor 3 request");
  expect(snapshot.currentFloor == MIN_FLOOR + 1,
         "Elevator should arrive after the configured travel duration");
  expect(snapshot.state == ElevatorState::Moving,
         "Elevator should continue toward floor 3");
  expectNear(snapshot.stateProgress, 0.0F,
             "The next movement segment should start at zero progress");
}

void testServicesRequestAtCurrentFloor() {
  Elevator elevator;
  elevator.addInternalRequest(MIN_FLOOR);

  const std::optional<ServiceEvent> event = elevator.tick();
  const ElevatorSnapshot snapshot = elevator.snapshot();

  expect(event.has_value(), "The current-floor request should be served");
  expect(event->floor == MIN_FLOOR,
         "The service event should contain the serviced floor");
  expect(event->servedInternalRequest,
         "The event should identify the internal request");
  expect(snapshot.currentFloor == MIN_FLOOR,
         "Elevator should not move for a current-floor request");
  expect(snapshot.state == ElevatorState::DoorOpening,
         "Servicing should begin door opening");
  expectNear(snapshot.stateProgress, 0.0F,
             "Door opening should begin at zero progress");
}

void testDoorPhasesUseConfiguredDurations() {
  Elevator elevator;
  elevator.addInternalRequest(MIN_FLOOR);
  elevator.tick();

  tick(elevator, simulation_timing::DOOR_OPENING_TICKS - 1);
  expect(elevator.snapshot().state == ElevatorState::DoorOpening,
         "Doors should remain opening until the configured duration");
  expect(elevator.snapshot().currentFloor == MIN_FLOOR,
         "Elevator should remain stationary while doors open");

  elevator.tick();
  expect(elevator.snapshot().state == ElevatorState::DoorOpen,
         "Doors should become open after the opening duration");
  expectNear(elevator.snapshot().stateProgress, 0.0F,
             "Door-open hold should begin at zero progress");

  tick(elevator, simulation_timing::DOOR_OPEN_TICKS);
  expect(elevator.snapshot().state == ElevatorState::DoorClosing,
         "Doors should begin closing after the open duration");

  tick(elevator, simulation_timing::DOOR_CLOSING_TICKS);
  expect(elevator.snapshot().state == ElevatorState::Idle,
         "Elevator should become idle after doors finish closing");
  expect(elevator.snapshot().currentFloor == MIN_FLOOR,
         "Door phases should never move the elevator");
}

void testRejectsFloorAboveMaximum() {
  Elevator elevator;
  bool threwExpectedException = false;

  try {
    elevator.addInternalRequest(MAX_FLOOR + 1);
  } catch (const std::out_of_range &) {
    threwExpectedException = true;
  }

  expect(threwExpectedException,
         "Request above MAX_FLOOR should throw std::out_of_range");
}

void testEventuallyServicesOppositeDirectionCallAbove() {
  Elevator elevator;
  elevator.assignHallCall({MAX_FLOOR, Direction::Down});

  bool callWasServed = false;

  for (int ticks = 0; ticks < 2'000; ++ticks) {
    const std::optional<ServiceEvent> event = elevator.tick();

    if (event && event->floor == MAX_FLOOR &&
        event->servedHallCall == Direction::Down) {
      callWasServed = true;
      break;
    }
  }

  expect(callWasServed,
         "Elevator should eventually serve a DOWN call above it");
}

void testDefersOppositeDirectionCallUntilReturnTrip() {
  Elevator elevator;
  elevator.assignHallCall({3, Direction::Down});
  elevator.addInternalRequest(5);

  std::vector<ServiceEvent> events;

  for (int ticks = 0; ticks < 3'000 && events.size() < 2; ++ticks) {
    if (const std::optional<ServiceEvent> event = elevator.tick()) {
      events.push_back(*event);
    }
  }

  expect(events.size() == 2, "Expected two service events");
  expect(events[0].floor == 5 && events[0].servedInternalRequest,
         "Elevator should service floor 5 before reversing");
  expect(events[1].floor == 3 &&
             events[1].servedHallCall == Direction::Down,
         "Elevator should service the DOWN call on its return trip");
}

} // namespace

int main() {
  int failures = 0;

  failures += !runTest("starts idle at minimum floor",
                       testStartsIdleAtMinimumFloor);
  failures += !runTest("begins moving without changing floor",
                       testBeginsMovingWithoutChangingFloor);
  failures += !runTest("travels one floor in configured duration",
                       testTravelsOneFloorInConfiguredDuration);
  failures += !runTest("services request at current floor",
                       testServicesRequestAtCurrentFloor);
  failures += !runTest("door phases use configured durations",
                       testDoorPhasesUseConfiguredDurations);
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
