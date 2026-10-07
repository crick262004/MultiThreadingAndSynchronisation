#include <mutex>
#include <functional>

class TrafficLight {
private:
    std::mutex mtx;
    int greenRoad;

public:
    TrafficLight() {
        greenRoad = 1; // Road 1 initially has green light
    }

    void carArrived(
        int carId,                   // ID of the car
        int roadId,                  // ID of the road (1 or 2)
        int direction,               // Direction of the car
        std::function<void()> turnGreen, // Function to turn light green
        std::function<void()> crossCar   // Function to cross intersection
    ) {
        std::lock_guard<std::mutex> lock(mtx);
        
        if (greenRoad != roadId) {
            turnGreen();
            greenRoad = roadId;
        }
        
        crossCar();
    }
};