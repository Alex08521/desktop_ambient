#include "audio_controller.h"
#include <iostream>
#include <csignal>
#include <atomic>

std::atomic<bool> gaRunning{true};

void signalHandler(int signal) {
    gaRunning = signal != 0;
}

int main() {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    ambient::AudioController controller;
    
    std::cout << "Starting sound service..." << std::endl;
    controller.start();
    
    while (gaRunning) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    controller.stop();
    std::cout << "Sound service stopped." << std::endl;
    
    return 0;
}