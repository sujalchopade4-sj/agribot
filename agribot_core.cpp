#include <bits/stdc++.h>
using namespace std;

const int N = 5;
class AgriBot;

class ActionCommand {
public:
    virtual ~ActionCommand() = default;
    virtual bool execute(AgriBot& b) = 0;
    virtual void undo(AgriBot& b) = 0;
    virtual void redo(AgriBot& b) { execute(b); }
    virtual string serialize() const = 0;
};

class AgriBot {
private:
    int x = 0, y = 0, battery = 100;
    vector<string> grid;
    vector<vector<bool>> disc;
    string msg = "Ready at Solar Dock [0,0].";

public:
    AgriBot() { resetField(); }

    int getX() const { return x; }
    int getY() const { return y; }
    int getBattery() const { return battery; }
    string getMessage() const { return msg; }
    char getCell(int r, int c) const { return grid[r][c]; }
    bool isDiscovered(int r, int c) const { return disc[r][c]; }

    void setMessage(const string& m) { msg = m; }
    void setBattery(int b) { battery = clamp(b, 0, 100); }
    void setPos(int nx, int ny) { x = clamp(nx, 0, N - 1); y = clamp(ny, 0, N - 1); }
    void setCell(int r, int c, char v) { grid[r][c] = v; }
    void setDiscovered(int r, int c, bool v) { disc[r][c] = v; }

    void resetField() {
        x = y = 0; battery = 100;
        grid.assign(N, string(N, 'H'));
        disc.assign(N, vector<bool>(N, false));
        disc[0][0] = true;

        vector<pair<int, int>> spots;
        for (int r = 0; r < N; ++r)
            for (int c = 0; c < N; ++c)
                if (r || c) spots.push_back({r, c});

        unsigned seed = chrono::system_clock::now().time_since_epoch().count();
        shuffle(spots.begin(), spots.end(), default_random_engine(seed));

        for (int i = 0; i < 3; ++i) grid[spots[i].first][spots[i].second] = 'W';
        for (int i = 3; i < 5; ++i) grid[spots[i].first][spots[i].second] = 'D';
        msg = "Field reset: 3 Weeds & 2 Blight patches masked under Fog of War.";
    }
};

class MoveCommand : public ActionCommand {
    string dir;
    int px, py, tx, ty, pb;
public:
    MoveCommand(string d, int px=0, int py=0, int tx=0, int ty=0, int pb=0)
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
        b.setMessage("Moved " + dir + " to [" + to_string(tx) + "," + to_string(ty) + "].");
        return true;
    }
    void redo(AgriBot& b) override { b.setPos(tx, ty); b.setBattery(pb - 1); b.setMessage("Redo move to [" + to_string(tx) + "," + to_string(ty) + "]."); }
    void undo(AgriBot& b) override { b.setPos(px, py); b.setBattery(pb); b.setMessage("Undo move to [" + to_string(px) + "," + to_string(py) + "]."); }
    string serialize() const override {
        return "move:" + dir + ":" + to_string(px) + ":" + to_string(py) + ":" + to_string(tx) + ":" + to_string(ty) + ":" + to_string(pb);
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
        b.setMessage("Scan [" + to_string(x) + "," + to_string(y) + "]: " + (c == 'H' ? "Healthy Foliage" : (c == 'W' ? "Weed Infestation!" : "Blight Pathogen!")));
        return true;
    }
    void redo(AgriBot& b) override { b.setBattery(pb - 1); b.setDiscovered(x, y, true); b.setMessage("Redo scan [" + to_string(x) + "," + to_string(y) + "]."); }
    void undo(AgriBot& b) override { b.setBattery(pb); b.setDiscovered(x, y, prevDisc); b.setMessage("Undo scan [" + to_string(x) + "," + to_string(y) + "]."); }
    string serialize() const override {
        return "inspect:" + to_string(x) + ":" + to_string(y) + ":" + to_string(pb) + ":" + (prevDisc ? "1" : "0");
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
        b.setMessage("Neutralized target at [" + to_string(x) + "," + to_string(y) + "].");
        return true;
    }
    void redo(AgriBot& b) override { b.setCell(x, y, 'H'); b.setBattery(pb - 3); b.setMessage("Redo spray at [" + to_string(x) + "," + to_string(y) + "]."); }
    void undo(AgriBot& b) override { b.setCell(x, y, prevStatus); b.setBattery(pb); b.setMessage("Undo spray at [" + to_string(x) + "," + to_string(y) + "]."); }
    string serialize() const override {
        return "spray:" + to_string(x) + ":" + to_string(y) + ":" + string(1, prevStatus) + ":" + to_string(pb);
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
    string serialize() const override { return "recharge:" + to_string(pb); }
};

