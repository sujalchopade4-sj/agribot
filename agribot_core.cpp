#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <memory>
#include <random>
#include <chrono>
#include <algorithm>

const int GRID_SIZE = 5;

class AgriBot;

// ==========================================
// Abstract Base Class: ActionCommand
// ==========================================
class ActionCommand {
public:
    virtual ~ActionCommand() = default;
    virtual bool execute(AgriBot& bot) = 0;
    virtual void undo(AgriBot& bot) = 0;
    virtual std::string describe() const = 0;
    virtual std::string serialize() const = 0;
};

// ==========================================
// Encapsulated Robot Core
// ==========================================
class AgriBot {
private:
    int x, y;
    int battery;
    std::vector<std::string> grid;
    std::vector<std::vector<bool>> discovered;
    std::string lastMessage;

public:
    AgriBot() : x(0), y(0), battery(100), lastMessage("AgriBot initialized and ready.") {
        resetField();
    }

    int getX() const { return x; }
    int getY() const { return y; }
    int getBattery() const { return battery; }
    const std::vector<std::string>& getGrid() const { return grid; }
    bool isDiscovered(int r, int c) const { return discovered[r][c]; }
    void setDiscovered(int r, int c, bool val) { discovered[r][c] = val; }
    std::string getMessage() const { return lastMessage; }

    void setMessage(const std::string& msg) { lastMessage = msg; }
    void setBattery(int b) { battery = std::clamp(b, 0, 100); }
    void setPos(int nx, int ny) { x = nx; y = ny; }
    char getCell(int r, int c) const { return grid[r][c]; }
    void setCell(int r, int c, char val) { grid[r][c] = val; }

    void resetField() {
        x = 0;
        y = 0;
        battery = 100;
        grid = std::vector<std::string>(GRID_SIZE, std::string(GRID_SIZE, 'H'));
        discovered = std::vector<std::vector<bool>>(GRID_SIZE, std::vector<bool>(GRID_SIZE, false));
        discovered[0][0] = true; // Base station start tile is scanned

        unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
        std::default_random_engine gen(seed);
        std::vector<std::pair<int, int>> spots;

        for (int r = 0; r < GRID_SIZE; ++r) {
            for (int c = 0; c < GRID_SIZE; ++c) {
                if (r == 0 && c == 0) continue;
                spots.push_back({r, c});
            }
        }
        std::shuffle(spots.begin(), spots.end(), gen);

        for (int i = 0; i < 3 && i < (int)spots.size(); ++i) {
            grid[spots[i].first][spots[i].second] = 'W';
        }
        for (int i = 3; i < 5 && i < (int)spots.size(); ++i) {
            grid[spots[i].first][spots[i].second] = 'D';
        }
        lastMessage = "Grid randomized: 3 Weeds and 2 Blight targets deployed under Fog of War.";
    }
};

// ==========================================
// Concrete Child Classes
// ==========================================
class MoveCommand : public ActionCommand {
private:
    std::string direction;
    int prevX, prevY;
    int newX, newY;
    int prevBattery;

public:
    MoveCommand(std::string dir) : direction(dir), prevX(-1), prevY(-1), newX(-1), newY(-1), prevBattery(-1) {}
    MoveCommand(std::string dir, int px, int py, int nx, int ny, int pb)
        : direction(dir), prevX(px), prevY(py), newX(nx), newY(ny), prevBattery(pb) {}

    bool execute(AgriBot& bot) override {
        if (bot.getBattery() < 4) {
            bot.setMessage("Battery critical (<4%). Navigation stopped.");
            return false;
        }

        prevX = bot.getX();
        prevY = bot.getY();
        prevBattery = bot.getBattery();

        int nx = prevX, ny = prevY;
        if (direction == "up") nx--;
        else if (direction == "down") nx++;
        else if (direction == "left") ny--;
        else if (direction == "right") ny++;

        if (nx < 0 || nx >= GRID_SIZE || ny < 0 || ny >= GRID_SIZE) {
            bot.setMessage("Boundary alert: Navigation blocked at edge.");
            return false;
        }

        newX = nx;
        newY = ny;
        bot.setPos(newX, newY);
        bot.setBattery(prevBattery - 4);
        bot.setMessage("Navigated " + direction + " to [" + std::to_string(newX) + "," + std::to_string(newY) + "].");
        return true;
    }

