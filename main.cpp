#include <iostream>
#include <random>
#include <vector>
#include <map>
#include <tuple>
#include <queue>
#include <conio.h>
#include <Windows.h>

using namespace std;

random_device rd;
mt19937 rng(rd());

const int GAME_FIELD_WIDTH = 80;
const int GAME_FIELD_HEIGHT = 30;
const char TILE_WALL = '#';
const char TILE_GROUND = ' ';
const char TILE_PLAYER = 'P';
const char TILE_ENEMY = 'E';
const char TILE_MERCHANT = 'M';
const char TILE_BOSS = 'B';
const char TILE_MINIBOSS = 'b';

struct GridPosition {
	int x, y;
	GridPosition(int xPos = 0, int yPos = 0) : x(xPos), y(yPos) {}
	bool operator<(const GridPosition& other) const { return tie(y, x) < tie(other.y, other.x); }
	bool operator==(const GridPosition& other) const { return x == other.x && y == other.y; }
};

vector<char> level;
map<GridPosition, char> entities;

struct Coord { int x, y; };

// ----------------- Level Generation -----------------
class LevelGenerator {
public:
	static int randomInt(int min, int max) {
		uniform_int_distribution<> dist(min, max);
		return dist(rng);
	}

	static vector<char> generateLevel() {
		vector<char> level(GAME_FIELD_WIDTH * GAME_FIELD_HEIGHT, TILE_GROUND);

		// Add border walls
		for (int x = 0; x < GAME_FIELD_WIDTH; ++x) {
			level[x] = TILE_WALL;
			level[(GAME_FIELD_HEIGHT - 1) * GAME_FIELD_WIDTH + x] = TILE_WALL;
		}
		for (int y = 0; y < GAME_FIELD_HEIGHT; ++y) {
			level[y * GAME_FIELD_WIDTH] = TILE_WALL;
			level[y * GAME_FIELD_WIDTH + (GAME_FIELD_WIDTH - 1)] = TILE_WALL;
		}

		int playerX = 1, playerY = 1;
		int walkableTiles = GAME_FIELD_WIDTH * GAME_FIELD_HEIGHT - 2 * GAME_FIELD_WIDTH - 2 * (GAME_FIELD_HEIGHT - 2) - 1;
		int maxClusters = 150;

		for (int attempt = 0; attempt < maxClusters; ++attempt) {
			int w = randomInt(2, 4);
			int h = randomInt(2, 4);
			int x = randomInt(1, GAME_FIELD_WIDTH - w - 2);
			int y = randomInt(1, GAME_FIELD_HEIGHT - h - 2);

			vector<int> clusterIndices;
			for (int dy = 0; dy < h; ++dy)
				for (int dx = 0; dx < w; ++dx) {
					int index = (y + dy) * GAME_FIELD_WIDTH + (x + dx);
					if (level[index] == TILE_GROUND) {
						clusterIndices.push_back(index);
						level[index] = TILE_WALL;
					}
				}

			// BFS to check connectivity
			vector<bool> visited(GAME_FIELD_WIDTH * GAME_FIELD_HEIGHT, false);
			queue<Coord> q;
			q.push({ playerX, playerY });
			visited[playerY * GAME_FIELD_WIDTH + playerX] = true;

			int reachable = 1;
			const int dxArr[] = { 1, -1, 0, 0 };
			const int dyArr[] = { 0, 0, 1, -1 };

			while (!q.empty()) {
				Coord cur = q.front(); q.pop();
				for (int d = 0; d < 4; ++d) {
					int nx = cur.x + dxArr[d];
					int ny = cur.y + dyArr[d];
					int idx = ny * GAME_FIELD_WIDTH + nx;
					if (nx > 0 && nx < GAME_FIELD_WIDTH - 1 && ny > 0 && ny < GAME_FIELD_HEIGHT - 1 &&
						!visited[idx] && level[idx] == TILE_GROUND) {
						visited[idx] = true;
						++reachable;
						q.push({ nx, ny });
					}
				}
			}

			if (reachable < walkableTiles - (int)clusterIndices.size()) {
				for (int idx : clusterIndices) level[idx] = TILE_GROUND;
			}
			else {
				walkableTiles -= (int)clusterIndices.size();
			}
		}

		return level;
	}
};

