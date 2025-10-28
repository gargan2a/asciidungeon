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
bool isPaused = false;

struct GridPosition {
	int x, y;
	GridPosition(int xPos = 0, int yPos = 0) : x(xPos), y(yPos) {}
	bool operator<(const GridPosition& other) const { return tie(y, x) < tie(other.y, other.x); }
	bool operator==(const GridPosition& other) const { return x == other.x && y == other.y; }
	bool operator!=(const GridPosition& other) const { return !(*this == other); }
};

struct Player {
	int hp, maxHp, attack, defense, level, money;
};

class Engine {
public:

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

	static void ClearConsole() {
		HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		CONSOLE_SCREEN_BUFFER_INFO csbi;
		GetConsoleScreenBufferInfo(hConsole, &csbi);
		DWORD consoleSize = csbi.dwSize.X * csbi.dwSize.Y;
		COORD topLeft = { 0, 0 };
		DWORD charsWritten;
		FillConsoleOutputCharacter(hConsole, ' ', consoleSize, topLeft, &charsWritten);
		FillConsoleOutputAttribute(hConsole, csbi.wAttributes, consoleSize, topLeft, &charsWritten);
		SetConsoleCursorPosition(hConsole, topLeft);
	}

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

				for (int dy = 0; dy < clusterHeight; ++dy) {
					for (int dx = 0; dx < clusterWidth; ++dx) {
						int index = (clusterY + dy) * GameFieldWidth + (clusterX + dx);

						if (level[index] == TileGround) {
							clusterIndices.push_back(index);
							level[index] = TileWall;
						}
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
					for (int idx : clusterIndices) {
						level[idx] = TileGround;
					}
				}
				else {
					walkableTiles -= (int)clusterIndices.size();
				}
			}
			return level;
		}
	};

	class LevelRenderer {
	public:
		static void DrawInitialMap(const vector<char>& levelData, const map<GridPosition, char>& entityMap, int marginX = 2);
		static void DrawRow(const vector<char>& levelData, const map<GridPosition, char>& entityMap, int row, int marginX = 2);
		static void DrawChangedRows(const vector<char>& levelData, const map<GridPosition, char>& entityMap, int prevRow, int curRow, int marginX = 2);
	private:
		static void SetTileColor(HANDLE hConsole, char ch);
	};
};