    void undo(AgriBot& bot) override {
        bot.setPos(prevX, prevY);
        bot.setBattery(prevBattery);
        bot.setMessage("Rolled back position to [" + std::to_string(prevX) + "," + std::to_string(prevY) + "].");
    }

    std::string describe() const override { return "Move " + direction; }
    
    std::string serialize() const override {
        return "move:" + direction + ":" + std::to_string(prevX) + ":" + std::to_string(prevY) +
               ":" + std::to_string(newX) + ":" + std::to_string(newY) + ":" + std::to_string(prevBattery);
    }
};

class InspectCommand : public ActionCommand {
private:
    int targetX, targetY;
    int prevBattery;
    bool wasAlreadyDiscovered;

public:
    InspectCommand() : targetX(-1), targetY(-1), prevBattery(-1), wasAlreadyDiscovered(false) {}
    InspectCommand(int tx, int ty, int pb, bool wad)
        : targetX(tx), targetY(ty), prevBattery(pb), wasAlreadyDiscovered(wad) {}

    bool execute(AgriBot& bot) override {
        if (bot.getBattery() < 2) {
            bot.setMessage("Battery insufficient for multispectral sweep.");
            return false;
        }
        targetX = bot.getX();
        targetY = bot.getY();
        prevBattery = bot.getBattery();
        wasAlreadyDiscovered = bot.isDiscovered(targetX, targetY);

        bot.setBattery(prevBattery - 2);
        bot.setDiscovered(targetX, targetY, true);

        char c = bot.getCell(targetX, targetY);
        std::string status = (c == 'H') ? "Healthy Foliage" : ((c == 'W') ? "Weed Infestation detected!" : "Crop Blight pathogen detected!");
        bot.setMessage("Scan at [" + std::to_string(targetX) + "," + std::to_string(targetY) + "]: " + status);
        return true;
    }

    void undo(AgriBot& bot) override {
        bot.setBattery(prevBattery);
        bot.setDiscovered(targetX, targetY, wasAlreadyDiscovered);
        bot.setMessage("Reverted inspection at [" + std::to_string(targetX) + "," + std::to_string(targetY) + "].");
    }

    std::string describe() const override { return "Multispectral Scan"; }

    std::string serialize() const override {
        return "inspect:" + std::to_string(targetX) + ":" + std::to_string(targetY) +
               ":" + std::to_string(prevBattery) + ":" + (wasAlreadyDiscovered ? "1" : "0");
    }
};

class SprayCommand : public ActionCommand {
private:
    char prevStatus;
    int targetX, targetY;
    int prevBattery;

public:
    SprayCommand() : prevStatus('H'), targetX(-1), targetY(-1), prevBattery(-1) {}
    SprayCommand(int tx, int ty, char ps, int pb)
        : prevStatus(ps), targetX(tx), targetY(ty), prevBattery(pb) {}

    bool execute(AgriBot& bot) override {
        targetX = bot.getX();
        targetY = bot.getY();

        if (!bot.isDiscovered(targetX, targetY)) {
            bot.setMessage("Safety Interlock: Cannot spray uninspected quadrant! Scan first.");
            return false;
        }

        if (bot.getBattery() < 8) {
            bot.setMessage("Insufficient charge for high-pressure spray.");
            return false;
        }

        prevStatus = bot.getCell(targetX, targetY);
        prevBattery = bot.getBattery();

        if (prevStatus == 'H') {
            bot.setMessage("Foliage healthy. Spray omitted to conserve chemical stores.");
            return false;
        }

        bot.setCell(targetX, targetY, 'H');
        bot.setBattery(prevBattery - 8);
        bot.setMessage("Neutralizer applied at [" + std::to_string(targetX) + "," + std::to_string(targetY) + "]. Status -> Healthy.");
        return true;
    }

    void undo(AgriBot& bot) override {
        bot.setCell(targetX, targetY, prevStatus);
        bot.setBattery(prevBattery);
        bot.setMessage("Reverted spray at [" + std::to_string(targetX) + "," + std::to_string(targetY) + "].");
    }

    std::string describe() const override { return "Precision Spray"; }

    std::string serialize() const override {
        return "spray:" + std::to_string(targetX) + ":" + std::to_string(targetY) +
               ":" + std::string(1, prevStatus) + ":" + std::to_string(prevBattery);
    }
};

