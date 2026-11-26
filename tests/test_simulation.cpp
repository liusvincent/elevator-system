#include "simulation.hpp"
#include "simulation_timing.hpp"

#include <cmath>
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

void testStartsAtTickZeroWithEntireFleet() {
  const Simulation simulation{3};
  const SimulationSnapshot snapshot = simulation.snapshot();

  expect(snapshot.currentTick == 0, "Simulation should start at tick zero");
  expect(snapshot.elevators.size() == 3,
         "Snapshot should contain every elevator");

  for (const ElevatorSnapshot &elevator : snapshot.elevators) {
    expect(elevator.currentFloor == MIN_FLOOR,
           "Every elevator should start at the bottom floor");
    expect(elevator.state == ElevatorState::Idle,
           "Every elevator should start idle");
  }
}

void testTickAdvancesWorldCounter() {
  Simulation simulation{1};

  simulation.tick();

  expect(simulation.snapshot().currentTick == 1,
         "One simulation tick should advance the world counter once");
}

void testForwardsInternalRequest() {
  Simulation simulation{1};

  simulation.selectFloor(0, 3);
  simulation.tick();

  const ElevatorSnapshot elevator = simulation.snapshot().elevators.at(0);
  expect(elevator.state == ElevatorState::Moving,
         "Internal request should start the elevator moving");
  expect(elevator.currentFloor == MIN_FLOOR,
         "One tick should not move the elevator a full floor");
  expect(elevator.hasPendingRequests,
         "Internal request should remain pending while travelling");
}

void testAssignsHallCall() {
  Simulation simulation{1};
  const HallCall call{3, Direction::Up};

  simulation.addHallCall(call);

  const SimulationSnapshot snapshot = simulation.snapshot();

  expect(snapshot.hallCallAssignments.size() == 1,
         "Snapshot should contain the active hall-call assignment");
  expect(snapshot.assignedElevator(call) == ElevatorId{0},
         "Snapshot should expose the assigned elevator");
}

void testSpawnsPassengerAndCreatesHallCall() {
  Simulation simulation{1};

  const PassengerId passengerId = simulation.spawnPassenger(2, 6);
  const SimulationSnapshot snapshot = simulation.snapshot();
  const PassengerSnapshot &passenger = snapshot.passengers.at(passengerId);

  expect(passenger.id == passengerId,
         "Spawned passenger should be present in the snapshot");
  expect(passenger.state == PassengerState::Waiting,
         "Spawned passenger should start waiting");
  expect(passenger.originFloor == 2 && passenger.destinationFloor == 6,
         "Spawned passenger should retain its trip");
  expect(snapshot.assignedElevator({2, Direction::Up}) == ElevatorId{0},
         "Spawned passenger should create the correct hall call");
}

void testPassengerWaitsUntilDoorsOpen() {
  Simulation simulation{1};
  const PassengerId passengerId =
      simulation.spawnPassenger(MIN_FLOOR, 4);

  simulation.tick();

  const SimulationSnapshot snapshot = simulation.snapshot();
  expect(snapshot.elevators.at(0).state == ElevatorState::DoorOpening,
         "Elevator should begin opening its doors");
  expect(snapshot.passengers.at(passengerId).state == PassengerState::Waiting,
         "Passenger should wait until the doors are fully open");
}

void testPassengerCompletesJourney() {
  Simulation simulation{1};
  const PassengerId passengerId =
      simulation.spawnPassenger(MIN_FLOOR, 5);
  bool observedRiding = false;
  bool observedDestinationRequest = false;

  for (int tick = 0; tick < 2'000; ++tick) {
    simulation.tick();
    const SimulationSnapshot snapshot = simulation.snapshot();
    const PassengerSnapshot &passenger = snapshot.passengers.at(passengerId);

    if (passenger.state == PassengerState::Riding) {
      observedRiding = true;
      expect(passenger.elevatorId == ElevatorId{0},
             "Riding passenger should identify their elevator");
      expect(passenger.boardedAtTick.has_value(),
             "Riding passenger should record their boarding tick");
      observedDestinationRequest |=
          snapshot.elevators.at(0).hasPendingRequests;
    }

    if (passenger.state == PassengerState::Arrived) {
      expect(passenger.arrivedAtTick.has_value(),
             "Arrived passenger should record their arrival tick");
      expect(passenger.boardedAtTick < passenger.arrivedAtTick,
             "Passenger should arrive after boarding");
      expect(!passenger.elevatorId.has_value(),
             "Arrived passenger should no longer be inside an elevator");

      expect(observedRiding,
             "Passenger journey should include the riding state");
      expect(observedDestinationRequest,
             "Boarding should create an internal destination request");
      return;
    }
  }

  throw std::runtime_error(
      "Passenger did not complete the journey within the tick limit");
}

