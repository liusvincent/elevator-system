#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <iostream>
#include <vector>
#include <set>
#include <map>
#include <memory>

#include <chrono>
#include <thread>
#include <mutex>

using std::vector;
using std::cout;
using std::endl;
using std::set;
using std::map;
using std::pair;
using std::unique_ptr;
using std::make_unique;


// -- Globals -- 
/*
Defining some magic numbers
*/ 

constexpr int MAX_FLOOR = 5;
constexpr int MIN_FLOOR = 1;
constexpr int NUM_ELEVATORS = 3;

constexpr int WIN_WIDTH = 1280;
constexpr int WIN_HEIGHT = 720;

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

// -- Service Event Struct --
/*
To notify the dispatcher class when the elevator class has serviced a floor
*/
struct ServiceEvent {
    bool served;
    int floor;
    Direction direction;
    bool was_hall;
};

// -- Elevator Class --
/*
What it needs to achieve:
1. receive requests/calls from inside and outside the elevator
2. serve all requests that are in the same direction
3. service each floor that has a request
*/
class Elevator {
    friend std::ostream& operator<<(std::ostream& os, const Elevator& elevator);
public:
    Elevator() : curr_floor_(MIN_FLOOR), state_(State::DOOR_CLOSE), direction_(Direction::IDLE) {}
    
    int getCurrFloor() const {
        return curr_floor_;
    }

    Direction getDirection() const {
        return direction_;
    }

    int getFirstRequest(Direction direction) const {
        const set<int> * temp_set = nullptr;
        if (direction == Direction::UP) temp_set = &external_up_;
        else if (direction == Direction::DOWN) temp_set = &external_down_;
        if (temp_set == nullptr || temp_set->empty()) {
            throw std::logic_error("No requests in set");
        }
        return *temp_set->begin();
    }

    int getFinalRequest(Direction direction) const{
        const set<int> * temp_set = nullptr;
        if (direction == Direction::UP) temp_set = &external_up_;
        else if (direction == Direction::DOWN) temp_set = &external_down_;
        if (temp_set == nullptr || temp_set->empty()) {
            throw std::logic_error("No requests in set");
        }
        return *temp_set->rbegin();
    }

    void addInternalRequest(int floor) {
        // if (floor == curr_floor_) serviceCurrFloor(); open door not service floor or do i handle in step look
        if (floor <= 0 || floor > MAX_FLOOR) {
            throw std::out_of_range("Invalid Floor Request: Out of Floor Range");
        } 
        internal_requests_.insert(floor);
    }

    void addExternalCall(int floor, Direction direction) {
        // if (floor == curr_floor_) serviceCurrFloor(); open door not service floor or do i handle in step look
        if (floor <= 0 || floor > MAX_FLOOR) {
            throw std::out_of_range("Invalid Floor Request: Out of Floor Range");
        }
        if (direction == Direction::IDLE) {
            throw std::invalid_argument("Invalid Floor Request: External Call Can't be IDLE");
        }else if (direction == Direction::UP) {
            external_up_.insert(floor);
        } else if (direction == Direction::DOWN) {
            external_down_.insert(floor);
        }
    }

    void removeRequest(set<int> & input_set, int floor) {
        auto it = input_set.find(floor);
        if (it != input_set.end()) input_set.erase(it);
    }

    ServiceEvent serviceCurrFloor() {
        ServiceEvent event{false, curr_floor_, direction_, false};

        state_ = State::STOPPED;

        // remove request
        if (direction_ == Direction::UP && external_up_.count(curr_floor_)) {
            removeRequest(external_up_, curr_floor_);
            event.served = true;
            event.was_hall = true;
        }
        if (direction_ == Direction::DOWN && external_down_.count(curr_floor_)) { 
            removeRequest(external_down_, curr_floor_);
            event.served = true;
            event.was_hall = true;
        }
        if (internal_requests_.count(curr_floor_)) {
            removeRequest(internal_requests_, curr_floor_); 
            event.served = true;
        }
               
        // handleDoors()
        
        return event;
    }

