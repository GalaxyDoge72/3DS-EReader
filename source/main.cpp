#include <3ds.h>
#include <dirent.h>
#include <vector>
#include <string>
#include <stdio.h>
#include <archive.h>
#include <archive_entry.h>
#include <cstring>
#include <tinyxml2.h>
#include <cctype>
#include <algorithm>
#include <sstream>
#include <map>

using namespace std;
using namespace tinyxml2;


const int maxFileSize = 96 * 1048576; // Allocate 96 MB as maximum file size
const size_t WORD_WRAP_WIDTH = 50; // Set word wrap width to 50 characters

// Make pageSize a global, non-constant variable so it can be modified.
size_t pageSize = 400; // Initial characters per page.

// Define min/max for pageSize
const size_t minPageSize = 200; // Minimum characters per page
const size_t maxPageSize = 2000; // Maximum characters per page
const size_t pageSizeStep = 50; // How much to increment/decrement by
const size_t largePageSizeStep = 100; // Larger step for L/R buttons

enum Colour {
    DEFAULT, WHITE, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, INVALID
};

// Global variable for the selected text color
Colour currentTextColor = DEFAULT;

string toLower(const string& input) {
    string output = input;
    transform(output.begin(), output.end(), output.begin(), [](unsigned char c){ return tolower(c); });
    return output;
}

// Function to get the string name of a color
string getColourName(Colour colour) {
    switch (colour) {
        case RED: return "Red";
        case GREEN: return "Green";
        case YELLOW: return "Yellow";
        case BLUE: return "Blue";
        case MAGENTA: return "Magenta";
        case CYAN: return "Cyan";
        case WHITE: return "White";
        case DEFAULT: return "Default";
        default: return "Invalid";
    }
}

Colour getColourFromString(const string& colourName) {
    string c = toLower(colourName);
    if (c == "red") return RED;
    if (c == "green") return GREEN;
    if (c == "yellow") return YELLOW;
    if (c == "blue") return BLUE;
    if (c == "magenta") return MAGENTA;
    if (c == "cyan") return CYAN;
    if (c == "white") return WHITE;
    if (c == "default") return DEFAULT;
    return INVALID;
}

string printColouredText(const string& input, Colour selectedColour) {
    string colourCode;
    switch (selectedColour) {
        case RED: colourCode = "\x1b[31m"; break;
        case GREEN: colourCode = "\x1b[32m"; break;
        case YELLOW: colourCode = "\x1b[33m"; break;
        case BLUE: colourCode = "\x1b[34m"; break;
        case MAGENTA: colourCode = "\x1b[35m"; break;
        case CYAN: colourCode = "\x1b[36m"; break;
        case WHITE: colourCode = "\x1b[37m"; break;
        case DEFAULT:
        case INVALID:
        default:
            colourCode = "\x1b[0m"; break;
    }
    // For default, we don't need to wrap it, just return the input
    if (selectedColour == DEFAULT) return input;
    
    return colourCode + input + "\x1b[0m";
}


bool DirExists(const char* path) {
    DIR* dir = opendir(path);
    if (dir) {
        closedir(dir);
        return true;
    }
    return false;
}

