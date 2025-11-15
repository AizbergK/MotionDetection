#include <opencv2/opencv.hpp>

#include "App.hpp"

//raspberry PI build
//g++ -std=c++20 -O3 -march=native -mtune=native -flto -pipe ./MotionDetectionApp/main.cpp -o ./release/motionApp `pkg-config --cflags --libs opencv4`

int main()
{
	// Camera settings
	constexpr int WIDTH{ 1280 };
    constexpr int HEIGHT{ 720 };
    constexpr int FPS{ 30 };
    constexpr int BUFFER_IN_SECONDS{ 3 };

	// Initialize app
	App app{ WIDTH, HEIGHT, FPS, BUFFER_IN_SECONDS };
    
	// Main loop
    app.run();

    return 0;
}