// ==========================================
// Exact Deserialization Helper
// ==========================================
std::shared_ptr<ActionCommand> deserializeCommand(const std::string& raw) {
    std::stringstream ss(raw);
    std::string type;
    std::getline(ss, type, ':');

    if (type == "move") {
        std::string dir, px, py, nx, ny, pb;
        if (std::getline(ss, dir, ':') && std::getline(ss, px, ':') && std::getline(ss, py, ':') &&
            std::getline(ss, nx, ':') && std::getline(ss, ny, ':') && std::getline(ss, pb, ':')) {
            return std::make_shared<MoveCommand>(dir, std::stoi(px), std::stoi(py), std::stoi(nx), std::stoi(ny), std::stoi(pb));
        } else if (!dir.empty()) {
            return std::make_shared<MoveCommand>(dir);
        }
    } else if (type == "inspect") {
        std::string tx, ty, pb, wad;
        if (std::getline(ss, tx, ':') && std::getline(ss, ty, ':') &&
            std::getline(ss, pb, ':') && std::getline(ss, wad, ':')) {
            return std::make_shared<InspectCommand>(std::stoi(tx), std::stoi(ty), std::stoi(pb), wad == "1");
        } else {
            return std::make_shared<InspectCommand>();
        }
    } else if (type == "spray") {
        std::string tx, ty, ps, pb;
        if (std::getline(ss, tx, ':') && std::getline(ss, ty, ':') &&
            std::getline(ss, ps, ':') && std::getline(ss, pb, ':')) {
            return std::make_shared<SprayCommand>(std::stoi(tx), std::stoi(ty), ps[0], std::stoi(pb));
        } else {
            return std::make_shared<SprayCommand>();
        }
    }
    return nullptr;
}

