#include "simulation.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr int MAX_SCENARIO_TICKS = 10'000;

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

bool allPassengersArrived(const SimulationSnapshot &snapshot) {
  for (const PassengerSnapshot &passenger : snapshot.passengers) {
    if (passenger.state != PassengerState::Arrived) {
      return false;
    }
  }

  return true;
}

void validateSnapshot(const SimulationSnapshot &snapshot) {
  std::vector<std::size_t> occupancy(snapshot.elevators.size(), 0);

  for (std::size_t index = 0; index < snapshot.passengers.size(); ++index) {
    const PassengerSnapshot &passenger = snapshot.passengers[index];
    expect(passenger.id == index,
           "Passenger IDs should match their stable snapshot position");

    switch (passenger.state) {
    case PassengerState::Waiting:
      expect(!passenger.elevatorId,
             "Waiting passenger should not be inside an elevator");
      expect(!passenger.boardedAtTick && !passenger.arrivedAtTick,
             "Waiting passenger should not have journey timestamps");
      break;

    case PassengerState::Riding:
      expect(passenger.elevatorId.has_value(),
             "Riding passenger should identify an elevator");
      expect(*passenger.elevatorId < snapshot.elevators.size(),
             "Riding passenger should reference a valid elevator");
      expect(passenger.boardedAtTick.has_value(),
             "Riding passenger should have a boarding timestamp");
      expect(!passenger.arrivedAtTick,
             "Riding passenger should not have arrived yet");
      expect(passenger.spawnedAtTick <= *passenger.boardedAtTick,
             "Passenger cannot board before spawning");
      ++occupancy[*passenger.elevatorId];
      break;

    case PassengerState::Arrived:
      expect(!passenger.elevatorId,
             "Arrived passenger should not remain inside an elevator");
      expect(passenger.boardedAtTick && passenger.arrivedAtTick,
             "Arrived passenger should retain both journey timestamps");
      expect(passenger.spawnedAtTick <= *passenger.boardedAtTick &&
                 *passenger.boardedAtTick < *passenger.arrivedAtTick,
             "Passenger timestamps should follow journey order");
      break;
    }
  }

  for (ElevatorId id = 0; id < occupancy.size(); ++id) {
    expect(occupancy[id] <= snapshot.elevators[id].capacity,
           "Elevator occupancy should never exceed capacity");
  }

  std::set<std::pair<int, Direction>> hallCalls;
  for (const HallCallAssignment &assignment :
       snapshot.hallCallAssignments) {
    expect(assignment.elevatorId < snapshot.elevators.size(),
           "Hall call should reference a valid elevator");
    expect(hallCalls.insert({assignment.call.floor,
                             assignment.call.direction})
               .second,
           "A hall call should have only one active assignment");
  }
}

SimulationSnapshot tickAndValidate(Simulation &simulation) {
  simulation.tick();
  SimulationSnapshot snapshot = simulation.snapshot();
  validateSnapshot(snapshot);
  return snapshot;
}

SimulationSnapshot advanceUntilElevatorState(
    Simulation &simulation, ElevatorId elevatorId, ElevatorState state,
    std::string_view failureMessage) {
  for (int tick = 0; tick < MAX_SCENARIO_TICKS; ++tick) {
    const SimulationSnapshot snapshot = simulation.snapshot();
    if (snapshot.elevators.at(elevatorId).state == state) {
      return snapshot;
    }
    tickAndValidate(simulation);
  }

  throw std::runtime_error(std::string(failureMessage));
}

void runUntilAllPassengersArrive(Simulation &simulation,
                                 std::string_view failureMessage) {
  for (int tick = 0; tick < MAX_SCENARIO_TICKS; ++tick) {
    const SimulationSnapshot snapshot = tickAndValidate(simulation);
    if (allPassengersArrived(snapshot)) {
      return;
    }
  }

  throw std::runtime_error(std::string(failureMessage));
}