// ----------------- Console -----------------
void hideCursor() {
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_CURSOR_INFO info;
	GetConsoleCursorInfo(h, &info);
	info.bVisible = FALSE;
	SetConsoleCursorInfo(h, &info);
}

void setConsoleSize(int width, int height, int marginX = 2) {
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	COORD buffer = { (SHORT)(width + marginX * 2), (SHORT)(height + 2) };
	SetConsoleScreenBufferSize(h, buffer);
	SMALL_RECT win = { 0,0, buffer.X - 1, buffer.Y - 1 };
	SetConsoleWindowInfo(h, TRUE, &win);
	HWND hwnd = GetConsoleWindow();
	LONG style = GetWindowLong(hwnd, GWL_STYLE);
	style &= ~WS_MAXIMIZEBOX; style &= ~WS_SIZEBOX;
	SetWindowLong(hwnd, GWL_STYLE, style);
}

// ----------------- Renderer -----------------
class LevelRenderer {
public:
	static void draw(const vector<char>& level, const map<GridPosition, char>& entities, int marginX = 2) {
		HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		COORD pos = { 0, 0 };
		SetConsoleCursorPosition(hConsole, pos);

		for (int y = 0; y < GAME_FIELD_HEIGHT; ++y) {
			for (int i = 0; i < marginX; ++i) cout << ' ';
			for (int x = 0; x < GAME_FIELD_WIDTH; ++x) {
				GridPosition p{ x, y };
				char ch;
				if (entities.count(p)) ch = entities.at(p);
				else ch = level[y * GAME_FIELD_WIDTH + x];

				// Set color based on symbol
				switch (ch) {
				case TILE_WALL: SetConsoleTextAttribute(hConsole, 8); break;       // Gray
				case TILE_PLAYER: SetConsoleTextAttribute(hConsole, 9); break;     // Blue
				case TILE_ENEMY: SetConsoleTextAttribute(hConsole, 14); break;     // Yellow
				case TILE_MERCHANT: SetConsoleTextAttribute(hConsole, 11); break;  // Cyan
				case TILE_MINIBOSS: SetConsoleTextAttribute(hConsole, 13); break;  // Magenta
				case TILE_BOSS: SetConsoleTextAttribute(hConsole, 4); break;       // Red
				default: SetConsoleTextAttribute(hConsole, 7); break;              // white
				}

				cout << ch;
			}
			cout << '\n';
		}

		// Reset color
		SetConsoleTextAttribute(hConsole, 7);
	}
};

vector<GridPosition> getWalkableTiles(const vector<char>& level) {
	vector<GridPosition> walkable;
	for (int y = 0; y < GAME_FIELD_HEIGHT; ++y) {
		for (int x = 0; x < GAME_FIELD_WIDTH; ++x) {
			if (level[y * GAME_FIELD_WIDTH + x] == TILE_GROUND) {
				walkable.push_back({ x, y });
			}
		}
	}
	return walkable;
}
void placeEntitiesRandomly(vector<char>& level, map<GridPosition, char>& entities, char entityChar, int count);
void generateNewLevel() {
	level = LevelGenerator::generateLevel();
	entities.clear();
	entities[{1, 1}] = TILE_PLAYER;
	placeEntitiesRandomly(level, entities, TILE_ENEMY, 5);
	placeEntitiesRandomly(level, entities, TILE_MERCHANT, 5);
	placeEntitiesRandomly(level, entities, TILE_BOSS, 5);
	placeEntitiesRandomly(level, entities, TILE_MINIBOSS, 5);
	LevelRenderer::draw(level, entities);
}
// ----------------- Player -----------------
class PlayerController {
public:


