#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <memory>
#include <random>
#include <chrono>
#include <algorithm>

const int N = 5;
class AgriBot;

class ActionCommand {
public:
    virtual ~ActionCommand() = default;
    virtual bool execute(AgriBot& b) = 0;
    virtual void undo(AgriBot& b) = 0;
    virtual void redo(AgriBot& b) { execute(b); }
    virtual std::string serialize() const = 0;
};

class AgriBot {
private:
    int x = 0, y = 0, battery = 100;
    std::vector<std::string> grid;
    std::vector<std::vector<bool>> disc;
    std::string msg = "Ready at Solar Dock [0,0].";

public:
    AgriBot() { resetField(); }

    int getX() const { return x; }
    int getY() const { return y; }
    int getBattery() const { return battery; }
    std::string getMessage() const { return msg; }
    char getCell(int r, int c) const { return grid[r][c]; }
    bool isDiscovered(int r, int c) const { return disc[r][c]; }

    void setMessage(const std::string& m) { msg = m; }
    void setBattery(int b) { battery = std::clamp(b, 0, 100); }
    void setPos(int nx, int ny) { x = std::clamp(nx, 0, N - 1); y = std::clamp(ny, 0, N - 1); }
    void setCell(int r, int c, char v) { grid[r][c] = v; }
    void setDiscovered(int r, int c, bool v) { disc[r][c] = v; }

    void resetField() {
        x = y = 0; battery = 100;
        grid.assign(N, std::string(N, 'H'));
        disc.assign(N, std::vector<bool>(N, false));
        disc[0][0] = true;

        std::vector<std::pair<int, int>> spots;
        for (int r = 0; r < N; ++r)
            for (int c = 0; c < N; ++c)
                if (r || c) spots.push_back({r, c});

        unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
        std::shuffle(spots.begin(), spots.end(), std::default_random_engine(seed));

        for (int i = 0; i < 3; ++i) grid[spots[i].first][spots[i].second] = 'W';
        for (int i = 3; i < 5; ++i) grid[spots[i].first][spots[i].second] = 'D';
        msg = "Field reset: 3 Weeds & 2 Blight patches masked under Fog of War.";
    }
};

class MoveCommand : public ActionCommand {
    std::string dir;
    int px, py, tx, ty, pb;
public:
    MoveCommand(std::string d, int px=0, int py=0, int tx=0, int ty=0, int pb=0)
        : dir(d), px(px), py(py), tx(tx), ty(ty), pb(pb) {}

    bool execute(AgriBot& b) override {
        if (b.getBattery() < 1) { b.setMessage("Battery depleted."); return false; }
        px = b.getX(); py = b.getY(); pb = b.getBattery();
        tx = px + (dir == "down") - (dir == "up");
        ty = py + (dir == "right") - (dir == "left");

        // Specific directional perimeter warnings
        if (tx < 0) { b.setMessage("Boundary limit: Cannot move UP (Already at top edge)."); return false; }
        if (tx >= N) { b.setMessage("Boundary limit: Cannot move DOWN (Already at bottom edge)."); return false; }
        if (ty < 0) { b.setMessage("Boundary limit: Cannot move LEFT (Already at west edge)."); return false; }
        if (ty >= N) { b.setMessage("Boundary limit: Cannot move RIGHT (Already at east edge)."); return false; }

        b.setPos(tx, ty); b.setBattery(pb - 1);
        b.setMessage("Moved " + dir + " to [" + std::to_string(tx) + "," + std::to_string(ty) + "].");
        return true;
    }
    void redo(AgriBot& b) override { b.setPos(tx, ty); b.setBattery(pb - 1); b.setMessage("Redo move to [" + std::to_string(tx) + "," + std::to_string(ty) + "]."); }
    void undo(AgriBot& b) override { b.setPos(px, py); b.setBattery(pb); b.setMessage("Undo move to [" + std::to_string(px) + "," + std::to_string(py) + "]."); }
    std::string serialize() const override {
        return "move:" + dir + ":" + std::to_string(px) + ":" + std::to_string(py) + ":" + std::to_string(tx) + ":" + std::to_string(ty) + ":" + std::to_string(pb);
    }
};