void testOverflowPassengersUseSecondElevator() {
  Simulation simulation{2};
  constexpr std::size_t EXTRA_PASSENGERS = 2;
  constexpr std::size_t PASSENGER_COUNT =
      DEFAULT_CAPACITY + EXTRA_PASSENGERS;

  for (std::size_t i = 0; i < PASSENGER_COUNT; ++i) {
    simulation.spawnPassenger(MIN_FLOOR, 5);
  }

  bool observedFirstElevatorAtCapacity = false;
  bool observedSecondAssignment = false;
  bool observedPassengerInSecondElevator = false;

  for (int tick = 0; tick < MAX_SCENARIO_TICKS; ++tick) {
    const SimulationSnapshot snapshot = tickAndValidate(simulation);

    std::size_t waiting = 0;
    std::size_t riding = 0;
    for (const PassengerSnapshot &passenger : snapshot.passengers) {
      if (passenger.state == PassengerState::Waiting) {
        ++waiting;
      } else if (passenger.state == PassengerState::Riding) {
        ++riding;
      }

      if (passenger.elevatorId == ElevatorId{1}) {
        observedPassengerInSecondElevator = true;
      }
    }

    if (riding == DEFAULT_CAPACITY &&
        waiting == EXTRA_PASSENGERS) {
      for (PassengerId passengerId = 0;
           passengerId < DEFAULT_CAPACITY; ++passengerId) {
        expect(snapshot.passengers.at(passengerId).state ==
                   PassengerState::Riding,
               "Passengers should board their queue in FIFO order");
      }
      for (PassengerId passengerId = DEFAULT_CAPACITY;
           passengerId < PASSENGER_COUNT; ++passengerId) {
        expect(snapshot.passengers.at(passengerId).state ==
                   PassengerState::Waiting,
               "Overflow passengers should remain at the back of the queue");
      }
      observedFirstElevatorAtCapacity = true;
    }

    if (observedFirstElevatorAtCapacity) {
      const std::optional<ElevatorId> assignment =
          snapshot.assignedElevator({MIN_FLOOR, Direction::Up});
      if (assignment) {
        expect(*assignment == ElevatorId{1},
               "Remaining passengers should be assigned to the idle "
               "second elevator");
        observedSecondAssignment = true;
      }
    }

    if (allPassengersArrived(snapshot)) {
      expect(observedFirstElevatorAtCapacity,
             "Scenario should fill the first elevator");
      expect(observedSecondAssignment,
             "Remaining demand should be assigned to the second elevator");
      expect(observedPassengerInSecondElevator,
             "Remaining passengers should ride the second elevator");
      return;
    }
  }

  throw std::runtime_error(
      "Overflow scenario did not finish within the tick limit");
}

void testOppositeDirectionsBoardSeparately() {
  Simulation simulation{1};
  const PassengerId upPassenger = simulation.spawnPassenger(4, 6);
  const PassengerId downPassenger = simulation.spawnPassenger(4, 2);
  bool observedSeparateBoarding = false;

  for (int tick = 0; tick < MAX_SCENARIO_TICKS; ++tick) {
    const SimulationSnapshot snapshot = tickAndValidate(simulation);
    const PassengerSnapshot &up = snapshot.passengers.at(upPassenger);
    const PassengerSnapshot &down = snapshot.passengers.at(downPassenger);

    if (up.state == PassengerState::Riding) {
      expect(down.state == PassengerState::Waiting,
             "Down passenger should not board an upward-serving elevator");
      observedSeparateBoarding = true;
    }

    if (down.state == PassengerState::Riding) {
      expect(up.state == PassengerState::Arrived,
             "Elevator should finish the upward trip before boarding down");
    }

    if (up.state == PassengerState::Arrived &&
        down.state == PassengerState::Arrived) {
      expect(observedSeparateBoarding,
             "Scenario should observe direction-specific boarding");
      return;
    }
  }

  throw std::runtime_error(
      "Opposite-direction scenario did not finish within the tick limit");
}

void testUnloadingFreesCapacityBeforeBoarding() {
  Simulation simulation{1};
  std::vector<PassengerId> arrivingPassengers;
  arrivingPassengers.reserve(DEFAULT_CAPACITY);

  for (std::size_t i = 0; i < DEFAULT_CAPACITY; ++i) {
    arrivingPassengers.push_back(
        simulation.spawnPassenger(MIN_FLOOR, 3));
  }
  const PassengerId waitingPassenger = simulation.spawnPassenger(3, 5);

  bool observedBoardingAfterUnload = false;

  for (int tick = 0; tick < MAX_SCENARIO_TICKS; ++tick) {
    const SimulationSnapshot snapshot = tickAndValidate(simulation);
    const PassengerSnapshot &waiting =
        snapshot.passengers.at(waitingPassenger);

    if (waiting.state == PassengerState::Riding &&
        !observedBoardingAfterUnload) {
      for (PassengerId passengerId : arrivingPassengers) {
        expect(snapshot.passengers.at(passengerId).state ==
                   PassengerState::Arrived,
               "Passengers should unload before a waiting passenger boards");
      }
      observedBoardingAfterUnload = true;
    }

    if (allPassengersArrived(snapshot)) {
      expect(observedBoardingAfterUnload,
             "Waiting passenger should use capacity freed by unloading");
      return;
    }
  }

  throw std::runtime_error(
      "Unload-before-boarding scenario did not finish within the tick limit");
}

