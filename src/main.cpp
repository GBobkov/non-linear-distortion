#include "audiogenerator_alsa.h"
#include <iostream>

int main() {
    TAlsaGenerator gen;
    
    // 1. Сканируем все звуковые карты
    std::cout << "Scanning audio interfaces..." << std::endl;
    gen.GetInterfaces();
    
    // 2. Пытаемся открыть UMC202HD
    std::cout << "Looking for UMC202HD..." << std::endl;
    if (!gen.GetUMC202Interface()) {
        std::cerr << "Failed to find UMC202HD!" << std::endl;
        return 1;
    }
    
    // 3. Играем тестовый сигнал
    std::cout << "Playing test tone..." << std::endl;
    gen.PlayAudioSample();
    
    std::cout << "Done!" << std::endl;
    return 0;
}