class InspectCommand : public ActionCommand {
    int x, y, pb; bool prevDisc;
public:
    InspectCommand(int x=0, int y=0, int pb=0, bool d=false) : x(x), y(y), pb(pb), prevDisc(d) {}
    bool execute(AgriBot& b) override {
        if (b.getBattery() < 1) { b.setMessage("Battery too low for scan."); return false; }
        x = b.getX(); y = b.getY(); pb = b.getBattery(); prevDisc = b.isDiscovered(x, y);
        b.setBattery(pb - 1); b.setDiscovered(x, y, true);
        char c = b.getCell(x, y);
        b.setMessage("Scan [" + std::to_string(x) + "," + std::to_string(y) + "]: " + (c == 'H' ? "Healthy Foliage" : (c == 'W' ? "Weed Infestation!" : "Blight Pathogen!")));
        return true;
    }
    void redo(AgriBot& b) override { b.setBattery(pb - 1); b.setDiscovered(x, y, true); b.setMessage("Redo scan [" + std::to_string(x) + "," + std::to_string(y) + "]."); }
    void undo(AgriBot& b) override { b.setBattery(pb); b.setDiscovered(x, y, prevDisc); b.setMessage("Undo scan [" + std::to_string(x) + "," + std::to_string(y) + "]."); }
    std::string serialize() const override {
        return "inspect:" + std::to_string(x) + ":" + std::to_string(y) + ":" + std::to_string(pb) + ":" + (prevDisc ? "1" : "0");
    }
};

class SprayCommand : public ActionCommand {
    int x, y, pb; char prevStatus;
public:
    SprayCommand(int x=0, int y=0, char ps='H', int pb=0) : x(x), y(y), pb(pb), prevStatus(ps) {}
    bool execute(AgriBot& b) override {
        x = b.getX(); y = b.getY(); pb = b.getBattery(); prevStatus = b.getCell(x, y);
        if (!b.isDiscovered(x, y)) { b.setMessage("Safety Lock: Scan tile first!"); return false; }
        if (pb < 3) { b.setMessage("Low battery for spray."); return false; }
        if (prevStatus == 'H') { b.setMessage("Target healthy. Spray skipped."); return false; }
        b.setCell(x, y, 'H'); b.setBattery(pb - 3);
        b.setMessage("Neutralized target at [" + std::to_string(x) + "," + std::to_string(y) + "].");
        return true;
    }
    void redo(AgriBot& b) override { b.setCell(x, y, 'H'); b.setBattery(pb - 3); b.setMessage("Redo spray at [" + std::to_string(x) + "," + std::to_string(y) + "]."); }
    void undo(AgriBot& b) override { b.setCell(x, y, prevStatus); b.setBattery(pb); b.setMessage("Undo spray at [" + std::to_string(x) + "," + std::to_string(y) + "]."); }
    std::string serialize() const override {
        return "spray:" + std::to_string(x) + ":" + std::to_string(y) + ":" + std::string(1, prevStatus) + ":" + std::to_string(pb);
    }
};

class RechargeCommand : public ActionCommand {
    int pb;
public:
    RechargeCommand(int pb=0) : pb(pb) {}
    bool execute(AgriBot& b) override {
        if (b.getX() || b.getY()) { b.setMessage("Dock recharge only at [0,0]."); return false; }
        pb = b.getBattery(); b.setBattery(100);
        b.setMessage("Docked at [0,0]. Battery charged to 100%.");
        return true;
    }
    void redo(AgriBot& b) override { b.setBattery(100); b.setMessage("Redo solar recharge."); }
    void undo(AgriBot& b) override { b.setBattery(pb); b.setMessage("Undo solar recharge."); }
    std::string serialize() const override { return "recharge:" + std::to_string(pb); }
};

