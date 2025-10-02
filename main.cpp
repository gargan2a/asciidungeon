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

using namespace std;

// ----------------- Global Game Settings -----------------
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

// ----------------- Structs -----------------
struct GridPosition {
	int x, y;
	GridPosition(int xPos = 0, int yPos = 0) : x(xPos), y(yPos) {}
	bool operator<(const GridPosition& other) const { return tie(y, x) < tie(other.y, other.x); }
	bool operator==(const GridPosition& other) const { return x == other.x && y == other.y; }
};

struct Player {
	int hp;
	int maxHp;
	int attack;
	int defense;
	int level;
};

Player player{ 100, 100, 10, 5, 1 };

// ----------------- Forward Declarations -----------------
void GenerateNewLevel();
class HUDBar;

std::string PlayerStatus(const Player& player) {
	std::stringstream ss;
	ss << "HP:" << player.hp << "/" << player.maxHp
		<< " ATK:" << player.attack
		<< " DEF:" << player.defense
		<< " LVL:" << player.level;
	return ss.str();
}

// ----------------- Global Game State -----------------
vector<char> LevelData(static_cast<size_t>(GameFieldWidth)* GameFieldHeight, TileGround);
int count = LevelData.size();
map<GridPosition, char> EntityMap;

// ----------------- Enum for movement -----------------
enum class Direction { Up, Down, Left, Right, None };

// ----------------- Player Input -----------------
Direction GetPlayerMoveDirection() {
	if (_kbhit()) {
		char key = _getch();
		switch (tolower(key)) {
		case 'w': return Direction::Up;
		case 's': return Direction::Down;
		case 'a': return Direction::Left;
		case 'd': return Direction::Right;
		case 'q': exit(0);
		case 'e': return Direction::None; // special case for level regeneration testing
		default: return Direction::None;
		}
	}
	return Direction::None;
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

		// Add border walls
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

			// BFS to check connectivity
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

// ----------------- Console Utilities -----------------
class ConsoleUtils {
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
};

// ----------------- HUD Bar -----------------
class HUDBar {
public:
	static void DrawHUDBar(int marginX = 2, const string& message = "") {
		if ((int)message.length() > GameFieldWidth) return;

		HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);

		COORD pos = { 0, (SHORT)(GameFieldHeight + 1) };
		SetConsoleCursorPosition(consoleHandle, pos);
		string margin(marginX, ' ');
		cout << margin;
		for (int i = 0; i < GameFieldWidth; ++i) cout << '=';
		cout << '\n';
		cout << margin << message << '\n';
		cout << margin;
		for (int i = 0; i < GameFieldWidth; ++i) cout << '=';
		cout << '\n';
	}

	static void ClearHUDBar(int marginX = 2) {
		HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
		COORD pos = { 0, (SHORT)(GameFieldHeight + 1) };
		SetConsoleCursorPosition(consoleHandle, pos);

		string margin(marginX, ' ');
		for (int i = 0; i < 3; ++i) {
			cout << margin;
			for (int j = 0; j < GameFieldWidth; ++j) cout << ' ';
			cout << '\n';
		}
	}
};