	// Úprava PlayerController::run
	static void run() {
		GridPosition playerPos = findPlayer(entities);
		LevelRenderer::draw(level, entities);

		while (true) {
			if (_kbhit()) {
				char c = _getch();
				GridPosition newPos = playerPos;
				switch (tolower(c)) {
				case 'w': newPos.y--; break;
				case 's': newPos.y++; break;
				case 'a': newPos.x--; break;
				case 'd': newPos.x++; break;
				case 'q': exit(0); break;
				case 'e':
					generateNewLevel();
					return; // restart
				
				default: continue;
				}
				if (!walkable(newPos, level)) continue;

				bool encountered = checkEntityEncounter(entities, newPos);
				if (encountered) {
					entities.erase(newPos);
				}

				entities.erase(playerPos);
				entities[newPos] = TILE_PLAYER;
				playerPos = newPos;
				LevelRenderer::draw(level, entities);
			}
			Sleep(5);
		}
	}

private:
	static GridPosition findPlayer(const map<GridPosition, char>& entities) {
		for (auto& [pos, sym] : entities) if (sym == TILE_PLAYER) return pos;
		return { -1,-1 };
	}

	static bool walkable(const GridPosition& pos, const vector<char>& level) {
		if (pos.x < 0 || pos.x >= GAME_FIELD_WIDTH || pos.y < 0 || pos.y >= GAME_FIELD_HEIGHT) return false;
		return level[pos.y * GAME_FIELD_WIDTH + pos.x] == TILE_GROUND;
	}

	static void waitForEnter() {
		while (true) {
			int ch = _getch();
			if (ch == 13) break; // Enter
		}
	}
	class encounter {
	public:
		static void onEnemyEncounter() {
			cout << "enemy   " << endl;
			//waitForEnter();
		}

		static void onMerchantEncounter() {
			cout << "merchant" << endl;
			//waitForEnter();
		}

		static void onBossEncounter() {
			cout << "boss    " << endl;
			//waitForEnter();
		}

		static void onMinibossEncounter() {
			cout << "miniboss" << endl;
			//waitForEnter();
		}
	};



	static bool checkEntityEncounter(const map<GridPosition, char>& entities, const GridPosition& pos) {
		auto it = entities.find(pos);
		if (it == entities.end()) return false; // žádná entita
		encounter enc;
		switch (it->second) {
		case TILE_ENEMY:
			enc.onEnemyEncounter();
			return true;
		case TILE_MERCHANT:
			enc.onMerchantEncounter();
			return true;
		case TILE_BOSS:
			enc.onBossEncounter();
			return true;
		case TILE_MINIBOSS:
			enc.onMinibossEncounter();
			return true;
		default:
			return false;
		}
	}
};






// Returns a vector of all walkable positions (not walls)


// Randomly places 'count' entities with symbol 'entityChar' on walkable tiles
void placeEntitiesRandomly(vector<char>& level, map<GridPosition, char>& entities, char entityChar, int count) {
	vector<GridPosition> walkable = getWalkableTiles(level);

	shuffle(walkable.begin(), walkable.end(), rng);

	int placed = 0;
	for (auto& pos : walkable) {
		// Skip if already an entity exists here
		if (entities.count(pos)) continue;

		entities[pos] = entityChar;
		++placed;
		if (placed >= count) break;
	}
}

class GameLoop
{
public:
	static void run(vector<char> level, map<GridPosition, char> entities) {
		LevelRenderer::draw(level, entities);
		PlayerController::run();
	}
};



// ----------------- Main -----------------
int main() {
	hideCursor();
	int marginX = 2;
	setConsoleSize(GAME_FIELD_WIDTH, GAME_FIELD_HEIGHT, marginX);
	bool firstRun = true;
	while (true) {
		if (firstRun) {
			firstRun = false;
			generateNewLevel();
		}
		PlayerController::run();
	}
}