shared_ptr<ActionCommand> deserialize(const string& s) {
    stringstream ss(s); string type;
    getline(ss, type, ':');
    if (type == "move") {
        string dir, px, py, tx, ty, pb;
        getline(ss, dir, ':');
        if (getline(ss, px, ':') && getline(ss, py, ':') &&
            getline(ss, tx, ':') && getline(ss, ty, ':') && getline(ss, pb, ':')) {
            return make_shared<MoveCommand>(dir, stoi(px), stoi(py), stoi(tx), stoi(ty), stoi(pb));
        }
        return make_shared<MoveCommand>(dir.empty() ? "up" : dir);
    }
    if (type == "inspect") {
        string x, y, pb, d;
        if (getline(ss, x, ':') && getline(ss, y, ':') && getline(ss, pb, ':') && getline(ss, d, ':')) {
            return make_shared<InspectCommand>(stoi(x), stoi(y), stoi(pb), d == "1");
        }
        return make_shared<InspectCommand>();
    }
    if (type == "spray") {
        string x, y, ps, pb;
        if (getline(ss, x, ':') && getline(ss, y, ':') && getline(ss, ps, ':') && getline(ss, pb, ':')) {
            return make_shared<SprayCommand>(stoi(x), stoi(y), ps[0], stoi(pb));
        }
        return make_shared<SprayCommand>();
    }
    if (type == "recharge") {
        string pb;
        if (getline(ss, pb, ':')) return make_shared<RechargeCommand>(stoi(pb));
        return make_shared<RechargeCommand>();
    }
    return nullptr;
}

int main(int argc, char* argv[]) {
    if (argc < 3) return 1;
    string stateFile = argv[1], act = argv[2];

    AgriBot bot;
    vector<string> undoStack, redoStack, queueList;

    ifstream in(stateFile);
    if (in.is_open()) {
        int x, y, batt, u, r, q;
        if (in >> x >> y >> batt) {
            bot.setPos(x, y); bot.setBattery(batt);
            for (int i = 0; i < N; ++i) { string row; in >> row; for (int j = 0; j < N; ++j) bot.setCell(i, j, row[j]); }
            for (int i = 0; i < N; ++i) { string row; in >> row; for (int j = 0; j < N; ++j) bot.setDiscovered(i, j, row[j] == '1'); }
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
        shared_ptr<ActionCommand> cmd = nullptr;
        if (act == "move") {
            string dir = "up";
            if (argc >= 4 && string(argv[3]).rfind("dir=", 0) == 0) dir = string(argv[3]).substr(4);
            cmd = make_shared<MoveCommand>(dir);
        } else if (act == "spray") cmd = make_shared<SprayCommand>();
        else if (act == "inspect") cmd = make_shared<InspectCommand>();
        else if (act == "recharge") cmd = make_shared<RechargeCommand>();

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
        string t = "inspect", d = "";
        for (int i = 3; i < argc; ++i) {
            string arg = argv[i];
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
            string nextCmd = queueList.front();
            queueList.erase(queueList.begin());
            auto cmd = deserialize(nextCmd);
            if (cmd && cmd->execute(bot)) { 
                undoStack.push_back(cmd->serialize()); 
                redoStack.clear(); 
            }
        }
    }

    ofstream out(stateFile);
    if (out.is_open()) {
        out << bot.getX() << " " << bot.getY() << " " << bot.getBattery() << "\n";
        for (int i = 0; i < N; ++i) { for (int j = 0; j < N; ++j) out << bot.getCell(i, j); out << "\n"; }
        for (int i = 0; i < N; ++i) { for (int j = 0; j < N; ++j) out << (bot.isDiscovered(i, j) ? '1' : '0'); out << "\n"; }
        out << undoStack.size() << "\n"; for (auto& s : undoStack) out << s << "\n";
        out << redoStack.size() << "\n"; for (auto& s : redoStack) out << s << "\n";
        out << queueList.size() << "\n"; for (auto& s : queueList) out << s << "\n";
        out.close();
    }

    cout << "{\"x\":" << bot.getX() << ",\"y\":" << bot.getY() << ",\"battery\":" << bot.getBattery() << ",\"msg\":\"" << bot.getMessage() << "\",\"grid\":[";
    for (int i = 0; i < N; ++i) {
        cout << "\""; for (int j = 0; j < N; ++j) cout << bot.getCell(i, j);
        cout << "\"" << (i + 1 < N ? "," : "");
    }
    cout << "],\"discovered\":[";
    for (int i = 0; i < N; ++i) {
        cout << "["; for (int j = 0; j < N; ++j) cout << (bot.isDiscovered(i, j) ? "true" : "false") << (j + 1 < N ? "," : "");
        cout << "]" << (i + 1 < N ? "," : "");
    }
    cout << "],\"undo\":[";
    for (size_t i = 0; i < undoStack.size(); ++i) cout << "\"" << undoStack[i] << "\"" << (i + 1 < undoStack.size() ? "," : "");
    cout << "],\"redo\":[";
    for (size_t i = 0; i < redoStack.size(); ++i) cout << "\"" << redoStack[i] << "\"" << (i + 1 < redoStack.size() ? "," : "");
    cout << "],\"queue\":[";
    for (size_t i = 0; i < queueList.size(); ++i) cout << "\"" << queueList[i] << "\"" << (i + 1 < queueList.size() ? "," : "");
    cout << "]}" << endl;
    return 0;
}
