#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <iostream>
#include <vector>
#include <set>

#include <chrono>
#include <thread>
#include <mutex>

using std::vector;
using std::cout;
using std::endl;
using std::out_of_range;
using std::set;
using std::invalid_argument;

// -- Globals -- 
/*
Defining some magic numbers
*/ 

constexpr int MAX_FLOOR = 5;
constexpr int MIN_FLOOR = 1;
constexpr int NUM_ELEVATORS = 3;

enum class Direction {
    IDLE = 0,
    DOWN = -1,
    UP = 1
};

enum class State {
    DOOR_OPEN,
    DOOR_CLOSE,
    DOOR_OPENING,
    DOOR_CLOSING,
    STOPPED,
    MOVING
};

// -- Elevator Class --
/*
What it needs to achieve:
1. receive requests/calls from inside and outside the elevator
2. serve all requests that are in the same direction
3. service each floor that has a request
*/

class Elevator {
public:
    Elevator() : num_floors_(MAX_FLOOR), curr_floor_(MIN_FLOOR), state_(State::DOOR_CLOSE), direction_(Direction::IDLE) {}
    
    int getCurrFloor() const {
        return curr_floor_;
    }

    void addInternalRequest(int floor) {
        if (floor <= 0 || floor > num_floors_) {
            throw out_of_range("Invalid Floor Request: Out of Floor Range");
        } 
        internal_requests_.insert(floor);
        if (floor == curr_floor_) serviceCurrFloor();
    }

    void addExternalCall(int floor, Direction direction) {
        if (floor <= 0 || floor > num_floors_) {
            throw out_of_range("Invalid Floor Request: Out of Floor Range");
        }
        if (direction == Direction::IDLE) {
            throw invalid_argument("Invalid Floor Request: External Call Can't be IDLE");
        }else if (direction == Direction::UP) {
            external_up_.insert(floor);
        } else if (direction == Direction::DOWN) {
            external_down_.insert(floor);
        }
        if (floor == curr_floor_) serviceCurrFloor();
    }

    void removeRequest(set<int> & input_set, int floor) {
        auto it = input_set.find(floor);
        if (it != input_set.end()) input_set.erase(it);
    }

    void serviceCurrFloor() {
        state_ = State::STOPPED;
        // remove request
        removeRequest(external_up_, curr_floor_);
        removeRequest(external_down_, curr_floor_);
        removeRequest(internal_requests_, curr_floor_);        
        // handleDoors()
    }

    void checkClosest(const set<int> & input_set, int & closest, int & closest_floor) const {
        // to help decide direction
        for (int floor : input_set) {
            int distance = abs(curr_floor_ - floor);
            if (distance <= closest) {
                closest = distance;
                closest_floor = floor;
            }
            else return; // early exit
        }
    }

    bool hasRequests() const {
        return !(internal_requests_.empty() && external_down_.empty() && external_up_.empty());
    }

    Direction decideDirection() {
        /*
        Decide on direction by checking which call/request is the closest to current floor when idle
        */
        int closest_floor = -1;
        int closest = num_floors_ + 1;
        if (!hasRequests()) {
            return Direction::IDLE;
        } else {
            if (!internal_requests_.empty()) checkClosest(internal_requests_, closest, closest_floor);
            if (!external_down_.empty()) checkClosest(external_down_, closest, closest_floor);
            if (!external_up_.empty()) checkClosest(external_up_, closest, closest_floor);
            if (closest_floor > curr_floor_) return Direction::UP;
            else if (closest_floor < curr_floor_) return Direction::DOWN;
            else return Direction::IDLE;
        }
    }

    bool hasRequestAtCurrFLoor() const {
        if (internal_requests_.count(curr_floor_)) return true;
        if (direction_ == Direction::UP && external_up_.count(curr_floor_)) return true;
        if (direction_ == Direction::DOWN && external_down_.count(curr_floor_)) return true;
        return false;
    }

    bool hasRequestAhead() const {
        if (direction_ == Direction::UP) {
            for (int floor : internal_requests_) {
                if (floor > curr_floor_) return true;
            }
            for (int floor : external_up_) {
                if (floor > curr_floor_) return true;
            }        
        } else if (direction_ == Direction::DOWN) {
            for (int floor : internal_requests_) {
                if (floor < curr_floor_) return true;
            }
            for (int floor : external_down_) {
                if (floor < curr_floor_) return true;
            }   
        }
        return false;
    }

    void stepLOOK() {
        if (direction_ == Direction::IDLE) {
            direction_ = decideDirection();
            return;
        }
        if (hasRequestAtCurrFLoor()) {
            serviceCurrFloor();
            return;
        }
        if (hasRequestAhead()) {
            state_ = State::MOVING;
            if (direction_ == Direction::UP) {
                if (curr_floor_ != num_floors_) curr_floor_++;
            } else if (direction_ == Direction::DOWN) {
                if (curr_floor_ != MIN_FLOOR) curr_floor_--;
            }
        } else {
            state_ = State::STOPPED;
            direction_ = Direction::IDLE;
        }
    }

private:
    int num_floors_;
    int curr_floor_;
    State state_;
    Direction direction_;
    set<int> internal_requests_;
    set<int> external_up_;
    set<int> external_down_;
};

int main() {
    cout << "Hello World";
}