std::shared_ptr<ActionCommand> deserialize(const std::string& s) {
    std::stringstream ss(s); std::string type;
    std::getline(ss, type, ':');
    if (type == "move") {
        std::string dir, px, py, tx, ty, pb;
        std::getline(ss, dir, ':');
        if (std::getline(ss, px, ':') && std::getline(ss, py, ':') &&
            std::getline(ss, tx, ':') && std::getline(ss, ty, ':') && std::getline(ss, pb, ':')) {
            return std::make_shared<MoveCommand>(dir, std::stoi(px), std::stoi(py), std::stoi(tx), std::stoi(ty), std::stoi(pb));
        }
        return std::make_shared<MoveCommand>(dir.empty() ? "up" : dir);
    }
    if (type == "inspect") {
        std::string x, y, pb, d;
        if (std::getline(ss, x, ':') && std::getline(ss, y, ':') && std::getline(ss, pb, ':') && std::getline(ss, d, ':')) {
            return std::make_shared<InspectCommand>(std::stoi(x), std::stoi(y), std::stoi(pb), d == "1");
        }
        return std::make_shared<InspectCommand>();
    }
    if (type == "spray") {
        std::string x, y, ps, pb;
        if (std::getline(ss, x, ':') && std::getline(ss, y, ':') && std::getline(ss, ps, ':') && std::getline(ss, pb, ':')) {
            return std::make_shared<SprayCommand>(std::stoi(x), std::stoi(y), ps[0], std::stoi(pb));
        }
        return std::make_shared<SprayCommand>();
    }
    if (type == "recharge") {
        std::string pb;
        if (std::getline(ss, pb, ':')) return std::make_shared<RechargeCommand>(std::stoi(pb));
        return std::make_shared<RechargeCommand>();
    }
    return nullptr;
}

