#include "saveLoadConfig.hpp"

// function to load parameters from config file, with error handling and defaults
int loadConfig(SetupParameters& params, const std::string& path) {
    std::ifstream configFile(path);
    if (!configFile) {
        std::cerr << "Warning: Could not open " << path << ". Using default parameters." << std::endl;
        return 0;
    }

    auto trim = [](std::string& s) {
        const char* ws = " \t\r\n";
        s.erase(0, s.find_first_not_of(ws));
        s.erase(s.find_last_not_of(ws) + 1);
    };

    std::string line;
    size_t lineNo = 0;
    while (std::getline(configFile, line)) {
        ++lineNo;
        trim(line);
        if (line.empty() || line[0] == '#') { continue; }

        auto pos = line.find('=');
        if (pos == std::string::npos) {
            std::cerr << "Warning: Invalid config format at line " << lineNo << ". Skipping." << std::endl;
            continue;
        }

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        trim(key);
        trim(value);

    //         float border_percent = 0.25; // 25%
    // float resize_factor = 0.25; // Resize to 25% of original size

        try {
            if (key == "STRIP_LENGTH") {
                params.setLength(std::stof(value));
            } else if (key == "LED_DENSITY") {
                params.setDensity(std::stof(value));
            } else if (key == "LED_COUNT") {
                params.setLEDCount(std::stoi(value));
            } else if (key == "BRIGHTNESS") {
                params.setBrightness(std::stoi(value));
            } else if (key == "CORNER_LED_LAYOUT") {
                std::stringstream ss(value);
                std::string item;
                int corners[4];
                int i = 0;
                while(std::getline(ss, item, ',') && i < 4) {
                    corners[i++] = std::stoi(item);
                }
                if (i != 4) {
                    throw std::runtime_error("CORNER_LED_LAYOUT must have 4 comma-separated values");
                }
                params.setCornerLEDLayout(corners);
            } else if(key == "BORDER_PERCENT") {
                params.setBorderPercent(std::stof(value));
            } else if(key == "RESIZE_FACTOR") {
                params.setResizeFactor(std::stof(value));
            } else if(key == "SALIENCY_FACTOR") {
                params.setSaliencyFactor(std::stof(value));
            } else if(key == "MAX_AMPERAGE") {
                params.setMaxAmperage(std::stof(value));
            } else if (key == "SUPPLY_VOLTAGE"){
                params.setSupplyVoltage(std::stof(value));
            } else {
                std::cerr << "Warning: Unknown config key '" << key << "' at line " << lineNo << ". Ignoring." << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Warning: Invalid value for '" << key << "' at line " << lineNo
                      << ". Using default. (" << e.what() << ")" << std::endl;
        }
    }
    return 1; // Return 1 to indicate that we attempted to load the config
}

int saveConfig(SetupParameters& params, const std::string& path) {
    std::cout << "Saving config to " << path << "..." << std::endl;
    std::ofstream configFile(path);
    if (!configFile.is_open()) {
        std::cerr << "Error: Could not open " << path << " for writing." << std::endl;
        return 0;
    }

    configFile << "STRIP_LENGTH=" << params.getLength() << "\n";
    configFile << "LED_DENSITY=" << params.getDensity() << "\n";
    configFile << "LED_COUNT=" << params.getLEDCount() << "\n";
    configFile << "BRIGHTNESS=" << params.getBrightness() << "\n";

    // Handle the layout array
    // Note: Depends on whether getCornerLEDLayout() is const qualified. If not, this might need adjustment in the header.
    auto corners = params.getCornerLEDLayout();
    if (corners) {
        configFile << "CORNER_LED_LAYOUT=" << corners[0] << "," << corners[1] << "," 
                   << corners[2] << "," << corners[3] << "\n";
    }

    // Write advanced image processing/hardware limits
    configFile << "BORDER_PERCENT=" << params.getBorderPercent() << "\n";
    configFile << "RESIZE_FACTOR=" << params.getResizeFactor() << "\n";
    configFile << "SALIENCY_FACTOR=" << params.getSaliencyFactor() << "\n";
    configFile << "MAX_AMPERAGE=" << params.getMaxAmperage() << "\n";
    configFile << "SUPPLY_VOLTAGE=" << params.getSupplyVoltage() << "\n";

    configFile.close();
    
    if (configFile.fail()) {
        std::cerr << "Error: Failed while writing to " << path << "." << std::endl;
        return 0;
    }
    
    return 1;
}