// ==========================================
// Main Dispatcher
// ==========================================
int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "{\"error\": \"Invalid arguments.\"}" << std::endl;
        return 1;
    }

    std::string stateFile = argv[1];
    std::string action = argv[2];

    AgriBot bot;
    std::vector<std::string> history;
    std::vector<std::string> undoStackSerialized;
    std::vector<std::string> redoStackSerialized;
    std::vector<std::string> queueSerialized;

    // Load persisted state
    std::ifstream in(stateFile);
    if (in.is_open()) {
        int x, y, batt;
        if (in >> x >> y >> batt) {
            bot.setPos(x, y);
            bot.setBattery(batt);
            for (int r = 0; r < GRID_SIZE; ++r) {
                std::string row;
                in >> row;
                for (int c = 0; c < GRID_SIZE; ++c) bot.setCell(r, c, row[c]);
            }
            for (int r = 0; r < GRID_SIZE; ++r) {
                std::string discRow;
                in >> discRow;
                for (int c = 0; c < GRID_SIZE; ++c) bot.setDiscovered(r, c, discRow[c] == '1');
            }

            int uSize, rSize, qSize, hSize;
            in >> uSize;
            undoStackSerialized.resize(uSize);
            for (int i = 0; i < uSize; ++i) in >> undoStackSerialized[i];

            in >> rSize;
            redoStackSerialized.resize(rSize);
            for (int i = 0; i < rSize; ++i) in >> redoStackSerialized[i];

            in >> qSize;
            queueSerialized.resize(qSize);
            for (int i = 0; i < qSize; ++i) in >> queueSerialized[i];

            in >> hSize;
            in.ignore();
            history.resize(hSize);
            for (int i = 0; i < hSize; ++i) std::getline(in, history[i]);
        }
        in.close();
    }

    // Process Actions
    if (action == "reset") {
        bot.resetField();
        undoStackSerialized.clear();
        redoStackSerialized.clear();
        queueSerialized.clear();
        history.clear();
        history.push_back("Reset mission grid.");
    }
    else if (action == "move" || action == "spray" || action == "inspect") {
        std::shared_ptr<ActionCommand> cmd = nullptr;
        if (action == "move") {
            std::string dir = "up";
            if (argc >= 4) {
                std::string p = argv[3];
                if (p.rfind("dir=", 0) == 0) dir = p.substr(4);
            }
            cmd = std::make_shared<MoveCommand>(dir);
        } else if (action == "spray") {
            cmd = std::make_shared<SprayCommand>();
        } else if (action == "inspect") {
            cmd = std::make_shared<InspectCommand>();
        }

        if (cmd && cmd->execute(bot)) {
            undoStackSerialized.push_back(cmd->serialize());
            redoStackSerialized.clear();
            history.push_back(cmd->describe());
        }
    }
    else if (action == "undo" && !undoStackSerialized.empty()) {
        std::string raw = undoStackSerialized.back();
        undoStackSerialized.pop_back();
        auto cmd = deserializeCommand(raw);
        if (cmd) {
            cmd->undo(bot);
            redoStackSerialized.push_back(raw);
            history.push_back("Undo: " + cmd->describe());
        }
    }
    else if (action == "redo" && !redoStackSerialized.empty()) {
        std::string raw = redoStackSerialized.back();
        redoStackSerialized.pop_back();
        auto cmd = deserializeCommand(raw);
        if (cmd && cmd->execute(bot)) {
            undoStackSerialized.push_back(cmd->serialize());
            history.push_back("Redo: " + cmd->describe());
        }
    }
    else if (action == "queue_add" && argc >= 4) {
        std::string type = "inspect";
        std::string dir = "";
        for (int i = 3; i < argc; ++i) {
            std::string p = argv[i];
            if (p.rfind("type=", 0) == 0) type = p.substr(5);
            if (p.rfind("dir=", 0) == 0) dir = p.substr(4);
        }
        std::string serialized = (type == "move") ? ("move:" + dir) : type;
        queueSerialized.push_back(serialized);
        bot.setMessage("Enqueued in STL Queue: " + serialized);
    }
    else if (action == "queue_next" && !queueSerialized.empty()) {
        std::string raw = queueSerialized.front();
        queueSerialized.erase(queueSerialized.begin());
        auto cmd = deserializeCommand(raw);
        if (cmd && cmd->execute(bot)) {
            undoStackSerialized.push_back(cmd->serialize());
            redoStackSerialized.clear();
            history.push_back("Queue run: " + cmd->describe());
        }
    }

    // Persist State
    std::ofstream out(stateFile);
    if (out.is_open()) {
        out << bot.getX() << " " << bot.getY() << " " << bot.getBattery() << "\n";
        for (int r = 0; r < GRID_SIZE; ++r) {
            for (int c = 0; c < GRID_SIZE; ++c) out << bot.getCell(r, c);
            out << "\n";
        }
        for (int r = 0; r < GRID_SIZE; ++r) {
            for (int c = 0; c < GRID_SIZE; ++c) out << (bot.isDiscovered(r, c) ? "1" : "0");
            out << "\n";
        }
        out << undoStackSerialized.size() << "\n";
        for (const auto& s : undoStackSerialized) out << s << "\n";
        out << redoStackSerialized.size() << "\n";
        for (const auto& s : redoStackSerialized) out << s << "\n";
        out << queueSerialized.size() << "\n";
        for (const auto& s : queueSerialized) out << s << "\n";
        out << history.size() << "\n";
        for (const auto& s : history) out << s << "\n";
        out.close();
    }

    // Emit JSON
    std::cout << "{";
    std::cout << "\"x\":" << bot.getX() << ",";
    std::cout << "\"y\":" << bot.getY() << ",";
    std::cout << "\"battery\":" << bot.getBattery() << ",";
    std::cout << "\"msg\":\"" << bot.getMessage() << "\",";
    std::cout << "\"grid\":[";
    for (int r = 0; r < GRID_SIZE; ++r) {
        std::cout << "\"";
        for (int c = 0; c < GRID_SIZE; ++c) std::cout << bot.getCell(r, c);
        std::cout << "\"" << (r + 1 < GRID_SIZE ? "," : "");
    }
    std::cout << "],";

    std::cout << "\"discovered\":[";
    for (int r = 0; r < GRID_SIZE; ++r) {
        std::cout << "[";
        for (int c = 0; c < GRID_SIZE; ++c) {
            std::cout << (bot.isDiscovered(r, c) ? "true" : "false") << (c + 1 < GRID_SIZE ? "," : "");
        }
        std::cout << "]" << (r + 1 < GRID_SIZE ? "," : "");
    }
    std::cout << "],";

    auto printArr = [](const std::string& key, const std::vector<std::string>& arr, bool last = false) {
        std::cout << "\"" << key << "\":[";
        for (size_t i = 0; i < arr.size(); ++i) {
            std::cout << "\"" << arr[i] << "\"" << (i + 1 < arr.size() ? "," : "");
        }
        std::cout << "]" << (last ? "" : ",");
    };

    printArr("undo", undoStackSerialized);
    printArr("redo", redoStackSerialized);
    printArr("queue", queueSerialized);
    printArr("history", history, true);
    std::cout << "}" << std::endl;

    return 0;
}
