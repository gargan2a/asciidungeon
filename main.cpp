#include <iostream>
#include <random>
#include <vector>
#include <map>
#include <tuple>
#include <queue>
#include <conio.h>
#include <Windows.h>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <cctype>

using namespace std;

// ----------------- Global Settings -----------------
random_device rd;
mt19937 rng(rd());
const size_t GameFieldWidth = 80;
const size_t GameFieldHeight = 30;
const char TileWall = '#';
const char TileGround = ' ';
const char TilePlayer = 'P';
const char TileEnemy = 'E';
const char TileMerchant = 'M';
const char TileBoss = 'B';
const char TileMiniBoss = 'b';

struct GridPosition {
    int x, y;
    GridPosition(int xPos = 0, int yPos = 0) : x(xPos), y(yPos) {}
    bool operator<(const GridPosition& other) const { return tie(y, x) < tie(other.y, other.x); }
    bool operator==(const GridPosition& other) const { return x == other.x && y == other.y; }
};

struct Player {
    int hp, maxHp, attack, defense, level;
};

// ----------------- Engine Class -----------------
class Engine {
public:
    // ----------------- Console -----------------
    static void HideCursor() {
        HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_CURSOR_INFO cursorInfo;
        GetConsoleCursorInfo(consoleHandle, &cursorInfo);
        cursorInfo.bVisible = FALSE;
        SetConsoleCursorInfo(consoleHandle, &cursorInfo);
    }

    static void SetConsoleSize(int width, int height, int marginX = 2) {
        HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        COORD bufferSize = { (SHORT)(width + marginX * 2), (SHORT)(height + 5) };
        SetConsoleScreenBufferSize(consoleHandle, bufferSize);
        SMALL_RECT windowSize = { 0, 0, bufferSize.X - 1, bufferSize.Y - 1 };
        SetConsoleWindowInfo(consoleHandle, TRUE, &windowSize);

        HWND hwnd = GetConsoleWindow();
        LONG style = GetWindowLong(hwnd, GWL_STYLE);
        style &= ~WS_MAXIMIZEBOX;
        style &= ~WS_SIZEBOX;
        SetWindowLong(hwnd, GWL_STYLE, style);
    }

    // ----------------- Level Generator -----------------
    class LevelGenerator {
    public:
        static int RandomInt(int minValue, int maxValue) {
            uniform_int_distribution<> dist(minValue, maxValue);
            return dist(rng);
        }

        static vector<char> GenerateLevel() {
            vector<char> level(GameFieldWidth * GameFieldHeight, TileGround);
            for (int x = 0; x < GameFieldWidth; ++x) {
                level[x] = TileWall;
                level[(GameFieldHeight - 1) * GameFieldWidth + x] = TileWall;
            }
            for (int y = 0; y < GameFieldHeight; ++y) {
                level[y * GameFieldWidth] = TileWall;
                level[y * GameFieldWidth + (GameFieldWidth - 1)] = TileWall;
            }

            int playerStartX = 1, playerStartY = 1;
            int walkableTiles = GameFieldWidth * GameFieldHeight - 2 * GameFieldWidth - 2 * (GameFieldHeight - 2) - 1;
            int maxClusters = 150;

            for (int attempt = 0; attempt < maxClusters; ++attempt) {
                int clusterWidth = RandomInt(2, 4);
                int clusterHeight = RandomInt(2, 4);
                int clusterX = RandomInt(1, GameFieldWidth - clusterWidth - 2);
                int clusterY = RandomInt(1, GameFieldHeight - clusterHeight - 2);

                vector<int> clusterIndices;
                for (int dy = 0; dy < clusterHeight; ++dy)
                    for (int dx = 0; dx < clusterWidth; ++dx) {
                        int index = (clusterY + dy) * GameFieldWidth + (clusterX + dx);
                        if (level[index] == TileGround) {
                            clusterIndices.push_back(index);
                            level[index] = TileWall;
                        }
                    }

                vector<bool> visited(GameFieldWidth * GameFieldHeight, false);
                queue<GridPosition> bfsQueue;
                bfsQueue.push({ playerStartX, playerStartY });
                visited[playerStartY * GameFieldWidth + playerStartX] = true;
                int reachableCount = 1;
                const int dxArr[] = { 1, -1, 0, 0 };
                const int dyArr[] = { 0, 0, 1, -1 };

                while (!bfsQueue.empty()) {
                    GridPosition current = bfsQueue.front(); bfsQueue.pop();
                    for (int d = 0; d < 4; ++d) {
                        int nx = current.x + dxArr[d];
                        int ny = current.y + dyArr[d];
                        if (nx > 0 && nx < GameFieldWidth - 1 && ny > 0 && ny < GameFieldHeight - 1) {
                            int idx = ny * GameFieldWidth + nx;
                            if (!visited[idx] && level[idx] == TileGround) {
                                visited[idx] = true;
                                ++reachableCount;
                                bfsQueue.push({ nx, ny });
                            }
                        }
                    }
                }

                if (reachableCount < walkableTiles - (int)clusterIndices.size()) {
                    for (int idx : clusterIndices) level[idx] = TileGround;
                }
                else {
                    walkableTiles -= (int)clusterIndices.size();
                }
            }

            return level;
        }
    };