// ----------------- Level Renderer -----------------
class LevelRenderer {
public:
	static void DrawInitialMap(const vector<char>& levelData, const map<GridPosition, char>& entityMap, int marginX = 2) {
		HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		COORD pos = { 0,0 };
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

	static void DrawRow(const vector<char>& levelData, const map<GridPosition, char>& entityMap, int row, int marginX = 2) {
		if (row < 0 || row >= GameFieldHeight) return;
		HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		COORD pos = { (SHORT)0, (SHORT)row };
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

	static void DrawChangedRows(const vector<char>& levelData, const map<GridPosition, char>& entityMap, int prevRow, int curRow, int marginX = 2) {
		if (prevRow != curRow) DrawRow(levelData, entityMap, prevRow, marginX);
		DrawRow(levelData, entityMap, curRow, marginX);
	}

private:
	static void SetTileColor(HANDLE hConsole, char ch) {
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
};

// ----------------- Game Utilities -----------------
vector<GridPosition> GetWalkableTiles(const vector<char>& levelData) {
	vector<GridPosition> walkableTiles;
	for (int y = 0; y < GameFieldHeight; ++y) {
		for (int x = 0; x < GameFieldWidth; ++x) {
			if (levelData[y * GameFieldWidth + x] == TileGround) {
				walkableTiles.push_back({ x, y });
			}
		}
	}
	return walkableTiles;
}

void PlaceEntitiesRandomly(vector<char>& levelData, map<GridPosition, char>& entityMap, char entityChar, int count) {
	vector<GridPosition> walkableTiles = GetWalkableTiles(levelData);
	shuffle(walkableTiles.begin(), walkableTiles.end(), rng);

	int placedCount = 0;
	for (auto& pos : walkableTiles) {
		if (entityMap.count(pos)) continue;
		entityMap[pos] = entityChar;
		++placedCount;
		if (placedCount >= count) break;
	}
}

void UpdateEntities(vector<char>& levelData, map<GridPosition, char>& entityMap, const GridPosition& playerPos) {
	vector<GridPosition> positions; // seznam entit, které se pohybují

	// Uložíme všechny entity kromě hráče
	for (auto& kv : entityMap) {
		if (kv.second != TilePlayer) {
			positions.push_back(kv.first);
		}
	}

	for (auto& pos : positions) {
		// Pokud entita už byla odstraněna (např. zabita nebo se smazala), přeskočíme ji
		if (!entityMap.count(pos)) continue;

		char entityType = entityMap[pos];
		GridPosition newPos = pos;

		bool moved = false;
		// Zkusíme až 4 různé směry
		vector<Direction> directions = { Direction::Up, Direction::Down, Direction::Left, Direction::Right };
		shuffle(directions.begin(), directions.end(), rng); // náhodné pořadí

		for (Direction dir : directions) {
			newPos = pos;
			switch (dir) {
			case Direction::Up: newPos.y--; break;
			case Direction::Down: newPos.y++; break;
			case Direction::Left: newPos.x--; break;
			case Direction::Right: newPos.x++; break;
			default: break;
			}

			// Kontrola hranic mapy
			if (newPos.x < 0 || newPos.x >= GameFieldWidth || newPos.y < 0 || newPos.y >= GameFieldHeight)
				continue;

			// Kontrola, že cílová pozice je zem
			if (levelData[newPos.y * GameFieldWidth + newPos.x] != TileGround)
				continue;

			// Kontrola kolize s hráčem
			if (newPos == playerPos)
				continue;

			// Kontrola kolize s jinou entitou
			if (entityMap.count(newPos))
				continue;

			// Pokud jsme sem došli, můžeme se pohnout
			moved = true;
			break;
		}

		if (moved) {
			entityMap.erase(pos);
			entityMap[newPos] = entityType;

			// Překreslíme změněné řádky
			LevelRenderer::DrawChangedRows(levelData, entityMap, pos.y, newPos.y);
		}
		// Pokud žádný směr není volný -> zůstane stát
	}
}

// ----------------- Encounter Handling -----------------
class Encounters {
public:
	static inline bool showingMessage = false;
	static inline chrono::steady_clock::time_point lastMessageTime;
	static inline string currentMessage = "";

	static void HandleEncounter(char entityType) {
		switch (entityType) {
		case TileEnemy:
			currentMessage = "Encountered an enemy!";
			break;
		case TileMerchant:
			currentMessage = "Encountered a merchant!";
			break;
		case TileBoss:
			currentMessage = "Encountered a boss!";
			break;
		case TileMiniBoss:
			currentMessage = "Encountered a miniboss!";
			break;
		default:
			currentMessage = "";
			return;
		}

		// Spustíme zobrazení zprávy
		showingMessage = true;
		lastMessageTime = chrono::steady_clock::now();

		HUDBar::ClearHUDBar(2);
		HUDBar::DrawHUDBar(2, currentMessage);
	}

	static void UpdateHUD(const Player& player) {
		using namespace chrono;

		if (showingMessage) {
			auto now = steady_clock::now();
			if (duration_cast<milliseconds>(now - lastMessageTime).count() >= 2000) {
				// Po 2 sekundách vypíšeme zpět status hráče
				showingMessage = false;
				HUDBar::ClearHUDBar(2);
				HUDBar::DrawHUDBar(2, PlayerStatus(player));
			}
		}
		else {
			HUDBar::DrawHUDBar(2, PlayerStatus(player));
		}
	}
};

// ----------------- Player Class -----------------
class PlayerClass {
public:
	GridPosition position;
	Player player;

	PlayerClass() {
		player = { 100, 100, 10, 5, 1 };
		position = { 1, 1 };
	}

	string GetStatusString() const {
		return PlayerStatus(player);
	}


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
			case 'e': HUDBar::ClearHUDBar(2); HUDBar::DrawHUDBar(2, "restarting ..."); Sleep(500); GenerateNewLevel(); return Direction::None;
			default: return Direction::None;
			}
		}
		return Direction::None;
	}