    void checkClosest(const set<int> & input_set, int & closest, int & closest_floor) const {
        // to help decide direction
        for (int floor : input_set) {
            int distance = abs(curr_floor_ - floor);
            if (distance <= closest) {
                closest = distance;
                closest_floor = floor;
            }
        }
    }

    Direction decideDirection() {
        /*
        Decide on direction by checking which call/request is the closest to current floor when idle
        */
        int closest_floor = -1;
        int closest = INT_MAX;

        if (internal_requests_.empty() && external_down_.empty() && external_up_.empty()) {
            return Direction::IDLE;
        } else {
            if (!internal_requests_.empty()) checkClosest(internal_requests_, closest, closest_floor);
            if (!external_down_.empty()) checkClosest(external_down_, closest, closest_floor);
            if (!external_up_.empty()) checkClosest(external_up_, closest, closest_floor);
            if (closest_floor > curr_floor_) return Direction::UP;
            else if (closest_floor < curr_floor_) return Direction::DOWN;
            else {
                if (external_up_.count(closest_floor)) return Direction::UP;
                if (external_down_.count(closest_floor)) return Direction::DOWN;
                return Direction::IDLE;
            }
        }
    }

    bool hasRequestAtCurrFloor() const {
        if (internal_requests_.count(curr_floor_)) return true;
        if ((direction_ == Direction::UP || direction_ == Direction::IDLE) && external_up_.count(curr_floor_)) return true;
        if ((direction_ == Direction::DOWN || direction_ == Direction::IDLE) && external_down_.count(curr_floor_)) return true;
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

    ServiceEvent stepLOOK() {
        ServiceEvent event{false, curr_floor_, direction_, false};

        if (direction_ == Direction::IDLE) {
            direction_ = decideDirection();
        }
        if (hasRequestAtCurrFloor()) {
            event = serviceCurrFloor();
            return event;
        }
        if (hasRequestAhead()) {
            if (direction_ == Direction::UP) {
                if (curr_floor_ != MAX_FLOOR) {
                    curr_floor_++; 
                    state_ = State::MOVING;
                }
            } else if (direction_ == Direction::DOWN) {
                if (curr_floor_ != MIN_FLOOR) {
                    curr_floor_--; 
                    state_ = State::MOVING;
                }
            }
        } else {
            state_ = State::STOPPED;
            direction_ = Direction::IDLE;
        }

        return event;
    }

private:
    int curr_floor_;
    State state_;
    Direction direction_;
    set<int> internal_requests_;
    set<int> external_up_;
    set<int> external_down_;
};

// -- Dispatcher Class --
/*
What it needs to achieve:
1. keep hall calls to signify lights in the hall
2. assign elevator stops
3. acts as building class as well
*/
class Dispatcher {
public:
    explicit Dispatcher(int num_elevators) {
        elevators.reserve(num_elevators);
        for (int i = 0; i < num_elevators; i++) {
            elevators.push_back(make_unique<Elevator>());
        }
    }

    void addHallCall(int floor, Direction dir) {
        /*
        Simply add the hall calls into its rightful set (up or down calls)
        */
        if (floor <= 0 || floor > MAX_FLOOR) 
            throw std::out_of_range("Invalid Floor Request: Out of Floor Range");
        if (dir == Direction::IDLE) 
            throw std::invalid_argument("Invalid Floor Request: Hall Call Can't be IDLE");
        else if (dir == Direction::UP) 
            up_calls_.insert(floor);
        else if (dir == Direction::DOWN) 
            down_calls_.insert(floor);

        dispatch(floor, dir); // assign the call to an elevator
    }

    void removeHallCall(int floor, Direction dir) {
        if (dir == Direction::UP) up_calls_.erase(floor);
        else if (dir ==Direction::DOWN) down_calls_.erase(floor);
        assigned_to_.erase({floor, dir});
    }

