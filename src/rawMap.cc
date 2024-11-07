#include <map>
#include <unordered_map>
#include <string_view>
#include <string>
#include <cstring>
#include <cstdlib>
#include <random>
#include <chrono>

using namespace std;
using namespace std::chrono;

int KEY_COUNT = 100000;
int ITERATIONS = 1000000;
int KEY_SIZE = 17;
int VALUE_SIZE = 1000;

static char* characters = (char*)"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

std::random_device rd; // obtain a random number from hardware
std::mt19937 gen(rd()); // seed the generator
std::uniform_int_distribution<> charDistr(0, strlen(characters) - 1); // define the range
std::uniform_int_distribution<> keyDistr(0, KEY_COUNT - 1); // define the range

static char* makeid(int length) {
  char* str = (char*)calloc(sizeof(char), length + 1);
  for (int i = 0; i < length; i++) {
    str[i] = characters[charDistr(gen)];
  }
  return str;
}


int main() {
  unordered_map<string_view, string_view> myMap;

  char** keys = (char**)calloc(sizeof(char**), KEY_COUNT);
  milliseconds startSetup = duration_cast<milliseconds>(system_clock::now().time_since_epoch());

  for (int i = 0; i < KEY_COUNT; i++) {
    keys[i] = makeid(KEY_SIZE);
    char* key = keys[i];
    char* value = makeid(VALUE_SIZE);
    myMap.insert(pair<string_view, string_view>{ string_view(keys[i]), string_view(value) });
  }
  milliseconds startRun = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
  for (int i = 0; i < ITERATIONS; i++) {
    string_view v = myMap.at(keys[keyDistr(gen)]);
    if (keyDistr(gen) > KEY_COUNT) {
      printf("%s\n", v.data());
    }
  }
  milliseconds endRun = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
  printf("Setup: %d, Run: %d\n", startRun - startSetup, endRun - startRun);
}