void testPassengersArrivingDuringDoorPhases() {
  {
    Simulation simulation{1};
    const HallCall call{MIN_FLOOR, Direction::Up};
    simulation.addHallCall(call);
    const SimulationSnapshot opening = tickAndValidate(simulation);
    expect(opening.elevators.at(0).state == ElevatorState::DoorOpening,
           "Scenario should begin during door opening");

    const PassengerId passenger = simulation.spawnPassenger(MIN_FLOOR, 4);
    advanceUntilElevatorState(simulation, 0, ElevatorState::DoorOpen,
                              "Elevator doors never finished opening");

    expect(simulation.snapshot().passengers.at(passenger).state ==
               PassengerState::Riding,
           "Passenger arriving during opening should board when doors open");
    runUntilAllPassengersArrive(
        simulation, "Door-opening passenger did not finish their journey");
  }

  for (ElevatorState arrivalState :
       {ElevatorState::DoorOpen, ElevatorState::DoorClosing}) {
    Simulation simulation{1};
    const HallCall call{MIN_FLOOR, Direction::Up};
    simulation.addHallCall(call);

    advanceUntilElevatorState(simulation, 0, arrivalState,
                              "Elevator never reached the expected door phase");

    const PassengerId passenger = simulation.spawnPassenger(MIN_FLOOR, 4);
    expect(simulation.snapshot().passengers.at(passenger).state ==
               PassengerState::Waiting,
           "Passenger arriving after service should wait for another call");

    advanceUntilElevatorState(
        simulation, 0, ElevatorState::Idle,
        "Elevator did not finish its current door cycle");

    const SimulationSnapshot afterDoorCycle = simulation.snapshot();
    expect(afterDoorCycle.passengers.at(passenger).state ==
               PassengerState::Waiting,
           "Late passenger should wait through the current door cycle");

    runUntilAllPassengersArrive(
        simulation, "Late door-phase passenger did not finish their journey");
  }
}

void testPassengerWaitsWhileAllElevatorsAreFull() {
  Simulation simulation{2};
  constexpr std::size_t FLEET_CAPACITY =
      2 * DEFAULT_CAPACITY;

  for (std::size_t i = 0; i < FLEET_CAPACITY; ++i) {
    simulation.spawnPassenger(MIN_FLOOR, MAX_FLOOR);
  }

  while (true) {
    const SimulationSnapshot snapshot = tickAndValidate(simulation);
    std::vector<std::size_t> occupancy(2, 0);

    for (const PassengerSnapshot &passenger : snapshot.passengers) {
      if (passenger.elevatorId) {
        ++occupancy[*passenger.elevatorId];
      }
    }

    if (occupancy[0] == DEFAULT_CAPACITY &&
        occupancy[1] == DEFAULT_CAPACITY) {
      break;
    }

    expect(snapshot.currentTick < MAX_SCENARIO_TICKS,
           "Both elevators should eventually fill");
  }

  const PassengerId waitingPassenger = simulation.spawnPassenger(2, 6);
  expect(simulation.snapshot().passengers.at(waitingPassenger).state ==
             PassengerState::Waiting,
         "Passenger should wait while the fleet is full");

  runUntilAllPassengersArrive(
      simulation, "Passenger was starved while all elevators were full");
}

void testInternalStopDoesNotBoardUnrelatedQueue() {
  Simulation simulation{1};
  simulation.selectFloor(0, MAX_FLOOR);

  for (int tick = 0; tick < MAX_SCENARIO_TICKS; ++tick) {
    const SimulationSnapshot snapshot = tickAndValidate(simulation);
    const ElevatorSnapshot &elevator = snapshot.elevators.at(0);
    if (elevator.currentFloor == MAX_FLOOR &&
        elevator.state == ElevatorState::Idle) {
      break;
    }
    expect(tick + 1 < MAX_SCENARIO_TICKS,
           "Elevator did not reach the top floor");
  }

  simulation.selectFloor(0, 4);
  simulation.selectFloor(0, 2);
  const PassengerId passenger = simulation.spawnPassenger(4, 6);
  bool observedInternalStop = false;

  for (int tick = 0; tick < MAX_SCENARIO_TICKS; ++tick) {
    const SimulationSnapshot snapshot = tickAndValidate(simulation);
    const ElevatorSnapshot &elevator = snapshot.elevators.at(0);
    const PassengerSnapshot &waiting = snapshot.passengers.at(passenger);

    if (!observedInternalStop && elevator.currentFloor == 4 &&
        elevator.state == ElevatorState::DoorOpen) {
      expect(elevator.direction == Direction::Down,
             "First floor-4 stop should occur while travelling down");
      expect(waiting.state == PassengerState::Waiting,
             "Up passenger should not board an internal-only downward stop");
      observedInternalStop = true;
    }

    if (waiting.state == PassengerState::Arrived) {
      expect(observedInternalStop,
             "Scenario should observe the unrelated internal stop");
      return;
    }
  }

  throw std::runtime_error(
      "Internal-stop scenario did not finish within the tick limit");
}