    int cost(Elevator* elevator, int target_floor, Direction call_dir) {
        /*
        Calculate and return how many floors it will take for that elevator to reach the target floor
        1. cost is calculated from how many floors it will take to reach target_floor
        */
       const int curr_floor = elevator->getCurrFloor();
       const Direction dir = elevator->getDirection();

       int cost = 0;

       if (dir == Direction::IDLE) {
            cost += abs(target_floor - curr_floor);
       } else if (dir == Direction::UP) {
            bool on_the_way = (call_dir == Direction::UP) && (target_floor >= curr_floor);
            if (!on_the_way) {
                int top;
                try {top = elevator->getFinalRequest(Direction::UP);}
                catch (const std::logic_error &) {top = curr_floor;}
                cost += (top - curr_floor) + (top - target_floor);
            } else {
                cost += (target_floor - curr_floor);
            }
       } else { // elevator->getDirection() == Direction::DOWN
            bool on_the_way = (call_dir == Direction::DOWN) && (target_floor <= curr_floor);
            if (!on_the_way) {
                int bottom;
                try {bottom = elevator->getFirstRequest(Direction::DOWN);}
                catch (const std::logic_error &){bottom = curr_floor;}
                cost += (curr_floor - bottom) + (target_floor - bottom);
            } else {
                cost += (curr_floor - target_floor);
            }
       }

       //add other costs in the future
       return cost;
    }

    void dispatch(int floor, Direction call_dir) {
        /*
        Find the most optimal elevator to assign the calls
        1. If the elevator is on its way (the closest elevator)
        2. If the elevator is IDLE
        */
        pair<int, Direction> key{floor, call_dir};
        if (assigned_to_.find(key) != assigned_to_.end()) // if call is already assigned, ignore
            return;
        
        int best_idx = -1;
        int best_cost = INT_MAX;

        for (size_t i = 0; i < elevators.size(); i++) { // loop through each elevator to find best cost and best index
            Elevator* e  = elevators[i].get();
            int c = cost(e, floor, call_dir);
            if (c < best_cost) {
                best_cost = c;
                best_idx = i;
            } else if (c == best_cost) { // if both elevators have same cost check if either is IDLE
                if (best_idx != -1 
                    && elevators[best_idx]->getDirection() != Direction::IDLE 
                    && e->getDirection() == Direction::IDLE) {
                        best_idx = i; 
                }
            }
        }

        if (best_idx == -1) return;

        elevators[best_idx]->addExternalCall(floor, call_dir);
        assigned_to_[key] = best_idx;
    }
private:
    set<int> up_calls_;
    set<int> down_calls_;
    vector<unique_ptr<Elevator>> elevators;
    map<pair<int, Direction>, int> assigned_to_;
};

// -- Function Declarations --
void renderUI();

// -- Main Callback --
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static Dispatcher dispatcher{NUM_ELEVATORS};

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    SDL_SetAppMetadata("Elevator Simulation", "1.0", NULL);

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("Elevator Simulation", WIN_WIDTH, WIN_HEIGHT, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderLogicalPresentation(renderer, WIN_WIDTH, WIN_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    return SDL_APP_CONTINUE;
}

/*
function to handle events
user input logic goes here
*/ 
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    switch(event->type) {
        case (SDL_EVENT_QUIT):
            return SDL_APP_SUCCESS;

        case (SDL_EVENT_MOUSE_BUTTON_DOWN):
            SDL_Log("Mouse button pressed at: %f, %f", event->button.x, event->button.y);
            break;
    }
    return SDL_APP_CONTINUE;
}

/*
function runs once per frame (the "main" code)
elevator logic goes here
*/ 
SDL_AppResult SDL_AppIterate(void *appstate) {
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
    SDL_RenderClear(renderer);

    // Building
    constexpr float BUILDING_WIDTH = 500;
    constexpr float BUILDING_HEIGHT = 695;

    SDL_SetRenderDrawColor(renderer, 0, 0, 0xFF, 0xFF);
    SDL_FRect building = {45, 25, BUILDING_WIDTH, BUILDING_HEIGHT};
    SDL_RenderFillRect(renderer, &building);

    // Floors
    constexpr float MARGIN = 10;
    constexpr float FLOOR_GAP = 10;
    constexpr float FLOOR_HEIGHT = (BUILDING_HEIGHT - MARGIN - FLOOR_GAP * (MAX_FLOOR-1)) / MAX_FLOOR;
    constexpr float FLOOR_WIDTH = BUILDING_WIDTH - MARGIN * 2;

    SDL_SetRenderDrawColor(renderer, 0xFF, 0, 0, 0xFF);
    SDL_FRect floors[MAX_FLOOR];
    for (int i = 0; i < MAX_FLOOR; i++) {
        floors[i].x = building.x + MARGIN; 
        floors[i].y = building.y + MARGIN + i * (FLOOR_HEIGHT + FLOOR_GAP);
        floors[i].w = FLOOR_WIDTH;
        floors[i].h = FLOOR_HEIGHT;
        SDL_RenderFillRect(renderer, &floors[i]);
    }

    // Elevator halls
    constexpr float HALL_GAP = 70;
    constexpr float HALL_WIDTH = 80;
    constexpr float HALL_HEIGHT = FLOOR_HEIGHT * 0.7;

    SDL_SetRenderDrawColor(renderer, 0, 0xFF, 0, 0xFF);
    SDL_FRect halls[MAX_FLOOR][NUM_ELEVATORS];
    for (int i = 0; i < MAX_FLOOR; i++) {
        for (int j = 0; j < NUM_ELEVATORS; j++) {
            halls[i][j].x = floors[i].x + MARGIN*4 + j * (HALL_GAP + HALL_WIDTH);
            halls[i][j].y = floors[i].y + (FLOOR_HEIGHT - HALL_HEIGHT);
            halls[i][j].w = HALL_WIDTH;
            halls[i][j].h = HALL_HEIGHT;
            SDL_RenderFillRect(renderer, &halls[i][j]);
        }
    }

    // Hall Indicator
    constexpr float INDICATOR_WIDTH = HALL_WIDTH * 0.5;
    constexpr float INDICATOR_HEIGHT = HALL_HEIGHT * 0.2;
    constexpr float INDICATOR_GAP = 5;

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
    SDL_FRect indicators[NUM_ELEVATORS];
    for (int i = 0; i < NUM_ELEVATORS; i++) {
        SDL_FRect hall = halls[MAX_FLOOR - 1][i];
        indicators[i].x = hall.x + (hall.w - INDICATOR_WIDTH) * 0.5;
        indicators[i].y = hall.y - INDICATOR_GAP - INDICATOR_HEIGHT;
        indicators[i].w = INDICATOR_WIDTH;
        indicators[i].h = INDICATOR_HEIGHT;
        SDL_RenderFillRect(renderer, &indicators[i]);
    }

    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;
}

/*
function runs at shutdown
*/
void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    /* SDL clean up automatically */
}