    // ----------------- Level Renderer -----------------
    class LevelRenderer {
    public:
        static void DrawInitialMap(const vector<char>& levelData, const map<GridPosition, char>& entityMap, int marginX = 2);
        static void DrawRow(const vector<char>& levelData, const map<GridPosition, char>& entityMap, int row, int marginX = 2);
        static void DrawChangedRows(const vector<char>& levelData, const map<GridPosition, char>& entityMap, int prevRow, int curRow, int marginX = 2);
    private:
        static void SetTileColor(HANDLE hConsole, char ch);
    };
};

// ----------------- LevelRenderer Definitions -----------------
void Engine::LevelRenderer::DrawInitialMap(const vector<char>& levelData, const map<GridPosition, char>& entityMap, int marginX) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD pos = { 0, 0 };
    SetConsoleCursorPosition(hConsole, pos);

    for (int y = 0; y < GameFieldHeight; ++y) {
        for (int i = 0; i < marginX; ++i) cout << ' ';
        for (int x = 0; x < GameFieldWidth; ++x) {
            GridPosition gp{ x, y };
            char ch = entityMap.count(gp) ? entityMap.at(gp) : levelData[y * GameFieldWidth + x];
            SetTileColor(hConsole, ch);
            cout << ch;
        }
        cout << '\n';
    }
    SetConsoleTextAttribute(hConsole, 7);
}

void Engine::LevelRenderer::DrawRow(const vector<char>& levelData, const map<GridPosition, char>& entityMap, int row, int marginX) {
    if (row < 0 || row >= GameFieldHeight) return;
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD pos = { 0, (SHORT)row };
    SetConsoleCursorPosition(hConsole, pos);

    for (int i = 0; i < marginX; ++i) cout << ' ';
    for (int x = 0; x < GameFieldWidth; ++x) {
        GridPosition gp{ x, row };
        char ch = entityMap.count(gp) ? entityMap.at(gp) : levelData[row * GameFieldWidth + x];
        SetTileColor(hConsole, ch);
        cout << ch;
    }
    SetConsoleTextAttribute(hConsole, 7);
}

void Engine::LevelRenderer::DrawChangedRows(const vector<char>& levelData, const map<GridPosition, char>& entityMap, int prevRow, int curRow, int marginX) {
    if (prevRow != curRow) DrawRow(levelData, entityMap, prevRow, marginX);
    DrawRow(levelData, entityMap, curRow, marginX);
}

void Engine::LevelRenderer::SetTileColor(HANDLE hConsole, char ch) {
    switch (ch) {
    case TileWall: SetConsoleTextAttribute(hConsole, 8); break;
    case TilePlayer: SetConsoleTextAttribute(hConsole, 9); break;
    case TileEnemy: SetConsoleTextAttribute(hConsole, 14); break;
    case TileMerchant: SetConsoleTextAttribute(hConsole, 11); break;
    case TileMiniBoss: SetConsoleTextAttribute(hConsole, 13); break;
    case TileBoss: SetConsoleTextAttribute(hConsole, 4); break;
    default: SetConsoleTextAttribute(hConsole, 7); break;
    }
}