struct ScheduledPassenger {
  std::uint64_t spawnTick;
  int originFloor;
  int destinationFloor;
};

SimulationSnapshot runScheduledScenario(
    const std::vector<ScheduledPassenger> &schedule) {
  Simulation simulation{3};
  std::size_t nextPassenger = 0;

  for (int tick = 0; tick < MAX_SCENARIO_TICKS; ++tick) {
    const std::uint64_t currentTick = simulation.snapshot().currentTick;
    while (nextPassenger < schedule.size() &&
           schedule[nextPassenger].spawnTick == currentTick) {
      const ScheduledPassenger &passenger = schedule[nextPassenger];
      simulation.spawnPassenger(passenger.originFloor,
                                passenger.destinationFloor);
      ++nextPassenger;
    }

    const SimulationSnapshot snapshot = tickAndValidate(simulation);
    if (nextPassenger == schedule.size() && allPassengersArrived(snapshot)) {
      return snapshot;
    }
  }

  throw std::runtime_error(
      "Scheduled mixed-traffic scenario did not finish within the tick limit");
}

void expectEquivalentResults(const SimulationSnapshot &left,
                             const SimulationSnapshot &right) {
  expect(left.currentTick == right.currentTick,
         "Identical scenarios should finish on the same tick");
  expect(left.passengers.size() == right.passengers.size(),
         "Identical scenarios should contain the same passengers");

  for (std::size_t id = 0; id < left.passengers.size(); ++id) {
    const PassengerSnapshot &a = left.passengers[id];
    const PassengerSnapshot &b = right.passengers[id];

    expect(a.id == b.id && a.originFloor == b.originFloor &&
               a.destinationFloor == b.destinationFloor &&
               a.state == b.state && a.elevatorId == b.elevatorId &&
               a.spawnedAtTick == b.spawnedAtTick &&
               a.boardedAtTick == b.boardedAtTick &&
               a.arrivedAtTick == b.arrivedAtTick,
           "Identical scenarios should produce identical passenger results");
  }
}

void testMixedTrafficIsDeterministicAndCompletes() {
  const std::vector<ScheduledPassenger> schedule{
      {0, 1, 8},  {0, 1, 7},  {0, 1, 6},  {0, 1, 5},
      {0, 1, 4},  {0, 1, 3},  {0, 1, 8},  {0, 1, 7},
      {5, 4, 1},  {5, 4, 8},  {12, 8, 1}, {12, 7, 2},
      {20, 3, 6}, {20, 3, 1}, {30, 6, 2}, {30, 2, 7},
      {45, 5, 8}, {45, 5, 1}, {60, 8, 3}, {60, 1, 6},
  };

  const SimulationSnapshot first = runScheduledScenario(schedule);
  const SimulationSnapshot second = runScheduledScenario(schedule);

  expect(allPassengersArrived(first),
         "Every passenger in mixed traffic should eventually arrive");
  expectEquivalentResults(first, second);
}

} // namespace

int main() {
  int failures = 0;

  failures += !runTest("overflow passengers use second elevator",
                       testOverflowPassengersUseSecondElevator);
  failures += !runTest("opposite directions board separately",
                       testOppositeDirectionsBoardSeparately);
  failures += !runTest("unloading frees capacity before boarding",
                       testUnloadingFreesCapacityBeforeBoarding);
  failures += !runTest("passengers arriving during door phases",
                       testPassengersArrivingDuringDoorPhases);
  failures += !runTest("passenger waits while all elevators are full",
                       testPassengerWaitsWhileAllElevatorsAreFull);
  failures += !runTest("internal stop does not board unrelated queue",
                       testInternalStopDoesNotBoardUnrelatedQueue);
  failures += !runTest("mixed traffic is deterministic and completes",
                       testMixedTrafficIsDeterministicAndCompletes);

  if (failures == 0) {
    std::cout << "All scenario tests passed.\n";
  }

  return failures == 0 ? 0 : 1;
}