void replace_all(std::string& s, const std::string& from, const std::string& to) {
    if (from.empty()) return;
    size_t start_pos = 0;
    while((start_pos = s.find(from, start_pos)) != std::string::npos) {
        s.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

std::map<std::string, char> htmlEntities = {
    {"&apos;", '\''}, {"&#39;", '\''},
    {"&quot;", '\"'}, {"&#34;", '\"'},
    {"&amp;", '&'},
    {"&lt;", '<'},
    {"&gt;", '>'},
    {"&lsquo;", '\''}, {"&#x2018;", '\''},
    {"&rsquo;", '\''}, {"&#x2019;", '\''},
    {"&ldquo;", '\"'}, {"&#x201C;", '\"'},
    {"&rdquo;", '\"'}, {"&#x201D;", '\"'},
    {"&mdash;", '-'}, {"&#x2014;", '-'},
    {"&ndash;", '-'}, {"&#x2013;", '-'},
    {"&hellip;", '.'}, {"&#x2026;", '.'},
    {"&nbsp;", ' '},  {"&#xa0;", ' '},
};

std::string decodeHtmlEntities(const std::string& input) {
    std::string output = input;

    for (const auto& pair : htmlEntities) {
        replace_all(output, pair.first, string(1, pair.second));
    }

    replace_all(output, "\xE2\x80\x99", "'");
    replace_all(output, "\xE2\x80\x98", "'");
    replace_all(output, "\xE2\x80\x9C", "\"");
    replace_all(output, "\xE2\x80\x9D", "\"");
    replace_all(output, "\xE2\x80\xA6", "...");
    replace_all(output, "\xE2\x80\x93", "-");
    replace_all(output, "\xE2\x80\x94", "-");

    std::string cleanedOutput;
    for (char c : output) {
        if ((static_cast<unsigned char>(c) >= 32 && static_cast<unsigned char>(c) <= 126) ||
            c == '\n' || c == '\r' || c == '\t') {
            cleanedOutput += c;
        } else if (static_cast<unsigned char>(c) > 126) { // Replace non-ASCII chars
            cleanedOutput += ' ';
        } else {
            cleanedOutput += c; // Keep other control chars like newline
        }
    }
    return cleanedOutput;
}

vector<string> searchEPub(const string& epubPath) {
    vector<string> foundFiles;
    DIR* dir = opendir(epubPath.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            string name = entry->d_name;
            if (name.length() > 5 && name.substr(name.length() - 5) == ".epub") {
                foundFiles.push_back(name);
            }
        }
        closedir(dir);
    }
    return foundFiles;
}

// Consolidated drawing function for the main menu to prevent redundant code.
void drawMainMenu(const vector<string>& files, int selectedIndex) {
    consoleClear();
    printf("Select an ePub:\n\n");
    for (size_t i = 0; i < files.size(); i++) {
        if ((int)i == selectedIndex) {
            printf(" > %s\n", files[i].c_str());
        } else {
            printf("   %s\n", files[i].c_str());
        }
    }
    printf("\nUse D-Pad UP/DOWN to navigate.\n");
    printf("A: Select | START: Exit | SELECT: Settings\n");
}


vector<string> paginateFile(const string& fullText, size_t pageChars) {
    vector<string> pages;
    if (fullText.empty()) return pages;
    
    size_t pos = 0;
    while (pos < fullText.length()) {
        size_t len = min(pageChars, fullText.length() - pos);
        pages.push_back(fullText.substr(pos, len));
        pos += len;
    }
    return pages;
}

void extractText(XMLNode* node, string& output) {
    if (!node) return;

    if (XMLElement* elem = node->ToElement()) {
        string tag = toLower(elem->Name());
        if (tag == "script" || tag == "style") return;

        if (tag == "p" || tag == "div" || tag == "h1" || tag == "h2" || tag == "h3") {
            if (!output.empty() && output.back() != '\n') output += "\n";
        }
        if (tag == "br") output += "\n";
    }

    if (XMLText* text = node->ToText()) {
        string textValue = text->Value();
        size_t first = textValue.find_first_not_of(" \t\n\r");
        if (string::npos != first) {
            size_t last = textValue.find_last_not_of(" \t\n\r");
            textValue = textValue.substr(first, (last - first + 1));
            
            if (!output.empty() && output.back() != ' ' && output.back() != '\n') {
                output += " ";
            }
            output += textValue;
        }
    }

    for (XMLNode* child = node->FirstChild(); child; child = child->NextSibling()) {
        extractText(child, output);
    }

    if (XMLElement* elem = node->ToElement()) {
        string tag = toLower(elem->Name());
        if (tag == "p" || tag == "div" || tag == "h1" || tag == "h2" || tag == "h3") {
            if (!output.empty() && output.back() != '\n') output += "\n";
        }
    }
}


void displayPage(const vector<string>& pages, int currentPage) {
    consoleClear();
    if (currentPage >= 0 && currentPage < (int)pages.size()) {
        // Apply coloring to the page content
        printf("%s\n\n", printColouredText(pages[currentPage], currentTextColor).c_str());
        printf("Page %d of %d\n", currentPage + 1, (int)pages.size());
        printf("L/R: Prev/Next | B: Back\n");
    } else {
        printf("\x1b[31mInvalid Page Number!\x1b[0m\n");
    }
    gfxFlushBuffers();
    gfxSwapBuffers();
}

string wordWrap(const string& input, size_t maxWidth) {
    stringstream ss(input);
    string line, wrappedText;
    
    while (getline(ss, line, '\n')) {
        string currentLine;
        stringstream words(line);
        string word;
        while (words >> word) {
            if (currentLine.length() + word.length() + 1 > maxWidth) {
                wrappedText += currentLine + "\n";
                currentLine = "";
            }
            if (!currentLine.empty()) currentLine += " ";
            currentLine += word;
        }
        wrappedText += currentLine + "\n";
    }
    return wrappedText;
}

vector<string> getChapterList(const char* epubPath) {
    vector<string> chapters;
    struct archive* a = archive_read_new();
    archive_read_support_format_zip(a);

    if (archive_read_open_filename(a, epubPath, 10240) != ARCHIVE_OK) {
        archive_read_free(a);
        return chapters;
    }

    struct archive_entry* entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        const char* name = archive_entry_pathname(entry);
        if (strstr(name, ".xhtml") || strstr(name, ".html")) {
            chapters.push_back(name);
        }
        archive_read_data_skip(a);
    }
    archive_read_free(a);
    return chapters;
}