// ----------------- Game Class -----------------
class Game {
public:
    vector<char> LevelData;
    map<GridPosition, char> EntityMap;
    Player player;
    GridPosition playerPos;
    enum class Direction { Up, Down, Left, Right, None };

    Game() {
        player = { 100, 100, 10, 5, 1 };
        playerPos = { 1, 1 };
        GenerateNewLevel();
    }

    // ----------------- HUD -----------------
    void DrawHUD(const string& message = "") {
        string msg = message.empty() ? PlayerStatus() : message;
        HUDBar::DrawHUDBar(2, msg);
    }

    string PlayerStatus() const {
        stringstream ss;
        ss << "HP:" << player.hp << "/" << player.maxHp
            << " ATK:" << player.attack
            << " DEF:" << player.defense
            << " LVL:" << player.level;
        return ss.str();
    }

    static string PlayerStatusStatic(const Player& player) {
        stringstream ss;
        ss << "HP:" << player.hp << "/" << player.maxHp
            << " ATK:" << player.attack
            << " DEF:" << player.defense
            << " LVL:" << player.level;
        return ss.str();
    }

    // ----------------- HUDBar Nested Class -----------------
    class HUDBar {
    public:
        static string lastMessage; // keep track of last drawn HUD

        static void DrawHUDBar(int marginX, const string& message) {
            if (message == lastMessage) return; // no redraw if same
            lastMessage = message;

            HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
            SetConsoleTextAttribute(consoleHandle, 7);
            COORD pos = { 0, (SHORT)(GameFieldHeight + 1) };
            SetConsoleCursorPosition(consoleHandle, pos);
            string margin(marginX, ' ');

            // Top border
            cout << margin;
            for (int i = 0; i < GameFieldWidth; ++i) cout << '=';
            cout << '\n';

            // Message line
            cout << margin;
            SetConsoleTextAttribute(consoleHandle, 14); // Yellow
            cout << message;
            if ((int)message.size() < GameFieldWidth)
                cout << string(GameFieldWidth - message.size(), ' ');
            cout << '\n';

            // Bottom border
            SetConsoleTextAttribute(consoleHandle, 7);
            cout << margin;
            for (int i = 0; i < GameFieldWidth; ++i) cout << '=';
            cout << '\n';
        }

        static void ClearHUDBar(int marginX) {
            HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
            SetConsoleTextAttribute(consoleHandle, 7);
            COORD pos = { 0, (SHORT)(GameFieldHeight + 1) };
            SetConsoleCursorPosition(consoleHandle, pos);
            string margin(marginX, ' ');

            for (int i = 0; i < 3; ++i) {
                cout << margin;
                for (int j = 0; j < GameFieldWidth; ++j) cout << ' ';
                cout << '\n';
            }
            lastMessage = ""; // reset last message
        }
    };

    // ----------------- EntityManager -----------------
    class EntityManager {
    public:
        class Encounters {
        public:
            static inline bool showingMessage = false;
            static inline chrono::steady_clock::time_point lastMessageTime;
            static inline string currentMessage = "";

            static void HandleEncounter(char entityType) {
                switch (entityType) {
                case TileEnemy: currentMessage = "Encountered an enemy!"; break;
                case TileMerchant: currentMessage = "Encountered a merchant!"; break;
                case TileBoss: currentMessage = "Encountered a boss!"; break;
                case TileMiniBoss: currentMessage = "Encountered a miniboss!"; break;
                default: currentMessage = ""; return;
                }
                showingMessage = true;
                lastMessageTime = chrono::steady_clock::now();

                // Draw message line only
                Game::HUDBar::DrawHUDBar(2, currentMessage);
            }

            static void UpdateHUD(Player& player) {
                using namespace chrono;
                if (showingMessage) {
                    auto now = steady_clock::now();
                    if (duration_cast<milliseconds>(now - lastMessageTime).count() >= 2000) {
                        showingMessage = false;
                        Game::HUDBar::DrawHUDBar(2, Game::PlayerStatusStatic(player));
                    }
                }
            }
        };