void testElevatorCapacityAndRemainingQueue() {
  Simulation simulation{1};
  constexpr int EXTRA_PASSENGERS = 2;
  constexpr std::size_t CAPACITY = DEFAULT_CAPACITY;

  for (std::size_t i = 0; i < CAPACITY + EXTRA_PASSENGERS; ++i) {
    simulation.spawnPassenger(MIN_FLOOR, 5);
  }

  bool observedFullElevator = false;
  bool observedReactivatedHallCall = false;

  for (int tick = 0; tick < 5'000; ++tick) {
    simulation.tick();
    const SimulationSnapshot snapshot = simulation.snapshot();

    std::size_t waiting = 0;
    std::size_t riding = 0;
    std::size_t arrived = 0;

    for (const PassengerSnapshot &passenger : snapshot.passengers) {
      switch (passenger.state) {
      case PassengerState::Waiting:
        ++waiting;
        break;
      case PassengerState::Riding:
        ++riding;
        break;
      case PassengerState::Arrived:
        ++arrived;
        break;
      }
    }

    if (!observedFullElevator && riding > 0) {
      expect(riding == CAPACITY,
             "Elevator should board passengers only up to capacity");
      expect(waiting == EXTRA_PASSENGERS,
             "Passengers above capacity should remain waiting");
      expect(!snapshot
                  .assignedElevator({MIN_FLOOR, Direction::Up})
                  .has_value(),
             "Remaining passengers should wait for the full elevator to "
             "depart before requesting another");
      observedFullElevator = true;
    }

    if (observedFullElevator &&
        snapshot.assignedElevator({MIN_FLOOR, Direction::Up}).has_value()) {
      observedReactivatedHallCall = true;
    }

    if (arrived == CAPACITY + EXTRA_PASSENGERS) {
      expect(observedFullElevator,
             "Scenario should exercise the elevator capacity limit");
      expect(observedReactivatedHallCall,
             "Remaining passengers should reactivate their hall call");
      return;
    }
  }

  throw std::runtime_error(
      "Capacity scenario did not finish within the tick limit");
}

void testPassengerArrivingAtOpenDoorsWaitsForDeparture() {
  Simulation simulation{1};
  const PassengerId firstPassenger =
      simulation.spawnPassenger(MIN_FLOOR, 5);

  while (simulation.snapshot().passengers.at(firstPassenger).state !=
         PassengerState::Riding) {
    simulation.tick();
  }

  const PassengerId latePassenger =
      simulation.spawnPassenger(MIN_FLOOR, 4);
  expect(!simulation.snapshot()
              .assignedElevator({MIN_FLOOR, Direction::Up})
              .has_value(),
         "A passenger arriving at open doors should wait for departure");

  for (int tick = 0; tick < 1'000; ++tick) {
    simulation.tick();
    const SimulationSnapshot snapshot = simulation.snapshot();

    if (snapshot.assignedElevator({MIN_FLOOR, Direction::Up}).has_value()) {
      expect(snapshot.elevators.at(0).state == ElevatorState::Moving,
             "The next hall call should be made only after departure");
      expect(snapshot.passengers.at(latePassenger).state ==
                 PassengerState::Waiting,
             "The late passenger should remain in the waiting queue");
      return;
    }
  }

  throw std::runtime_error(
      "Late passenger did not request another elevator after departure");
}

void testPropagatesServiceEvent() {
  Simulation simulation{1};
  const HallCall call{MIN_FLOOR, Direction::Up};

  simulation.addHallCall(call);
  expect(simulation.tick().empty(),
         "Service event should wait until the doors open");
  expect(simulation.snapshot().assignedElevator(call).has_value(),
         "Hall call should remain assigned while doors open");

  for (int tick = 0; tick < simulation_timing::DOOR_OPENING_TICKS - 1;
       ++tick) {
    simulation.tick();
  }
  const std::vector<DispatcherServiceEvent> events = simulation.tick();

  expect(events.size() == 1,
         "Servicing the current floor should produce one event");
  expect(events.front().elevatorId == ElevatorId{0},
         "Service event should identify the elevator");
  expect(events.front().service.floor == MIN_FLOOR,
         "Service event should identify the serviced floor");
  expect(events.front().service.servedHallCall == Direction::Up,
         "Service event should identify the hall-call direction");
  expect(!simulation.snapshot().assignedElevator(call).has_value(),
         "Serviced hall call should no longer be assigned");
}