void readAndDisplayChapter(const char* epubPath, const char* chapterPath) {
    struct archive* a = archive_read_new();
    archive_read_support_format_zip(a);

    if (archive_read_open_filename(a, epubPath, 10240) != ARCHIVE_OK) {
        printf("Failed to open EPUB: %s\n", archive_error_string(a));
        archive_read_free(a);
        return;
    }

    struct archive_entry* entry;
    string fullText;
    bool chapterFound = false;

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        if (strcmp(archive_entry_pathname(entry), chapterPath) == 0) {
            chapterFound = true;
            char buffer[4096];
            string fileContent;
            ssize_t size;

            while ((size = archive_read_data(a, buffer, sizeof(buffer))) > 0) {
                fileContent.append(buffer, size);
            }

            fileContent = decodeHtmlEntities(fileContent);

            XMLDocument doc;
            if (doc.Parse(fileContent.c_str()) == XML_SUCCESS) {
                if (XMLElement* body = doc.FirstChildElement("html")->FirstChildElement("body")) {
                    extractText(body, fullText);
                }
            }
            break;
        }
        archive_read_data_skip(a);
    }
    archive_read_free(a);

    if (!chapterFound) {
        printf("Could not find chapter: %s\n", chapterPath);
        return;
    }

    fullText = wordWrap(fullText, WORD_WRAP_WIDTH);
    vector<string> pages = paginateFile(fullText, pageSize);
    int currentPage = 0;
    
    displayPage(pages, currentPage);
    while (aptMainLoop()) {
        hidScanInput();
        u32 kdown = hidKeysDown();

        if (kdown & KEY_B) break;
        if ((kdown & KEY_L) && currentPage > 0) {
            currentPage--;
            displayPage(pages, currentPage);
        }
        if ((kdown & KEY_R) && currentPage < (int)pages.size() - 1) {
            currentPage++;
            displayPage(pages, currentPage);
        }
        if (kdown & (KEY_UP | KEY_CPAD_UP)) { // Adjust page size
             if (pageSize < maxPageSize) pageSize = min(maxPageSize, pageSize + pageSizeStep);
             pages = paginateFile(fullText, pageSize);
             displayPage(pages, currentPage);
        }
        if (kdown & (KEY_DOWN | KEY_CPAD_DOWN)) { // Adjust page size
             if (pageSize > minPageSize) pageSize = max(minPageSize, pageSize - pageSizeStep);
             pages = paginateFile(fullText, pageSize);
             displayPage(pages, currentPage);
        }
        
        gspWaitForVBlank();
    }
}