        static vector<GridPosition> GetWalkableTiles(const vector<char>& levelData) {
            vector<GridPosition> walkable;
            for (int y = 0; y < GameFieldHeight; ++y)
                for (int x = 0; x < GameFieldWidth; ++x)
                    if (levelData[y * GameFieldWidth + x] == TileGround)
                        walkable.push_back({ x, y });
            return walkable;
        }

        static void PlaceEntitiesRandomly(vector<char>& levelData, map<GridPosition, char>& entityMap, char entityChar, int count) {
            auto walkable = GetWalkableTiles(levelData);
            shuffle(walkable.begin(), walkable.end(), rng);
            int placed = 0;
            for (auto& pos : walkable) {
                if (entityMap.count(pos)) continue;
                entityMap[pos] = entityChar;
                if (++placed >= count) break;
            }
        }

        static void UpdateEntities(vector<char>& levelData, map<GridPosition, char>& entityMap, GridPosition playerPos) {
            vector<GridPosition> positions;
            for (auto& kv : entityMap) if (kv.second != TilePlayer) positions.push_back(kv.first);

            for (auto& pos : positions) {
                if (!entityMap.count(pos)) continue;
                char type = entityMap[pos];
                GridPosition newPos = pos;
                vector<Direction> dirs = { Direction::Up, Direction::Down, Direction::Left, Direction::Right };
                shuffle(dirs.begin(), dirs.end(), rng);
                bool moved = false;
                for (auto dir : dirs) {
                    newPos = pos;
                    switch (dir) {
                    case Direction::Up: newPos.y--; break;
                    case Direction::Down: newPos.y++; break;
                    case Direction::Left: newPos.x--; break;
                    case Direction::Right: newPos.x++; break;
                    default: break;
                    }
                    if (newPos.x < 0 || newPos.x >= GameFieldWidth || newPos.y < 0 || newPos.y >= GameFieldHeight) continue;
                    if (levelData[newPos.y * GameFieldWidth + newPos.x] != TileGround) continue;
                    if (newPos == playerPos) continue;
                    if (entityMap.count(newPos)) continue;
                    moved = true; break;
                }
                if (moved) {
                    entityMap.erase(pos);
                    entityMap[newPos] = type;
                    Engine::LevelRenderer::DrawChangedRows(levelData, entityMap, pos.y, newPos.y);
                }
            }
        }
    };

    // ----------------- Player Movement -----------------
    static bool CanMove(const GridPosition& pos, const vector<char>& levelData) {
        if (pos.x < 0 || pos.x >= GameFieldWidth || pos.y < 0 || pos.y >= GameFieldHeight) return false;
        return levelData[pos.y * GameFieldWidth + pos.x] == TileGround;
    }

    static Direction GetMoveDirection() {
        if (_kbhit()) {
            char key = _getch();
            switch (tolower(key)) {
            case 'w': return Direction::Up;
            case 's': return Direction::Down;
            case 'a': return Direction::Left;
            case 'd': return Direction::Right;
            case 'q': exit(0);
            default: return Direction::None;
            }
        }
        return Direction::None;
    }

    void MovePlayer(Direction dir) {
        GridPosition newPos = playerPos;
        switch (dir) {
        case Direction::Up: newPos.y--; break;
        case Direction::Down: newPos.y++; break;
        case Direction::Left: newPos.x--; break;
        case Direction::Right: newPos.x++; break;
        default: return;
        }
        if (!CanMove(newPos, LevelData)) return;
        auto it = EntityMap.find(newPos);
        if (it != EntityMap.end()) {
            EntityManager::Encounters::HandleEncounter(it->second);
            EntityMap.erase(newPos);
        }
        EntityMap.erase(playerPos);
        EntityMap[newPos] = TilePlayer;
        Engine::LevelRenderer::DrawChangedRows(LevelData, EntityMap, playerPos.y, newPos.y);
        playerPos = newPos;
    }