// -- Function Definitions --
std::ostream& operator<<(std::ostream& os, const Elevator& elevator) {
    // I just want to see a snapshot of the elevator to see if it's working
    os << "Direction: " << (elevator.direction_ == Direction::UP ? "UP" : (elevator.direction_ == Direction::DOWN ? "DOWN" : "IDLE")) << endl;
    os << "State: ";
    switch(elevator.state_) {
        using enum State;
        case(DOOR_OPEN): os << "DOOR_OPEN"; break;
        case(DOOR_CLOSE): os << "DOOR_CLOSE"; break;
        case(DOOR_OPENING): os << "DOOR_OPENING"; break;
        case(DOOR_CLOSING): os << "DOOR_CLOSING"; break;
        case(MOVING): os << "MOVING"; break;
        case(STOPPED): os << "STOPPED"; break;
    }
    os << endl;
    os << "Current Floor: " << elevator.curr_floor_ << endl;
    os << "Internal Request: ";
    for (const auto & i : elevator.internal_requests_) {
        os << i << " ";
    }
    os << endl;
    os << "External Up: ";
    for (const auto & i : elevator.external_up_) {
        os << i << " ";
    }
    os << endl;
    os << "External Down: ";
    for (const auto & i : elevator.external_down_) {
        os << i << " ";
    }
    os << endl;
    return os;
}