void drawChapterList(const vector<string>& files, int selectedIndex) {
    consoleClear();
    printf("Select a chapter:\n\n");
    for (size_t i = 0; i < files.size(); i++) {
        string displayName = files[i];
        size_t lastSlash = displayName.find_last_of('/');
        if(lastSlash != string::npos) {
            displayName = displayName.substr(lastSlash + 1);
        }

        if ((int)i == selectedIndex) {
            printf(" > %s\n", displayName.c_str());
        } else {
            printf("   %s\n", displayName.c_str());
        }
    }
    printf("\nUse D-Pad UP/DOWN to navigate. A to select. B to go back.\n");
    gfxFlushBuffers();
    gfxSwapBuffers();
}


void displayChapterMenu(const char* epubPath) {
    vector<string> chapterList = getChapterList(epubPath);

    if (chapterList.empty()) {
        consoleClear();
        printf("No chapters (.xhtml or .html files) found in this EPUB.\n");
        printf("\nPress B to return.\n");
        gfxFlushBuffers();
        gfxSwapBuffers();
        while (aptMainLoop()) {
            hidScanInput();
            if (hidKeysDown() & KEY_B) break;
            gspWaitForVBlank();
        }
        return;
    }

    int selected = 0;
    bool needsRedraw = true; // Flag to control drawing

    while (aptMainLoop()) {
        // Only draw the menu when the selection changes or we return from the reader
        if (needsRedraw) {
            drawChapterList(chapterList, selected);
            needsRedraw = false; // Reset the flag after drawing
        }

        hidScanInput();
        u32 kDown = hidKeysDown();

        if (kDown & KEY_B) break;

        if (kDown & (KEY_DOWN | KEY_CPAD_DOWN)) {
            selected = (selected + 1) % chapterList.size();
            needsRedraw = true; // Flag for redraw
        }
        if (kDown & (KEY_UP | KEY_CPAD_UP)) {
            selected = (selected - 1 + chapterList.size()) % chapterList.size();
            needsRedraw = true; // Flag for redraw
        }
        if (kDown & KEY_A) {
            readAndDisplayChapter(epubPath, chapterList[selected].c_str());
            needsRedraw = true; // After returning from the reader, redraw the menu
        }

        gspWaitForVBlank();
    }
}
void displaySettingsMenu() {
    int selectedSetting = 0;
    const int numSettings = 2;
    bool needsRedraw = true; // Flag to control drawing

    while (aptMainLoop()) {
        // Only draw the menu when a setting changes
        if (needsRedraw) {
            consoleClear();
            printf("--- Settings ---\n\n");
            printf("%s Page Size: %zu\n", (selectedSetting == 0 ? ">" : " "), pageSize);
            printf("%s Text Color: %s\n", (selectedSetting == 1 ? ">" : " "), getColourName(currentTextColor).c_str());
            printf("\n\nUse D-Pad UP/DOWN to select.\n");
            printf("Use D-Pad LEFT/RIGHT to change value.\n");
            printf("L/R buttons for large page size adjustment.\n");
            printf("B to go back.\n");
            gfxFlushBuffers();
            gfxSwapBuffers();
            needsRedraw = false; // Reset the flag
        }

        hidScanInput();
        u32 kDown = hidKeysDown();

        if (kDown & KEY_B) break;

        // --- Input checks that trigger a redraw ---
        if (kDown & (KEY_DOWN | KEY_CPAD_DOWN)) {
            selectedSetting = (selectedSetting + 1) % numSettings;
            needsRedraw = true;
        }
        if (kDown & (KEY_UP | KEY_CPAD_UP)) {
            selectedSetting = (selectedSetting - 1 + numSettings) % numSettings;
            needsRedraw = true;
        }

        if (selectedSetting == 0) { // Page Size
            if ((kDown & (KEY_DLEFT | KEY_CPAD_LEFT)) && pageSize > minPageSize) {
                pageSize = max(minPageSize, pageSize - pageSizeStep);
                needsRedraw = true;
            }
            if ((kDown & KEY_L) && pageSize > minPageSize) {
                 pageSize = max(minPageSize, pageSize - largePageSizeStep);
                 needsRedraw = true;
            }
            if ((kDown & (KEY_DRIGHT | KEY_CPAD_RIGHT)) && pageSize < maxPageSize) {
                pageSize = min(maxPageSize, pageSize + pageSizeStep);
                needsRedraw = true;
            }
            if ((kDown & KEY_R) && pageSize < maxPageSize) {
                 pageSize = min(maxPageSize, pageSize + largePageSizeStep);
                 needsRedraw = true;
            }
        } else if (selectedSetting == 1) { // Text Color
            if (kDown & (KEY_DRIGHT | KEY_CPAD_RIGHT)) {
                currentTextColor = (Colour)((currentTextColor + 1) % (CYAN + 1));
                needsRedraw = true;
            }
            if (kDown & (KEY_DLEFT | KEY_CPAD_LEFT)) {
                currentTextColor = (Colour)((currentTextColor - 1 + (CYAN + 1)) % (CYAN + 1));
                needsRedraw = true;
            }
        }
        
        gspWaitForVBlank();
    }
}