int main(int argc, char* argv[]) {
    if (argc < 3) return 1;
    std::string stateFile = argv[1], act = argv[2];

    AgriBot bot;
    std::vector<std::string> undoStack, redoStack, queueList;

    std::ifstream in(stateFile);
    if (in.is_open()) {
        int x, y, batt, u, r, q;
        if (in >> x >> y >> batt) {
            bot.setPos(x, y); bot.setBattery(batt);
            for (int i = 0; i < N; ++i) { std::string row; in >> row; for (int j = 0; j < N; ++j) bot.setCell(i, j, row[j]); }
            for (int i = 0; i < N; ++i) { std::string row; in >> row; for (int j = 0; j < N; ++j) bot.setDiscovered(i, j, row[j] == '1'); }
            if (in >> u) { undoStack.resize(u); for (auto& s : undoStack) in >> s; }
            if (in >> r) { redoStack.resize(r); for (auto& s : redoStack) in >> s; }
            if (in >> q) { queueList.resize(q); for (auto& s : queueList) in >> s; }
        }
        in.close();
    }

    if (act == "reset") { 
        bot.resetField(); 
        undoStack.clear(); 
        redoStack.clear(); 
        queueList.clear(); 
    }
    else if (act == "move" || act == "spray" || act == "inspect" || act == "recharge") {
        std::shared_ptr<ActionCommand> cmd = nullptr;
        if (act == "move") {
            std::string dir = "up";
            if (argc >= 4 && std::string(argv[3]).rfind("dir=", 0) == 0) dir = std::string(argv[3]).substr(4);
            cmd = std::make_shared<MoveCommand>(dir);
        } else if (act == "spray") cmd = std::make_shared<SprayCommand>();
        else if (act == "inspect") cmd = std::make_shared<InspectCommand>();
        else if (act == "recharge") cmd = std::make_shared<RechargeCommand>();

        if (cmd && cmd->execute(bot)) { 
            undoStack.push_back(cmd->serialize()); 
            redoStack.clear(); 
        }
    }
    else if (act == "undo") {
        if (undoStack.empty()) {
            bot.setMessage("Undo Stack is empty. Nothing to undo!");
        } else {
            auto cmd = deserialize(undoStack.back());
            undoStack.pop_back();
            if (cmd) { cmd->undo(bot); redoStack.push_back(cmd->serialize()); }
        }
    }
    else if (act == "redo") {
        if (redoStack.empty()) {
            bot.setMessage("Redo Stack is empty. Nothing to redo!");
        } else {
            auto cmd = deserialize(redoStack.back());
            redoStack.pop_back();
            if (cmd) { cmd->redo(bot); undoStack.push_back(cmd->serialize()); }
        }
    }
    else if (act == "queue_add" && argc >= 4) {
        std::string t = "inspect", d = "";
        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg.rfind("type=", 0) == 0) t = arg.substr(5);
            if (arg.rfind("dir=", 0) == 0) d = arg.substr(4);
        }
        queueList.push_back(t == "move" ? "move:" + d : t);
        bot.setMessage("Enqueued: " + queueList.back());
    }
    else if (act == "queue_next") {
        if (queueList.empty()) {
            bot.setMessage("Pipeline is empty. No instructions to run.");
        } else {
            std::string nextCmd = queueList.front();
            queueList.erase(queueList.begin());
            auto cmd = deserialize(nextCmd);
            if (cmd && cmd->execute(bot)) { 
                undoStack.push_back(cmd->serialize()); 
                redoStack.clear(); 
            }
        }
    }

    std::ofstream out(stateFile);
    if (out.is_open()) {
        out << bot.getX() << " " << bot.getY() << " " << bot.getBattery() << "\n";
        for (int i = 0; i < N; ++i) { for (int j = 0; j < N; ++j) out << bot.getCell(i, j); out << "\n"; }
        for (int i = 0; i < N; ++i) { for (int j = 0; j < N; ++j) out << (bot.isDiscovered(i, j) ? '1' : '0'); out << "\n"; }
        out << undoStack.size() << "\n"; for (auto& s : undoStack) out << s << "\n";
        out << redoStack.size() << "\n"; for (auto& s : redoStack) out << s << "\n";
        out << queueList.size() << "\n"; for (auto& s : queueList) out << s << "\n";
        out.close();
    }

    std::cout << "{\"x\":" << bot.getX() << ",\"y\":" << bot.getY() << ",\"battery\":" << bot.getBattery() << ",\"msg\":\"" << bot.getMessage() << "\",\"grid\":[";
    for (int i = 0; i < N; ++i) {
        std::cout << "\""; for (int j = 0; j < N; ++j) std::cout << bot.getCell(i, j);
        std::cout << "\"" << (i + 1 < N ? "," : "");
    }
    std::cout << "],\"discovered\":[";
    for (int i = 0; i < N; ++i) {
        std::cout << "["; for (int j = 0; j < N; ++j) std::cout << (bot.isDiscovered(i, j) ? "true" : "false") << (j + 1 < N ? "," : "");
        std::cout << "]" << (i + 1 < N ? "," : "");
    }
    std::cout << "],\"undo\":[";
    for (size_t i = 0; i < undoStack.size(); ++i) std::cout << "\"" << undoStack[i] << "\"" << (i + 1 < undoStack.size() ? "," : "");
    std::cout << "],\"redo\":[";
    for (size_t i = 0; i < redoStack.size(); ++i) std::cout << "\"" << redoStack[i] << "\"" << (i + 1 < redoStack.size() ? "," : "");
    std::cout << "],\"queue\":[";
    for (size_t i = 0; i < queueList.size(); ++i) std::cout << "\"" << queueList[i] << "\"" << (i + 1 < queueList.size() ? "," : "");
    std::cout << "]}" << std::endl;
    return 0;
}
