#include "dispatcher.hpp"

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

void testRejectsZeroElevators() {
  bool threwExpectedException = false;

  try {
    Dispatcher dispatcher{0};
  } catch (const std::invalid_argument &) {
    threwExpectedException = true;
  }

  expect(threwExpectedException,
         "Dispatcher should reject an empty elevator fleet");
}

void testRejectsNegativeElevatorCount() {
  bool threwExpectedException = false;

  try {
    Dispatcher dispatcher{-1};
  } catch (const std::invalid_argument &) {
    threwExpectedException = true;
  }

  expect(threwExpectedException,
         "Dispatcher should reject a negative elevator count");
}

void testAssignsEqualCostCallToLowestElevatorId() {
  Dispatcher dispatcher{3};
  const HallCall call{3, Direction::Up};

  dispatcher.addHallCall(call);

  expect(dispatcher.assignedElevator(call) == ElevatorId{0},
         "Equal-cost call should be assigned to elevator 0");
}

void testRejectsHallCallOutsideBuilding() {
  Dispatcher dispatcher{1};
  bool threwExpectedException = false;

  try {
    dispatcher.addHallCall({MAX_FLOOR + 1, Direction::Up});
  } catch (const std::out_of_range &) {
    threwExpectedException = true;
  }

  expect(threwExpectedException,
         "Dispatcher should reject a hall call outside the building");
}

void testRejectsIdleHallCall() {
  Dispatcher dispatcher{1};
  bool threwExpectedException = false;

  try {
    dispatcher.addHallCall({MIN_FLOOR, Direction::Idle});
  } catch (const std::invalid_argument &) {
    threwExpectedException = true;
  }

  expect(threwExpectedException,
         "Dispatcher should reject an idle hall-call direction");
}

void testDeduplicatesHallCalls() {
  Dispatcher dispatcher{1};
  const HallCall call{3, Direction::Up};

  dispatcher.addHallCall(call);
  dispatcher.addHallCall(call);

  int matchingEvents = 0;

  for (int step = 0; step < 20; ++step) {
    for (const DispatcherServiceEvent &event : dispatcher.advance()) {
      if (event.service.floor == call.floor &&
          event.service.servedHallCall == call.direction) {
        ++matchingEvents;
      }
    }
  }

  expect(matchingEvents == 1,
         "Duplicate hall calls should produce one service event");
}

void testChoosesElevatorWithShortestSimulatedRoute() {
  Dispatcher dispatcher{2};

  dispatcher.selectFloor(0, 5);
  dispatcher.advance();

  const HallCall call{4, Direction::Up};
  dispatcher.addHallCall(call);

  expect(dispatcher.assignedElevator(call) == ElevatorId{0},
         "Moving elevator should receive an on-route hall call");
}

void testAdvancesEveryElevator() {
  Dispatcher dispatcher{2};

  dispatcher.selectFloor(0, 3);
  dispatcher.selectFloor(1, 3);
  dispatcher.advance();

  expect(dispatcher.elevatorSnapshot(0).currentFloor == 2,
         "First elevator should advance");
  expect(dispatcher.elevatorSnapshot(1).currentFloor == 2,
         "Second elevator should advance");
}

void testTagsServiceEventWithElevatorId() {
  Dispatcher dispatcher{2};
  dispatcher.selectFloor(1, MIN_FLOOR);

  const std::vector<DispatcherServiceEvent> events = dispatcher.advance();

  expect(events.size() == 1, "Expected one service event");
  expect(events.front().elevatorId == ElevatorId{1},
         "Service event should identify elevator 1");
  expect(events.front().service.servedInternalRequest,
         "Service event should identify the internal request");
}

void testClearsCompletedAssignment() {
  Dispatcher dispatcher{1};
  const HallCall call{MIN_FLOOR, Direction::Up};

  dispatcher.addHallCall(call);
  expect(dispatcher.assignedElevator(call).has_value(),
         "Hall call should be assigned");

  dispatcher.advance();

  expect(!dispatcher.assignedElevator(call).has_value(),
         "Completed hall call assignment should be removed");
}

void testAllowsHallCallAgainAfterCompletion() {
  Dispatcher dispatcher{1};
  const HallCall call{MIN_FLOOR, Direction::Up};

  dispatcher.addHallCall(call);
  dispatcher.advance();
  dispatcher.addHallCall(call);

  expect(dispatcher.assignedElevator(call) == ElevatorId{0},
         "Completed hall call should be assignable again");
}

void testRejectsInvalidElevatorId() {
  Dispatcher dispatcher{1};
  bool threwExpectedException = false;

  try {
    dispatcher.selectFloor(1, 3);
  } catch (const std::out_of_range &) {
    threwExpectedException = true;
  }

  expect(threwExpectedException,
         "Selecting a floor for an unknown elevator should fail");
}

void testForwardsInternalFloorSelection() {
  Dispatcher dispatcher{1};

  dispatcher.selectFloor(0, 3);
  dispatcher.advance();

  const ElevatorSnapshot snapshot = dispatcher.elevatorSnapshot(0);
  expect(snapshot.currentFloor == 2,
         "Internal selection should be forwarded to the elevator");
  expect(snapshot.hasPendingRequests,
         "Internal request should remain pending until serviced");
}

} // namespace

int main() {
  int failures = 0;

  failures += !runTest("rejects zero elevators", testRejectsZeroElevators);
  failures += !runTest("rejects negative elevator count",
                       testRejectsNegativeElevatorCount);
  failures += !runTest("assigns equal-cost call to lowest elevator id",
                       testAssignsEqualCostCallToLowestElevatorId);
  failures += !runTest("rejects hall call outside building",
                       testRejectsHallCallOutsideBuilding);
  failures += !runTest("rejects idle hall call", testRejectsIdleHallCall);
  failures +=
      !runTest("deduplicates hall calls", testDeduplicatesHallCalls);
  failures += !runTest("chooses elevator with shortest simulated route",
                       testChoosesElevatorWithShortestSimulatedRoute);
  failures +=
      !runTest("advances every elevator", testAdvancesEveryElevator);
  failures += !runTest("tags service event with elevator id",
                       testTagsServiceEventWithElevatorId);
  failures += !runTest("clears completed assignment",
                       testClearsCompletedAssignment);
  failures += !runTest("allows hall call again after completion",
                       testAllowsHallCallAgainAfterCompletion);
  failures += !runTest("rejects invalid elevator id",
                       testRejectsInvalidElevatorId);
  failures += !runTest("forwards internal floor selection",
                       testForwardsInternalFloorSelection);

  if (failures == 0) {
    std::cout << "All dispatcher tests passed\n";
    return 0;
  }

  std::cerr << failures << " dispatcher test(s) failed\n";
  return 1;
}