int main(int argc, char** argv) {
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    const char* ebookDir = "sdmc:/ebooks";

    if (!DirExists(ebookDir)) {
        consoleClear();
        printf("Directory '%s' not found.\n", ebookDir);
        printf("Please create an 'ebooks' folder on the root of your SD card.\n");
        printf("\nPress START to exit.\n");
        while (aptMainLoop()) {
            hidScanInput();
            if (hidKeysDown() & KEY_START) break;
            gspWaitForVBlank();
        }
        gfxExit();
        return 0;
    }

    vector<string> ePubList = searchEPub(ebookDir);
    if (ePubList.empty()) {
        consoleClear();
        printf("No .epub files found in '%s'.\n", ebookDir);
        printf("\nPress START to exit.\n");
        while (aptMainLoop()) {
            hidScanInput();
            if (hidKeysDown() & KEY_START) break;
            gspWaitForVBlank();
        }
        gfxExit();
        return 0;
    }

    int selected = 0;
    
    // Initial draw of the main menu
    drawMainMenu(ePubList, selected);
    gfxFlushBuffers();
    gfxSwapBuffers();

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();

        bool needsRedraw = false;

        if (kDown & KEY_START) break;

        if (kDown & (KEY_DOWN | KEY_CPAD_DOWN)) {
            selected = (selected + 1) % ePubList.size();
            needsRedraw = true;
        }
        if (kDown & (KEY_UP | KEY_CPAD_UP)) {
            selected = (selected - 1 + ePubList.size()) % ePubList.size();
            needsRedraw = true;
        }
        if (kDown & KEY_A) {
            string path = string(ebookDir) + "/" + ePubList[selected];
            displayChapterMenu(path.c_str());
            needsRedraw = true; 
        }
        if (kDown & KEY_SELECT) {
            displaySettingsMenu();
            needsRedraw = true;
        }

        if (needsRedraw) {
            drawMainMenu(ePubList, selected);
            gfxFlushBuffers();
            gfxSwapBuffers();
        }
        
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}