	void MoveAndHandle(Direction dir, vector<char>& levelData, map<GridPosition, char>& entityMap,
		int& previousRow, string& currentMessage, bool& showingMessage, chrono::steady_clock::time_point& lastTime) {

		GridPosition newPos = position;
		switch (dir) {
		case Direction::Up: newPos.y--; break;
		case Direction::Down: newPos.y++; break;
		case Direction::Left: newPos.x--; break;
		case Direction::Right: newPos.x++; break;
		default: return;
		}
		if (!CanMove(newPos, levelData)) return;

		auto it = entityMap.find(newPos);
		if (it != entityMap.end()) {
			Encounters::HandleEncounter(it->second);
			entityMap.erase(newPos);
		}


		entityMap.erase(position);
		entityMap[newPos] = TilePlayer;

		int currentRow = newPos.y;
		LevelRenderer::DrawChangedRows(LevelData, entityMap, previousRow, currentRow);
		previousRow = currentRow;

		position = newPos;
	}

	void Run(vector<char>& levelData, map<GridPosition, char>& entityMap) {
		using namespace chrono;
		const int frameTimeMs = 20;
		int previousRow = position.y;
		auto lastEntityUpdate = steady_clock::now();

		while (true) {
			auto frameStart = steady_clock::now();

			// --- Pohyb hráče ---
			Direction dir = GetMoveDirection();
			if (dir != Direction::None) {
				GridPosition newPos = position;
				switch (dir) {
				case Direction::Up: newPos.y--; break;
				case Direction::Down: newPos.y++; break;
				case Direction::Left: newPos.x--; break;
				case Direction::Right: newPos.x++; break;
				default: break;
				}

				if (CanMove(newPos, levelData)) {
					auto it = entityMap.find(newPos);
					if (it != entityMap.end()) {
						Encounters::HandleEncounter(it->second); // start 2s zprávy
						entityMap.erase(newPos);
					}

					entityMap.erase(position);
					entityMap[newPos] = TilePlayer;

					int currentRow = newPos.y;
					LevelRenderer::DrawChangedRows(levelData, entityMap, previousRow, currentRow);
					previousRow = currentRow;
					position = newPos;
				}
			}

			// --- Pohyb nepřátel každých 500 ms ---
			auto now = steady_clock::now();
			if (duration_cast<milliseconds>(now - lastEntityUpdate).count() >= 500) {
				UpdateEntities(levelData, entityMap, position);
				lastEntityUpdate = now;
			}

			// --- Aktualizace HUD (včetně zpráv na 2s) ---
			Encounters::UpdateHUD(player);

			// --- Frame timing ---
			auto frameEnd = steady_clock::now();
			int sleepTime = frameTimeMs - duration_cast<milliseconds>(frameEnd - frameStart).count();
			if (sleepTime > 0) Sleep(sleepTime);
		}
	}


};

