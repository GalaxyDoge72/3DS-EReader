#include <3ds.h>
#include <dirent.h>
#include <vector>
#include <string>
#include <stdio.h>

using namespace std;

// Check if the /ebooks/ directory exists on the SD card
bool DirExists() {
    DIR* dir = opendir("sdmc:/ebooks");
    if (dir) {
        closedir(dir);
        return true;
    }
    return false;
}

// Search for ePub books in the /ebooks/ directory
vector<string> searchEPub(const string& epubPath) {
    vector<string> foundFiles;
    DIR* dir = opendir(epubPath.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            string name = entry->d_name;
            if (name.size() > 5 && name.substr(name.size() - 5) == ".epub") {
                foundFiles.push_back(name);
            }
        }
        closedir(dir);
    }
    return foundFiles;
}

void drawList(const vector<string>& files, int selectedIndex) {
    consoleClear();
    printf("Select an ePub:\n\n");
    for (size_t i = 0; i < files.size(); i++) {
        if ((int)i == selectedIndex) {
            printf(" > %s\n", files[i].c_str());
        }
        else {
            printf("   %s\n", files[i].c_str());
        }
    }
    printf("\nUse D-Pad UP/DOWN. A to select. Start to exit.");
}

int main(int argc, char** argv) {
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);
    // Panic if we can't find the folder //
    if (!DirExists()) {
        consoleClear();
        printf("The path 'sdmc:/ebooks' was not found.\nEnsure that you have created a folder named 'ebooks' in the ROOT OF YOUR SD CARD.");
        printf("\nPress any key to exit.\n");
        
        while (aptMainLoop()) {
            hidScanInput();
            if (hidKeysDown()) break; // End the program once the user presses buttons //
            gspWaitForVBlank();
        }

        gfxExit();
        return 0;

    }

    vector<string> ePubList = searchEPub("sdmc:/ebooks");
    if (ePubList.empty()) {
        consoleClear();
        printf("No .epub files were found.\n");
        printf("\nPress Start to exit.");
        while (aptMainLoop()) {
            hidScanInput();
            if (hidKeysDown() & KEY_START) break;
            gspWaitForVBlank();
        }
        gfxExit();
        return 0;
    }

    int selected = 0;
    drawList(ePubList, selected);

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        
        if (kDown & KEY_START) break;

        if (kDown & (KEY_CPAD_DOWN | KEY_UP)) {
            selected = (selected + 1) % ePubList.size(); // Ensure wrap around //
            drawList(ePubList, selected);
        }
        if (kDown & (KEY_UP | KEY_CPAD_UP)) {
            selected = (selected - 1 + ePubList.size()) % ePubList.size();
            drawList(ePubList, selected);
        }
    }
}