void Engine::LevelRenderer::DrawInitialMap(const vector<char>& levelData, const map<GridPosition, char>& entityMap, int marginX) {
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	COORD pos = { 0, 0 };
	SetConsoleCursorPosition(hConsole, pos);

	for (int y = 0; y < GameFieldHeight; ++y) {
		for (int i = 0; i < marginX; ++i) {
			cout << ' ';
		}
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
	if (row < 0 || row >= GameFieldHeight) {
		return;
	}
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	COORD pos = { 0, (SHORT)row };
	SetConsoleCursorPosition(hConsole, pos);

	for (int i = 0; i < marginX; ++i) {
		cout << ' ';
	}
	for (int x = 0; x < GameFieldWidth; ++x) {
		GridPosition gp{ x, row };
		char ch = entityMap.count(gp) ? entityMap.at(gp) : levelData[row * GameFieldWidth + x];
		SetTileColor(hConsole, ch);
		cout << ch;
	}
	SetConsoleTextAttribute(hConsole, 7);
}

void Engine::LevelRenderer::DrawChangedRows(const vector<char>& levelData, const map<GridPosition, char>& entityMap, int prevRow, int curRow, int marginX) {
	if (prevRow != curRow) {
		DrawRow(levelData, entityMap, prevRow, marginX);
	}
	DrawRow(levelData, entityMap, curRow, marginX);
}

void Engine::LevelRenderer::SetTileColor(HANDLE hConsole, char ch) {
	switch (ch) {
	case TileWall: {
		SetConsoleTextAttribute(hConsole, 8);
		break;
	}
	case TilePlayer: {
		SetConsoleTextAttribute(hConsole, 9);
		break;
	}
	case TileEnemy: {
		SetConsoleTextAttribute(hConsole, 14);
		break;
	}
	case TileMerchant: {
		SetConsoleTextAttribute(hConsole, 11);
		break;
	}
	case TileMiniBoss: {
		SetConsoleTextAttribute(hConsole, 13);
		break;
	}
	case TileBoss: {
		SetConsoleTextAttribute(hConsole, 4);
		break;
	}
	default: {
		SetConsoleTextAttribute(hConsole, 7);
		break;
	}
	}
}

class Game {
public:
	vector<char> LevelData;
	map<GridPosition, char> EntityMap;
	Player player;
	GridPosition playerPos;
	enum class Direction { Up, Down, Left, Right, None };

	Game() {
		player = { 100, 100, 10, 5, 1, 100 };
		playerPos = { 1, 1 };
		GenerateNewLevel();
	}

	void DrawHUD(const string& message = "") {
		string msg = message.empty() ? PlayerStatus() : message;
		HUDBar::DrawHUDBar(2, msg);
	}

	string PlayerStatus() const {
		stringstream ss;
		ss << "HP:" << player.hp << "/" << player.maxHp
			<< " ATK:" << player.attack
			<< " DEF:" << player.defense
			<< " LVL:" << player.level
			<< " GOLD:" << player.money;
		return ss.str();
	}

	static string PlayerStatusStatic(const Player& player) {
		stringstream ss;
		ss << "HP:" << player.hp << "/" << player.maxHp
			<< " ATK:" << player.attack
			<< " DEF:" << player.defense
			<< " LVL:" << player.level
			<< " GOLD:" << player.money;
		return ss.str();
	}

	class InputManager {
	public:
		static bool up, down, left, right;

		static void Update() {
			while (_kbhit()) {
				char key = _getch();
				key = tolower(key);

				switch (key) {
				case 'w': {
					up = true;
					break;
				}
				case 's': {
					down = true;
					break;
				}
				case 'a': {
					left = true;
					break;
				}
				case 'd': {
					right = true;
					break;
				}
				case 27: {
					exit(0);
					break;
				}
				}
			}
		}
		static void Reset() {
			up = down = left = right = false;
		}
	};

	class HUDBar {
	public:
		static string lastMessage;

		static void DrawHUDBar(int marginX, const string& message) {
			if (message == lastMessage) {
				return;
			}
			lastMessage = message;
			HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
			SetConsoleTextAttribute(consoleHandle, 7);
			COORD pos = { 0, (SHORT)(GameFieldHeight + 1) };
			SetConsoleCursorPosition(consoleHandle, pos);
			string margin(marginX, ' ');
			cout << margin;

			for (int i = 0; i < GameFieldWidth; ++i) {
				cout << '=';
			}
			cout << '\n';
			cout << margin;
			SetConsoleTextAttribute(consoleHandle, 14);
			cout << message;

			if ((int)message.size() < GameFieldWidth) {
				cout << string(GameFieldWidth - message.size(), ' ');
			}
			cout << '\n';
			SetConsoleTextAttribute(consoleHandle, 7);
			cout << margin;

			for (int i = 0; i < GameFieldWidth; ++i) {
				cout << '=';
			}
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
			lastMessage = "";
		}
	};

	class EntityManager {
	public:

		class Encounters {
		public:
			static bool showingMessage;
			static chrono::steady_clock::time_point lastMessageTime;
			static string currentMessage;

			static void HandleEncounter(Game& game, char entityType) {
				switch (entityType) {
				case TileEnemy: {
					currentMessage = "Encountered an enemy!";
					EnemyEncounter(game);
					break;
				}
				case TileMerchant: {
					currentMessage = "Encountered a merchant!";
					MerchantEncounter(game);
					break;
				}
				case TileBoss: {
					currentMessage = "Encountered a boss!";
					BossEncounter(game);
					break;
				}
				case TileMiniBoss: {
					currentMessage = "Encountered a miniboss!";
					MinibossEncounter(game);
					break;
				}
				default: {
					currentMessage = "";
					return;
				}
				}
				showingMessage = true;
				lastMessageTime = chrono::steady_clock::now();
				Game::HUDBar::DrawHUDBar(2, currentMessage);
			}

			static void EnemyEncounter(Game& game) {
				Combat::StartCombat(game, TileEnemy);
			}

			static void MerchantEncounter(Game& game) {
				Shop::OpenShop(game);
			}

			static void BossEncounter(Game& game) {
				Combat::StartCombat(game, TileBoss);
			}

			static void MinibossEncounter(Game& game) {
				Combat::StartCombat(game, TileMiniBoss);
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

		class Shop {
		public:
			static void OpenShop(Game& game) {
				game.Pause();
				Engine::ClearConsole();

				bool inShop = true;

				auto getChoice = []() -> char {
					char c = std::tolower(_getch());
					std::cout << c << "\n";
					return c;
					};

				auto confirmAction = [](const std::string& msg) -> bool {
					std::cout << msg << " (y/n): ";
					char c;
					while (true) {
						c = std::tolower(_getch());
						if (c == 'y') { std::cout << "y\n"; return true; }
						if (c == 'n') { std::cout << "n\n"; return false; }
					}
					};

				while (inShop) {
					while (true) {
						Engine::ClearConsole();
						std::cout << "=== MERCHANT'S SHOP ===\n";
						std::cout << "Welcome, traveler!\n\n";
						std::cout << "Your current stats:\n";
						std::cout << "HP: " << game.player.hp << "/" << game.player.maxHp << "\n";
						std::cout << "ATK: " << game.player.attack << "\n";
						std::cout << "DEF: " << game.player.defense << "\n";
						std::cout << "LVL: " << game.player.level << "\n";
						std::cout << "Gold: " << game.player.money << "\n\n";

						std::cout << "Choose an option:\n";
						std::cout << "[H] Heal to full HP (10 gold)\n";
						std::cout << "[1] Upgrade Max HP (+10) (20 gold)\n";
						std::cout << "[2] Upgrade Attack (+2) (15 gold)\n";
						std::cout << "[3] Upgrade Defense (+2) (15 gold)\n";
						std::cout << "[E] Exit Shop\n\n";
						std::cout << "Enter your choice: ";

						char choice = getChoice();
						bool validInput = true;

						switch (choice) {
						case 'h':
							if (game.player.money >= 10 && confirmAction("Heal to full HP for 10 gold?")) {
								game.player.hp = game.player.maxHp;
								game.player.money -= 10;
								std::cout << "You have been fully healed!\n";
							}
							else validInput = false;
							break;

						case '1':
							if (game.player.money >= 20 && confirmAction("Upgrade Max HP for 20 gold?")) {
								game.player.maxHp += 10;
								game.player.money -= 20;
								std::cout << "Your maximum HP increased to " << game.player.maxHp << "!\n";
							}
							else validInput = false;
							break;

						case '2':
							if (game.player.money >= 15 && confirmAction("Upgrade Attack for 15 gold?")) {
								game.player.attack += 2;
								game.player.money -= 15;
								std::cout << "Your attack increased to " << game.player.attack << "!\n";
							}
							else validInput = false;
							break;

						case '3':
							if (game.player.money >= 15 && confirmAction("Upgrade Defense for 15 gold?")) {
								game.player.defense += 2;
								game.player.money -= 15;
								std::cout << "Your defense increased to " << game.player.defense << "!\n";
							}
							else validInput = false;
							break;

						case 'e':
							std::cout << "Safe travels, adventurer!\n";
							inShop = false;
							break;

						default:
							std::cout << "Invalid choice. Try again...\n";
							validInput = false;
							break;
						}

						if (validInput) {
							std::cout << "Press any key to continue...\n";
							_getch();  							break;
						}
						else {
							std::cout << "Press any key to continue...\n";
							_getch();
						}
					}
				}

				Engine::ClearConsole();
				game.Resume();
			}
		};

		class Combat {
		public:
			static void StartCombat(Game& game, const char enemyType) {
				Engine::ClearConsole();

				int enemyHp, enemyAttack, enemyDefense, reward;
				std::string enemyName;

				switch (enemyType) {
				case TileEnemy:
					enemyName = "Enemy";
					enemyHp = 40;
					enemyAttack = 7;
					enemyDefense = 2;
					reward = 10;
					break;
				case TileMiniBoss:
					enemyName = "MiniBoss";
					enemyHp = 100;
					enemyAttack = 14;
					enemyDefense = 4;
					reward = 50;
					break;
				case TileBoss:
					enemyName = "Boss";
					enemyHp = 300;
					enemyAttack = 20;
					enemyDefense = 8;
					reward = 200;
					break;
				default:
					return;
				}

				bool playerAlive = true;
				game.Pause();

				auto getChoice = []() -> char {
					char c = std::tolower(_getch());
					std::cout << c << "\n";
					return c;
					};

				auto confirmAction = [](const std::string& msg) -> bool {
					std::cout << msg << " (y/n): ";
					char c;
					while (true) {
						c = std::tolower(_getch());
						if (c == 'y') { std::cout << "y\n"; return true; }
						if (c == 'n') { std::cout << "n\n"; return false; }
					}
					};

				while (enemyHp > 0 && game.player.hp > 0) {
					int damageToPlayer = max(0, enemyAttack - game.player.defense);
					game.player.hp -= damageToPlayer;

					Engine::ClearConsole();
					if (game.player.hp <= 0) {
						std::cout << enemyName << " attacks! You take " << damageToPlayer
							<< " damage. (HP:0/" << game.player.maxHp << ")\n";
						std::cout << "\nYou were defeated...\n";
						playerAlive = false;
						break;
					}
					else {
						std::cout << enemyName << " attacks! You take " << damageToPlayer
							<< " damage. (HP:" << game.player.hp << "/" << game.player.maxHp << ")\n";
					}
					std::cout << enemyName << " HP: " << enemyHp << "\n";
					std::cout << "Your HP: " << game.player.hp << "\n";

					std::cout << "[A] Attack  [H] Heal  [Q] Run\n";
					std::cout << "Your choice: ";
					char action = getChoice();
					bool validInput = true;

					switch (action) {
					case 'a': {
						if (confirmAction("Attack " + enemyName + "?")) {
							int damageToEnemy = max(0, game.player.attack - enemyDefense);
							enemyHp -= damageToEnemy;
							Engine::ClearConsole();
							std::cout << "You hit " << enemyName << " for " << damageToEnemy
								<< " damage! (Enemy HP: " << max(0, enemyHp) << ")\n";
							std::cout << "Your HP: " << game.player.hp << "\n";
						}
						else validInput = false;
						break;
					}
					case 'h': {
						int heal = min(15, game.player.maxHp - game.player.hp);
						if (confirmAction("Heal " + std::to_string(heal) + " HP?")) {
							game.player.hp += heal;
							Engine::ClearConsole();
							std::cout << "You heal " << heal << " HP. (HP:" << game.player.hp
								<< "/" << game.player.maxHp << ")\n";
							std::cout << enemyName << " HP: " << enemyHp << "\n";
						}
						else validInput = false;
						break;
					}
					case 'q':
						if (confirmAction("Flee the battle?")) {
							std::cout << "You fled the battle!\n";
							game.Resume();
							return;
						}
						else validInput = false;
						break;
					default:
						std::cout << "Invalid input.\n";
						validInput = false;
						break;
					}

					if (!validInput) {
						std::cout << "Invalid input.\n";
						_getch();
					}
				}

				if (playerAlive && enemyHp <= 0) {
					Engine::ClearConsole();
					std::cout << "\nYou defeated " << enemyName << "!\n";
					game.player.money += reward;
					std::cout << "You earned " << reward << " gold! (Total gold: "
						<< game.player.money << ")\n";
				}

				if (enemyType == TileBoss && enemyHp <= 0) {
					GameWinManager::ShowGameWin();
				}

				std::cout << "\nPress any key to continue...";
				_getch();
				game.Resume();
			}
		};

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

				if (pos.x < 0 || pos.x >= GameFieldWidth || pos.y < 0 || pos.y >= GameFieldHeight) {
					return false;
				}
				if (levelData[pos.y * GameFieldWidth + pos.x] != TileGround) {
					return false;
				}
				if (entityMap.count(pos) && pos != playerPos) {
					return false;
				}
				return true;
			}

			static bool HasLineOfSight(GridPosition from, GridPosition to,
				const vector<char>& levelData) {
				int x0 = from.x, y0 = from.y;
				int x1 = to.x, y1 = to.y;
				int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
				int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
				int err = dx + dy;

				while (true) {
					if (levelData[y0 * GameFieldWidth + x0] != TileGround && !(x0 == from.x && y0 == from.y)) {
						return false;
					}
					if (x0 == x1 && y0 == y1) {
						break;
					}
					int e2 = 2 * err;

					if (e2 >= dy) {
						err += dy;
						x0 += sx;
					}
					if (e2 <= dx) {
						err += dx;
						y0 += sy;
					}
				}
				return true;
			}

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

						for (GridPosition p = goal; p != start; p = cameFrom[key(p)]) {
							path.push_back(p);
						}
						reverse(path.begin(), path.end());
						return path;
					}
					for (auto d : dirs) {
						GridPosition next = { current.pos.x + d.x, current.pos.y + d.y };

						if (!IsWalkable(next, levelData, entityMap, goal) && next != goal) {
							continue;
						}
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

						if (t != TileEnemy && t != TileMiniBoss && t != TileBoss) {
							continue;
						}
						int dist = ManhattanDistance(kv.first, playerPos);

						if (HasLineOfSight(kv.first, playerPos, levelData)) {
							visibleEnemies.push_back({ dist, kv.first });
						}
						else {
							hiddenEnemies.push_back({ dist, kv.first });
						}
					}
					GridPosition chosen{ -1, -1 };

					if (!visibleEnemies.empty()) {
						sort(visibleEnemies.begin(), visibleEnemies.end(), [](auto& a, auto& b) { return a.first < b.first; });
						chosen = visibleEnemies.front().second;
					}
					else if (!hiddenEnemies.empty()) {
						sort(hiddenEnemies.begin(), hiddenEnemies.end(), [](auto& a, auto& b) { return a.first < b.first; });
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

		static vector<GridPosition> GetWalkableTiles(const vector<char>& levelData) {
			vector<GridPosition> walkable;

			for (int y = 0; y < GameFieldHeight; ++y) {
				for (int x = 0; x < GameFieldWidth; ++x) {
					if (levelData[y * GameFieldWidth + x] == TileGround) {
						walkable.push_back({ x, y });
					}
				}
			}
			return walkable;
		}

		static void PlaceEntitiesRandomly(vector<char>& levelData, map<GridPosition, char>& entityMap,
			char entityChar, int count) {
			auto walkable = GetWalkableTiles(levelData);
			shuffle(walkable.begin(), walkable.end(), rng);
			int placed = 0;

			for (auto& pos : walkable) {
				if (entityMap.count(pos)) {
					continue;
				}
				entityMap[pos] = entityChar;

				if (++placed >= count) {
					break;
				}
			}
		}

		static void UpdateEntities(Game& game, vector<char>& levelData, map<GridPosition, char>& entityMap,
			GridPosition playerPos) {
			vector<GridPosition> positions;

			for (auto& kv : entityMap) {
				if (kv.second != TilePlayer) {
					positions.push_back(kv.first);
				}
			}
			GridPosition aiMove = AIController::GetNextAIMove(levelData, entityMap, playerPos);

			for (auto& pos : positions) {
				if (!entityMap.count(pos)) {
					continue;
				}
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
						case Direction::Up: {
							newPos.y--; break;
						}
						case Direction::Down: {
							newPos.y++; break;
						}
						case Direction::Left: {
							newPos.x--; break;
						}
						case Direction::Right: {
							newPos.x++; break;
						}
						default: {
							break;
						}
						}

						if (newPos.x < 0 || newPos.x >= GameFieldWidth || newPos.y < 0 || newPos.y >= GameFieldHeight) {
							continue;
						}
						if (levelData[newPos.y * GameFieldWidth + newPos.x] != TileGround) {
							continue;
						}
						if (entityMap.count(newPos) && entityMap[newPos] != TilePlayer) {
							continue;
						}
						if (newPos == playerPos) {
							break;
						}
						if (!entityMap.count(newPos)) {
							break;
						}
					}
				}

				if (newPos == playerPos) {
					if (type == TileEnemy || type == TileMiniBoss || type == TileBoss) {
						entityMap.erase(pos);
						EntityManager::Encounters::HandleEncounter(game, type);
						Engine::LevelRenderer::DrawChangedRows(levelData, entityMap, pos.y, newPos.y);
						continue;
					}
					else {
						continue;
					}
				}

				if (newPos != pos) {
					if (entityMap.count(newPos) && entityMap[newPos] != TilePlayer) {
						continue;
					}
					entityMap.erase(pos);
					entityMap[newPos] = type;
					Engine::LevelRenderer::DrawChangedRows(levelData, entityMap, pos.y, newPos.y);
				}
			}
		}
	};

	static bool CanMove(const GridPosition& pos, const vector<char>& levelData) {
		if (pos.x < 0 || pos.x >= GameFieldWidth || pos.y < 0 || pos.y >= GameFieldHeight) {
			return false;
		}
		return levelData[pos.y * GameFieldWidth + pos.x] == TileGround;
	}

	static Direction GetMoveDirection() {
		if (InputManager::up) {
			return Direction::Up;
		}
		else if (InputManager::down) {
			return Direction::Down;
		}
		else if (InputManager::left) {
			return Direction::Left;
		}
		else if (InputManager::right) {
			return Direction::Right;
		}
		else {
			return Direction::None;
		}
	}

	void MovePlayer(Direction dir) {
		GridPosition newPos = playerPos;

		switch (dir) {
		case Direction::Up: {
			newPos.y--;
			break;
		}
		case Direction::Down: {
			newPos.y++;
			break;
		}
		case Direction::Left: {
			newPos.x--;
			break;
		}
		case Direction::Right: {
			newPos.x++;
			break;
		}
		default: {
			return;
		}
		}

		if (!CanMove(newPos, LevelData)) {
			return;
		}
		auto it = EntityMap.find(newPos);

		if (it != EntityMap.end()) {

			EntityManager::Encounters::HandleEncounter(*this, it->second);
			EntityMap.erase(newPos);
		}
		EntityMap.erase(playerPos);
		EntityMap[newPos] = TilePlayer;
		Engine::LevelRenderer::DrawChangedRows(LevelData, EntityMap, playerPos.y, newPos.y);
		playerPos = newPos;
	}

	void GenerateNewLevel() {
		LevelData = Engine::LevelGenerator::GenerateLevel();
		EntityMap.clear();
		playerPos = { 1, 1 };
		EntityMap[playerPos] = TilePlayer;

		WaveManager::WaveInfo wave = waveManager.GetNextWave();

		if (wave.enemies > 0)
			EntityManager::PlaceEntitiesRandomly(LevelData, EntityMap, TileEnemy, wave.enemies);

		if (wave.minibosses > 0)
			EntityManager::PlaceEntitiesRandomly(LevelData, EntityMap, TileMiniBoss, wave.minibosses);

		if (wave.bosses > 0)
			EntityManager::PlaceEntitiesRandomly(LevelData, EntityMap, TileBoss, wave.bosses);

		if (wave.merchants > 0)
			EntityManager::PlaceEntitiesRandomly(LevelData, EntityMap, TileMerchant, wave.merchants);

		Engine::LevelRenderer::DrawInitialMap(LevelData, EntityMap);
		DrawHUD(PlayerStatus());
	}

	void Resume() {
		Engine::LevelRenderer::DrawInitialMap(LevelData, EntityMap);
		DrawHUD(PlayerStatus());
		Sleep(200);
		isPaused = false;
	}

	void Pause() {
		isPaused = true;
		Engine::ClearConsole();
	}

	bool EntityMapFinishedWave() const {
		for (const auto& kv : EntityMap) {
			char type = kv.second;
			if (type == TileEnemy || type == TileMiniBoss || type == TileBoss || type == TileMerchant) {
				return false;
			}
		}
		return true;
	}

	void Run(Game& game) {
		using namespace std::chrono;
		auto lastEntityUpdate = steady_clock::now();
		const int frameTimeMs = 20;

		while (true) {
			if (!isPaused) {
				auto frameStart = steady_clock::now();
				InputManager::Update();
				Direction dir = GetMoveDirection();

				if (player.hp <= 0) {
					Pause();
					GameOverManager::HandleGameOver();
				}
				if (dir != Direction::None) {
					MovePlayer(dir);
				}

				auto now = steady_clock::now();
				if (duration_cast<milliseconds>(now - lastEntityUpdate).count() >= 300) {
					EntityManager::UpdateEntities(game, LevelData, EntityMap, playerPos);
					lastEntityUpdate = now;
				}

				EntityManager::Encounters::UpdateHUD(player);

				if (EntityManager::Encounters::showingMessage) {
					DrawHUD(EntityManager::Encounters::currentMessage);
				}
				else {
					DrawHUD(PlayerStatus());
				}

				InputManager::Reset();
				auto frameEnd = steady_clock::now();
				int sleepTime = frameTimeMs - duration_cast<milliseconds>(frameEnd - frameStart).count();
				if (sleepTime > 0) Sleep(sleepTime);

				if (EntityMapFinishedWave()) {
					GenerateNewLevel();
				}
			}
			else {
				Sleep(50);
			}
		}
	}

	class GameOverManager {
	public:
		static void ShowGameOver() {
			HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
			std::vector<std::string> logo = {
				"  _____                        ",
				" / ____|                       ",
				"| |  __  __ _ _ __ ___   ___   ",
				"| | |_ |/ _` | '_ ` _ \\ / _ \\  ",
				"| |__| | (_| | | | | | |  __/  ",
				" \\_____|\\__,_|_| |_| |_|\\___|  ",
				"   / __ \\                       ",
				"  | |  | |_   _____ _ __         ",
				"  | |  | \\ \\ / / _ \\ '__|       ",
				"  | |__| |\\ V /  __/ |           ",
				"   \\____/  \\_/ \\___|_|           "

			};

			int logoHeight = (int)logo.size();
			int startY = (GameFieldHeight - logoHeight) / 2;

			for (int i = 0; i < logoHeight; i++) {
				int startX = (GameFieldWidth - (int)logo[i].size()) / 2;
				COORD p = { (SHORT)startX, (SHORT)(startY + i) };
				SetConsoleCursorPosition(consoleHandle, p);
				SetConsoleTextAttribute(consoleHandle, 12);
				std::cout << logo[i];
			}

			SetConsoleTextAttribute(consoleHandle, 7);
			Sleep(1000);
			COORD pos = { 0, 0 };
			SetConsoleCursorPosition(consoleHandle, pos);
		}

		static void HandleGameOver() {
			ShowGameOver();
			exit(0);
		}
	};

	class WaveManager {
	public:
		int currentWave = 0;

		struct WaveInfo {
			int enemies;
			int minibosses;
			int bosses;
			int merchants;
		};

		WaveManager() {}

		WaveInfo GetNextWave() {
			currentWave++;
			WaveInfo wave{ 0, 0, 0, 1 };
			wave.enemies = 4 + currentWave / 2;

			if (currentWave % 10 == 0) {
				wave.minibosses = currentWave / 10;
				wave.enemies = max(0, wave.enemies - wave.minibosses);
			}

			if (currentWave == 100) {
				wave.bosses = 1;
				wave.enemies = 0;
				wave.minibosses = 0;
			}

			return wave;
		}
	};
	WaveManager waveManager;

	class GameWinManager {
	public:
		static void ShowGameWin() {
			HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
			std::vector<std::string> logo = {
				"__      ___      _                   _",
				"\\ \\    / (_)    | |                 | |",
				" \\ \\  / / _  ___| |_ ___  _ __ _   _| |",
				"  \\ \\/ / | |/ __| __/ _ \\| '__| | | | |",
				"   \\  /  | | (__| || (_) | |  | |_| |_|",
				"    \\/   |_|\\___|\\__\\___/|_|   \\__, (_)",
				"                                 __/ |",
				"                                |___/"

			};

			int logoHeight = (int)logo.size();
			int startY = (GameFieldHeight - logoHeight) / 2;

			for (int i = 0; i < logoHeight; i++) {
				int startX = (GameFieldWidth - (int)logo[i].size()) / 2;
				COORD p = { (SHORT)startX, (SHORT)(startY + i) };
				SetConsoleCursorPosition(consoleHandle, p);
				SetConsoleTextAttribute(consoleHandle, 2);
				std::cout << logo[i];
			}

			SetConsoleTextAttribute(consoleHandle, 7);
			std::cout << "\n\n";
			std::cout << "Congratulations! You defeated the boss!\n";
			std::cout << "Press any key to exit...";
			_getch();
			exit(0);
		}
	};

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

			if (i <= 4) {
				SetConsoleTextAttribute(consoleHandle, 11);
			}
			else if (i == 5) {
				SetConsoleTextAttribute(consoleHandle, 7);
			}
			else {
				SetConsoleTextAttribute(consoleHandle, 14);
			}

			cout << logo[i];
			SetConsoleTextAttribute(consoleHandle, 7);
		}
		Sleep(1000);
		COORD pos = { 0, 0 };
		SetConsoleCursorPosition(consoleHandle, pos);
		SetConsoleTextAttribute(consoleHandle, 7);

	}

};

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

int main() {
	Engine::HideCursor();
	Engine::SetConsoleSize(GameFieldWidth, GameFieldHeight, 2);
	Game::ShowLogo(2);
	Game game;
	game.Run(game);
}
