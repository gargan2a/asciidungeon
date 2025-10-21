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
#include<unordered_map>

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
	bool operator!=(const GridPosition& other) const { return !(*this == other); }
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
		SMALL_RECT windowSize = { 0, 0, bufferSize.X - 1, bufferSize.Y - 1 };
		SetConsoleWindowInfo(consoleHandle, TRUE, &windowSize);
		SetConsoleScreenBufferSize(consoleHandle, bufferSize);
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

	//----- input handling class -----
	class InputManager {
	public:
		static bool up, down, left, right;

		static void Update() {
			while (_kbhit()) {
				char key = _getch();
				key = tolower(key);
				switch (key) {
				case 'w': up = true; break;
				case 's': down = true; break;
				case 'a': left = true; break;
				case 'd': right = true; break;
				case 27: exit(0); break;   // ESC
				}
			}
		}

		static void Reset() {
			up = down = left = right = false;
		}
	};

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
		// ----------------- ENCOUNTER HANDLER -----------------
		class Encounters {
		public:
			static bool showingMessage;
			static chrono::steady_clock::time_point lastMessageTime;
			static string currentMessage;

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

		

		// ----------------- AI CONTROLLER -----------------
		class AIController {
		public:
			static int stepCounter;
			static GridPosition currentTarget;
			static bool hasTarget;
			static vector<GridPosition> currentPath;
			static int reevalInterval;

			struct Node {
				GridPosition pos;
				int g, f;
				bool operator>(const Node& other) const { return f > other.f; }
			};

			static int ManhattanDistance(const GridPosition& a, const GridPosition& b) {
				return abs(a.x - b.x) + abs(a.y - b.y);
			}

			static bool IsWalkable(GridPosition pos, const vector<char>& levelData,
				const map<GridPosition, char>& entityMap, GridPosition playerPos) {
				if (pos.x < 0 || pos.x >= GameFieldWidth || pos.y < 0 || pos.y >= GameFieldHeight)
					return false;
				if (levelData[pos.y * GameFieldWidth + pos.x] != TileGround)
					return false;
				if (entityMap.count(pos) && pos != playerPos)
					return false;
				return true;
			}

			// ---------------- LINE OF SIGHT CHECK ----------------
			static bool HasLineOfSight(GridPosition from, GridPosition to,
				const vector<char>& levelData) {
				int x0 = from.x, y0 = from.y;
				int x1 = to.x, y1 = to.y;
				int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
				int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
				int err = dx + dy;

				while (true) {
					if (levelData[y0 * GameFieldWidth + x0] != TileGround &&
						!(x0 == from.x && y0 == from.y))
						return false; // wall blocks LOS

					if (x0 == x1 && y0 == y1)
						break;
					int e2 = 2 * err;
					if (e2 >= dy) { err += dy; x0 += sx; }
					if (e2 <= dx) { err += dx; y0 += sy; }
				}
				return true;
			}

			// ---------------- PATHFINDING ----------------
			static vector<GridPosition> AStarPath(GridPosition start, GridPosition goal,
				const vector<char>& levelData,
				const map<GridPosition, char>& entityMap) {
				priority_queue<Node, vector<Node>, greater<Node>> open;
				unordered_map<int, GridPosition> cameFrom;
				unordered_map<int, int> gScore;

				auto key = [](GridPosition p) { return p.y * GameFieldWidth + p.x; };
				open.push({ start, 0, ManhattanDistance(start, goal) });
				gScore[key(start)] = 0;

				vector<GridPosition> dirs = { {0,1}, {0,-1}, {1,0}, {-1,0} };

				while (!open.empty()) {
					Node current = open.top(); open.pop();
					if (current.pos == goal) {
						vector<GridPosition> path;
						for (GridPosition p = goal; p != start; p = cameFrom[key(p)])
							path.push_back(p);
						reverse(path.begin(), path.end());
						return path;
					}

					for (auto d : dirs) {
						GridPosition next = { current.pos.x + d.x, current.pos.y + d.y };
						if (!IsWalkable(next, levelData, entityMap, goal) && next != goal)
							continue;
						int tentative = gScore[key(current.pos)] + 1;
						if (!gScore.count(key(next)) || tentative < gScore[key(next)]) {
							cameFrom[key(next)] = current.pos;
							gScore[key(next)] = tentative;
							open.push({ next, tentative, tentative + ManhattanDistance(next, goal) });
						}
					}
				}
				return {};
			}

			// ---------------- ENEMY TARGETING ----------------
			static GridPosition GetNextAIMove(vector<char>& levelData, map<GridPosition, char>& entityMap,
				GridPosition playerPos) {
				stepCounter++;

				if (!hasTarget || stepCounter >= reevalInterval || !entityMap.count(currentTarget)) {
					hasTarget = false;
					stepCounter = 0;

					vector<pair<int, GridPosition>> visibleEnemies;
					vector<pair<int, GridPosition>> hiddenEnemies;

					for (auto& kv : entityMap) {
						char t = kv.second;
						if (t != TileEnemy && t != TileMiniBoss && t != TileBoss)
							continue;

						int dist = ManhattanDistance(kv.first, playerPos);
						if (HasLineOfSight(kv.first, playerPos, levelData))
							visibleEnemies.push_back({ dist, kv.first });
						else
							hiddenEnemies.push_back({ dist, kv.first });
					}

					GridPosition chosen{ -1, -1 };

					if (!visibleEnemies.empty()) {
						sort(visibleEnemies.begin(), visibleEnemies.end(),
							[](auto& a, auto& b) { return a.first < b.first; });
						chosen = visibleEnemies.front().second;
					}
					else if (!hiddenEnemies.empty()) {
						sort(hiddenEnemies.begin(), hiddenEnemies.end(),
							[](auto& a, auto& b) { return a.first < b.first; });
						chosen = hiddenEnemies.front().second;
					}

					if (chosen.x != -1) {
						currentTarget = chosen;
						hasTarget = true;
						currentPath = AStarPath(currentTarget, playerPos, levelData, entityMap);
					}
				}

				if (hasTarget && !currentPath.empty()) {
					GridPosition nextStep = currentPath.front();
					currentPath.erase(currentPath.begin());
					return nextStep;
				}

				return { -1, -1 };
			}
		};

		

		// ----------------- UTILITIES -----------------
		static vector<GridPosition> GetWalkableTiles(const vector<char>& levelData) {
			vector<GridPosition> walkable;
			for (int y = 0; y < GameFieldHeight; ++y)
				for (int x = 0; x < GameFieldWidth; ++x)
					if (levelData[y * GameFieldWidth + x] == TileGround)
						walkable.push_back({ x, y });
			return walkable;
		}

		static void PlaceEntitiesRandomly(vector<char>& levelData, map<GridPosition, char>& entityMap,
			char entityChar, int count) {
			auto walkable = GetWalkableTiles(levelData);
			shuffle(walkable.begin(), walkable.end(), rng);
			int placed = 0;
			for (auto& pos : walkable) {
				if (entityMap.count(pos)) continue;
				entityMap[pos] = entityChar;
				if (++placed >= count) break;
			}
		}

		// ----------------- ENTITY UPDATES -----------------
		static void UpdateEntities(vector<char>& levelData, map<GridPosition, char>& entityMap,
			GridPosition playerPos) {
			vector<GridPosition> positions;
			for (auto& kv : entityMap)
				if (kv.second != TilePlayer)
					positions.push_back(kv.first);

			GridPosition aiMove = AIController::GetNextAIMove(levelData, entityMap, playerPos);

			for (auto& pos : positions) {
				if (!entityMap.count(pos)) continue;
				char type = entityMap[pos];
				GridPosition newPos = pos;

				if (AIController::hasTarget && pos == AIController::currentTarget && aiMove.x != -1) {
					newPos = aiMove;
				}
				else {
					vector<Direction> dirs = { Direction::Up, Direction::Down, Direction::Left, Direction::Right };
					shuffle(dirs.begin(), dirs.end(), rng);
					for (auto dir : dirs) {
						newPos = pos;
						switch (dir) {
						case Direction::Up: newPos.y--; break;
						case Direction::Down: newPos.y++; break;
						case Direction::Left: newPos.x--; break;
						case Direction::Right: newPos.x++; break;
						default: break;
						}

						if (newPos.x < 0 || newPos.x >= GameFieldWidth || newPos.y < 0 || newPos.y >= GameFieldHeight)
							continue;
						if (levelData[newPos.y * GameFieldWidth + newPos.x] != TileGround)
							continue;

						// entities block each other completely
						if (entityMap.count(newPos) && entityMap[newPos] != TilePlayer)
							continue;

						// allow enemies to chase player
						if (newPos == playerPos)
							break;

						if (!entityMap.count(newPos))
							break;
					}
				}

				// Enemy catches player
				if (newPos == playerPos) {
					if (type == TileEnemy || type == TileMiniBoss || type == TileBoss) {
						entityMap.erase(pos);
						Encounters::HandleEncounter(type);
						Engine::LevelRenderer::DrawChangedRows(levelData, entityMap, pos.y, newPos.y);
						continue;
					}
					else {
						continue;
					}
				}

				// Prevent collisions between entities
				if (newPos != pos) {
					if (entityMap.count(newPos) && entityMap[newPos] != TilePlayer)
						continue;

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
		if (InputManager::up) return Direction::Up;
		if (InputManager::down) return Direction::Down;
		if (InputManager::left) return Direction::Left;
		if (InputManager::right) return Direction::Right;
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
			InputManager::Update();
			Direction dir = GetMoveDirection();
			if (dir != Direction::None)
				MovePlayer(dir);
			

			auto now = steady_clock::now();
			if (duration_cast<milliseconds>(now - lastEntityUpdate).count() >= 300) {
				EntityManager::UpdateEntities(LevelData, EntityMap, playerPos);
				lastEntityUpdate = now;
			}

			// Fixed HUD timing
			EntityManager::Encounters::UpdateHUD(player);

			if (EntityManager::Encounters::showingMessage)
				DrawHUD(EntityManager::Encounters::currentMessage);
			else
				DrawHUD(PlayerStatus());

			InputManager::Reset();

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

//definitions
bool Game::InputManager::up = false;
bool Game::InputManager::down = false;
bool Game::InputManager::left = false;
bool Game::InputManager::right = false;
int Game::EntityManager::AIController::stepCounter = 0;
GridPosition Game::EntityManager::AIController::currentTarget{};
bool Game::EntityManager::AIController::hasTarget = false;
vector<GridPosition> Game::EntityManager::AIController::currentPath;
int Game::EntityManager::AIController::reevalInterval = 2;
bool Game::EntityManager::Encounters::showingMessage = false;
chrono::steady_clock::time_point Game::EntityManager::Encounters::lastMessageTime;
string Game::EntityManager::Encounters::currentMessage = "";
string Game::HUDBar::lastMessage = "";

// ----------------- Main -----------------
int main() {
	Engine::HideCursor();
	Engine::SetConsoleSize(GameFieldWidth, GameFieldHeight, 2);

	Game::ShowLogo(2);

	Game gameInstance;
	gameInstance.Run();
}