    // ----------------- Generate New Level -----------------
    void GenerateNewLevel() {
        LevelData = Engine::LevelGenerator::GenerateLevel();
        EntityMap.clear();
        EntityMap[playerPos] = TilePlayer;
        EntityManager::PlaceEntitiesRandomly(LevelData, EntityMap, TileEnemy, 5);
        EntityManager::PlaceEntitiesRandomly(LevelData, EntityMap, TileMerchant, 5);
        EntityManager::PlaceEntitiesRandomly(LevelData, EntityMap, TileBoss, 5);
        EntityManager::PlaceEntitiesRandomly(LevelData, EntityMap, TileMiniBoss, 5);
        Engine::LevelRenderer::DrawInitialMap(LevelData, EntityMap);
        DrawHUD(PlayerStatus());
    }

    // ----------------- Run Loop -----------------
    void Run() {
        using namespace chrono;
        auto lastEntityUpdate = steady_clock::now();
        const int frameTimeMs = 20;

        while (true) {
            auto frameStart = steady_clock::now();

            Direction dir = GetMoveDirection();
            if (dir != Direction::None) MovePlayer(dir);

            auto now = steady_clock::now();
            if (duration_cast<milliseconds>(now - lastEntityUpdate).count() >= 500) {
                EntityManager::UpdateEntities(LevelData, EntityMap, playerPos);
                lastEntityUpdate = now;
            }

            // Fixed HUD timing
            EntityManager::Encounters::UpdateHUD(player);

            if (EntityManager::Encounters::showingMessage)
                DrawHUD(EntityManager::Encounters::currentMessage);
            else
                DrawHUD(PlayerStatus());

            auto frameEnd = steady_clock::now();
            int sleepTime = frameTimeMs - duration_cast<milliseconds>(frameEnd - frameStart).count();
            if (sleepTime > 0) Sleep(sleepTime);
        }
    }

    // ----------------- Logo Display -----------------
    static void ShowLogo(int marginX = 2) {
        HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        vector<string> logo = {
            "            _____  _____ _____ _____            ",
            "     /\\    / ____|/ ____|_   _|_   _|           ",
            "    /  \\  | (___ | |      | |   | |             ",
            "   / /\\ \\  \\___ \\| |      | |   | |             ",
            "  / ____ \\ ____) | |____ _| |_ _| |_            ",
            " /_/___ \\_\\_____/ \\_____|_____|_____|___  _   _ ",
            " |  __ \\| |  | | \\ | |/ ____|  ____/ __ \\| \\ | |",
            " | |  | | |  | |  \\| | |  __| |__ | |  | |  \\| |",
            " | |  | | |  | | . ` | | |_ |  __|| |  | | . ` |",
            " | |__| | |__| | |\\  | |__| | |___| |__| | |\\  |",
            " |_____/ \\____/|_| \\_|\\_____|______\\____/|_| \\_|"
        };

        int logoHeight = (int)logo.size();
        int startY = (GameFieldHeight - logoHeight) / 2;

        for (int i = 0; i < logoHeight; i++) {
            int startX = (GameFieldWidth - (int)logo[i].size()) / 2 + marginX;
            COORD p = { (SHORT)startX, (SHORT)(startY + i) };
            SetConsoleCursorPosition(consoleHandle, p);

            if (i <= 4) SetConsoleTextAttribute(consoleHandle, 11); // Cyan
            else if (i == 5) SetConsoleTextAttribute(consoleHandle, 7); // White
            else SetConsoleTextAttribute(consoleHandle, 14); // Yellow

            cout << logo[i];
            SetConsoleTextAttribute(consoleHandle, 7); // Reset
        }

        Sleep(1000);
        COORD pos = { 0, 0 };
        SetConsoleCursorPosition(consoleHandle, pos);
        SetConsoleTextAttribute(consoleHandle, 7);
    }
};

string Game::HUDBar::lastMessage = ""; // define and initialize

// ----------------- Main -----------------
int main() {
    Engine::HideCursor();
    Engine::SetConsoleSize(GameFieldWidth, GameFieldHeight, 2);

    Game::ShowLogo(2);

    Game gameInstance;
    gameInstance.Run();
}