void testEmptyHallCallDoesNotBlockFuturePassenger() {
  Simulation simulation{1};
  const HallCall call{MIN_FLOOR, Direction::Up};

  simulation.addHallCall(call);
  while (simulation.snapshot().elevators.at(0).state !=
         ElevatorState::DoorOpen) {
    simulation.tick();
  }

  const PassengerId passengerId = simulation.spawnPassenger(MIN_FLOOR, 4);
  expect(!simulation.snapshot().assignedElevator(call).has_value(),
         "Passenger should wait while the previous doors remain open");

  for (int tick = 0; tick < 1'000; ++tick) {
    simulation.tick();
    const SimulationSnapshot snapshot = simulation.snapshot();

    if (snapshot.assignedElevator(call).has_value()) {
      expect(snapshot.passengers.at(passengerId).state ==
                 PassengerState::Waiting,
             "Passenger should remain queued for the new hall call");
      return;
    }
  }

  throw std::runtime_error(
      "Empty hall call permanently blocked a future passenger");
}

void testSameInputsProduceSameState() {
  Simulation first{2};
  Simulation second{2};
  const HallCall call{3, Direction::Up};

  first.selectFloor(0, 5);
  second.selectFloor(0, 5);
  first.addHallCall(call);
  second.addHallCall(call);

  const int ticks = 2 * simulation_timing::TRAVEL_TICKS_PER_FLOOR + 1;
  for (int i = 0; i < ticks; ++i) {
    first.tick();
    second.tick();
  }

  const SimulationSnapshot firstSnapshot = first.snapshot();
  const SimulationSnapshot secondSnapshot = second.snapshot();

  expect(firstSnapshot.currentTick == secondSnapshot.currentTick,
         "Equal inputs should produce equal tick counters");
  expect(firstSnapshot.elevators.size() == secondSnapshot.elevators.size(),
         "Equal simulations should contain the same fleet size");

  for (std::size_t id = 0; id < firstSnapshot.elevators.size(); ++id) {
    const ElevatorSnapshot &left = firstSnapshot.elevators[id];
    const ElevatorSnapshot &right = secondSnapshot.elevators[id];

    expect(left.currentFloor == right.currentFloor,
           "Equal inputs should produce equal elevator floors");
    expect(left.direction == right.direction,
           "Equal inputs should produce equal elevator directions");
    expect(left.state == right.state,
           "Equal inputs should produce equal elevator states");
    expect(std::abs(left.stateProgress - right.stateProgress) < 0.0001F,
           "Equal inputs should produce equal state progress");
    expect(left.hasPendingRequests == right.hasPendingRequests,
           "Equal inputs should produce equal pending-request state");
    expect(left.capacity == right.capacity,
           "Equal inputs should produce equal elevator capacities");
  }
}

} // namespace

int main() {
  int failures = 0;

  failures += !runTest("starts at tick zero with entire fleet",
                       testStartsAtTickZeroWithEntireFleet);
  failures += !runTest("tick advances world counter",
                       testTickAdvancesWorldCounter);
  failures += !runTest("forwards internal request",
                       testForwardsInternalRequest);
  failures += !runTest("assigns hall call", testAssignsHallCall);
  failures += !runTest("spawns passenger and creates hall call",
                       testSpawnsPassengerAndCreatesHallCall);
  failures += !runTest("passenger waits until doors open",
                       testPassengerWaitsUntilDoorsOpen);
  failures += !runTest("passenger completes journey",
                       testPassengerCompletesJourney);
  failures += !runTest("elevator capacity and remaining queue",
                       testElevatorCapacityAndRemainingQueue);
  failures += !runTest("passenger arriving at open doors waits for departure",
                       testPassengerArrivingAtOpenDoorsWaitsForDeparture);
  failures += !runTest("propagates service event",
                       testPropagatesServiceEvent);
  failures += !runTest("empty hall call does not block future passenger",
                       testEmptyHallCallDoesNotBlockFuturePassenger);
  failures += !runTest("same inputs produce same state",
                       testSameInputsProduceSameState);

  if (failures == 0) {
    std::cout << "All simulation tests passed.\n";
  }

  return failures == 0 ? 0 : 1;
}