// ----------------- Generate New Level -----------------
void GenerateNewLevel() {
	// Vygenerujeme novou mapu
	LevelData = LevelGenerator::GenerateLevel();

	// Uložíme předchozí mapu pro diff (pokud potřebujeme)
	map<GridPosition, char> previousEntityMap = EntityMap;

	// Vyčistíme aktuální mapu a nastavíme hráče
	EntityMap.clear();
	EntityMap[{1, 1}] = TilePlayer;

	// Umístíme ostatní entity
	PlaceEntitiesRandomly(LevelData, EntityMap, TileEnemy, 5);
	PlaceEntitiesRandomly(LevelData, EntityMap, TileMerchant, 5);
	PlaceEntitiesRandomly(LevelData, EntityMap, TileBoss, 5);
	PlaceEntitiesRandomly(LevelData, EntityMap, TileMiniBoss, 5);

	// První kompletní vykreslení mapy
	LevelRenderer::DrawInitialMap(LevelData, EntityMap);

	// Vykreslení HUD s aktuálním stavem hráče
	HUDBar::ClearHUDBar(2);
	HUDBar::DrawHUDBar(2, PlayerStatus(player));
}

// ----------------- Game Loop -----------------
class GameLoop {
public:
	static void Run(vector<char> levelData, map<GridPosition, char> entityMap) {
		PlayerClass playerInstance;
		playerInstance.position = { 1,1 };

		// Draw map once
		LevelRenderer::DrawInitialMap(levelData, entityMap);
		HUDBar::DrawHUDBar(2, PlayerStatus(player));

		playerInstance.Run(levelData, entityMap);
	}
};

// ----------------- Logo -----------------
void ShowLogo(int marginX = 2) {
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
	int maxWidth = 0;
	for (auto& line : logo) if ((int)line.size() > maxWidth) maxWidth = (int)line.size();
	if (logoHeight > GameFieldHeight || maxWidth > GameFieldWidth) return;

	COORD pos = { 0, 0 };
	SetConsoleCursorPosition(consoleHandle, pos);
	for (int y = 0; y < GameFieldHeight; ++y) {
		for (int i = 0; i < marginX; ++i) cout << ' ';
		for (int x = 0; x < GameFieldWidth; ++x) cout << ' ';
		cout << '\n';
	}

	int startY = (GameFieldHeight - logoHeight) / 2;
	for (int i = 0; i < logoHeight; ++i) {
		const string& line = logo[i];
		int startX = (GameFieldWidth - (int)line.size()) / 2 + marginX;
		COORD p = { (SHORT)startX, (SHORT)(startY + i) };
		SetConsoleCursorPosition(consoleHandle, p);
		if (i <= 4) SetConsoleTextAttribute(consoleHandle, 11); // cyan
		else if (i == 5) SetConsoleTextAttribute(consoleHandle, 7); // white
		else SetConsoleTextAttribute(consoleHandle, 14); // yellow
		cout << line;
	}

	SetConsoleTextAttribute(consoleHandle, 7);
	Sleep(1000);

	COORD clearPos = { 0, (SHORT)startY };
	SetConsoleCursorPosition(consoleHandle, clearPos);
	for (int y = 0; y < logoHeight; ++y) {
		for (int i = 0; i < marginX; ++i) cout << ' ';
		for (int x = 0; x < GameFieldWidth; ++x) cout << ' ';
		cout << '\n';
	}

	pos = { 0, 0 };
	SetConsoleCursorPosition(consoleHandle, pos);
}

// ----------------- Main -----------------
int main() {
	ConsoleUtils::HideCursor();
	int marginX = 2;
	ConsoleUtils::SetConsoleSize(GameFieldWidth, GameFieldHeight, marginX);

	bool firstRun = true;
	while (true) {
		if (firstRun) {
			firstRun = false;
			ShowLogo(marginX);
			GenerateNewLevel();
		}
		PlayerClass playerInstance;
		playerInstance.position = { 1,1 }; // start position
		playerInstance.Run(LevelData, EntityMap);
	}
}
