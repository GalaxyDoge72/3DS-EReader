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
#include <sys/stat.h>
#include <fstream>
#include <stdexcept>

using namespace std;
using namespace tinyxml2;


const int maxFileSize = 96 * 1048576; // Allocate 96 MB as maximum file size
const size_t WORD_WRAP_WIDTH = 50; // Set word wrap width to 50 characters

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

struct AppSettings {
    size_t pageSize;
    Colour currentTextColor;
};

AppSettings currentSettings = {
    .pageSize = 400, .currentTextColor = DEFAULT
};

bool DirExists(const char* path) {
    DIR* dir = opendir(path);
    if (dir) {
        closedir(dir);
        return true;
    }
    return false;
}

bool settingsFileExists() {
    const char* settingsPath = "sdmc:/settings/ereader/settings.inf";
    FILE* file = fopen(settingsPath, "r");

    if (file) {
        fclose(file);
        return true;
    }
    else {
        return false;
    }
}

bool createSettingsDirRecursive() {
    if (!DirExists("sdmc:/settings")) {
        if (mkdir("sdmc:/settings", 0777) != 0 && errno != EEXIST) return false;
    }
    if (!DirExists("sdmc:/settings/ereader")) {
        if (mkdir("sdmc:/settings/ereader", 0777) != 0 && errno != EEXIST) return false;
    }
    return true;
}

void saveSettings(const AppSettings& settings) {
    if (!createSettingsDirRecursive()) return;

    FILE* file = fopen("sdmc:/settings/ereader/settings.inf", "w");
    if (file) {
        fprintf(file, "pageSize=%zu\n", settings.pageSize);
        fprintf(file, "textColour=%s\n", getColourName(settings.currentTextColor).c_str());
        fclose(file);
    }
}

void loadSettings(AppSettings& settings) {
    if (!settingsFileExists()) {
        saveSettings(settings);
        return;
    }

    ifstream file("sdmc:/settings/ereader/settings.inf");
    string line;
    if (file.is_open()) {
        while (getline(file, line)){
            string key;
            string value;
            size_t delimiterPos = line.find('=');

            if (delimiterPos != string::npos) {
                key = line.substr(0, delimiterPos);
                value = line.substr(delimiterPos + 1);

                if (key == "pageSize") {
                    try {
                        settings.pageSize = stoul(value);
                        if (settings.pageSize < minPageSize) settings.pageSize = minPageSize;
                        if (settings.pageSize > maxPageSize) settings.pageSize = maxPageSize;
                    }
                    catch (const exception& e) {
                        settings.pageSize = 400;
                    }
                }
                else if (key == "currentTextColor") {
                    currentSettings.currentTextColor = getColourFromString(value);
                }

            }
        }
        file.close();
    }
    else {
        printf("error loading settings fire");
    }
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

void recursiveSearchEPub(const std::string& basePath, std::map<std::string, std::vector<std::string>>& epubFilesByDir) {
    DIR* dir = opendir(basePath.c_str());
    if (!dir) {
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") {
            continue;
        }

        std::string fullPath = basePath + "/" + name;
        struct stat s;
        if (stat(fullPath.c_str(), &s) == 0) {
            if (S_ISDIR(s.st_mode)) {
                // It's a directory, so recurse into it
                recursiveSearchEPub(fullPath, epubFilesByDir);
            } else if (S_ISREG(s.st_mode)) {
                // It's a regular file, check if it's an .epub
                if (name.length() > 5 && name.substr(name.length() - 5) == ".epub") {
                    epubFilesByDir[basePath].push_back(name);
                }
            }
        }
    }
    closedir(dir);
}


vector<string> paginateFile(const string& fullText, size_t pageSize) {
    vector<string> pages;
    size_t pos = 0;

    while (pos < fullText.size()) {
        size_t end = min(pageSize, fullText.size() - pos);
        pages.push_back(fullText.substr(pos, end));
        pos += end;
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
        printf("%s", printColouredText(pages[currentPage], currentTextColor).c_str());
        printf("\x1b[29;1HPage %d of %d", currentPage + 1, (int)pages.size());
        printf("\x1b[30;1HL/R: Prev/Next | B: Back");
    }
    else {
        printf("\x1b[31mInvalid Page Number!\x1b[0m");
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

    // Sort the chapters alphabetically to ensure numerical order
    std::sort(chapters.begin(), chapters.end());
    
    return chapters;
}


// This function replaces the chapter menu and reads the entire book into one document
void readAndDisplayBook(const char* epubPath) {
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

    string fullBookText; // This will hold the content of the entire book

    struct archive* a = archive_read_new();
    archive_read_support_format_zip(a);

    if (archive_read_open_filename(a, epubPath, 10240) != ARCHIVE_OK) {
        printf("Failed to open EPUB: %s\n", archive_error_string(a));
        archive_read_free(a);
        return;
    }

    // Loop through each chapter file in the sorted list
    for (const auto& chapterPath : chapterList) {
        // We have to re-open the archive for each file search as libarchive works as a stream.
        // This is inefficient but necessary for this library's API.
        archive_read_free(a); 
        a = archive_read_new();
        archive_read_support_format_zip(a);
        archive_read_open_filename(a, epubPath, 10240);

        struct archive_entry* entry;
        while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
            if (strcmp(archive_entry_pathname(entry), chapterPath.c_str()) == 0) {
                char buffer[4096];
                string fileContent;
                ssize_t size;

                while ((size = archive_read_data(a, buffer, sizeof(buffer))) > 0) {
                    fileContent.append(buffer, size);
                }

                fileContent = decodeHtmlEntities(fileContent);

                XMLDocument doc;
                string extractedText;
                if (doc.Parse(fileContent.c_str()) == XML_SUCCESS) {
                    if (XMLElement* body = doc.FirstChildElement("html")->FirstChildElement("body")) {
                        extractText(body, extractedText);
                        fullBookText += extractedText + "\n\n"; // Append text with separators
                    }
                }
                break; 
            }
            archive_read_data_skip(a);
        }
    }
    archive_read_free(a);

    // --- Now, handle the combined text just like you did for a single chapter ---

    string wrappedFullText = wordWrap(fullBookText, WORD_WRAP_WIDTH);
    vector<string> pages = paginateFile(wrappedFullText, currentSettings.pageSize);
    int currentPage = 0;
    
    displayPage(pages, currentPage);
    while (aptMainLoop()) {
        hidScanInput();
        u32 kdown = hidKeysDown();

        if (kdown & KEY_B) {
            saveSettings(currentSettings);
            break;
        }
        if ((kdown & KEY_L) && currentPage > 0) {
            currentPage--;
            displayPage(pages, currentPage);
        }
        if ((kdown & KEY_R) && currentPage < (int)pages.size() - 1) {
            currentPage++;
            displayPage(pages, currentPage);
        }
        
        if (kdown & (KEY_UP | KEY_CPAD_UP)) {
            if (currentSettings.pageSize < maxPageSize) {
                currentSettings.pageSize = min(maxPageSize, currentSettings.pageSize + pageSizeStep);
                pages = paginateFile(wrappedFullText, currentSettings.pageSize);
                if (currentPage >= (int)pages.size()) currentPage = (int)pages.size() - 1;
                if (currentPage < 0) currentPage = 0;
                displayPage(pages, currentPage);
            }
        }
        if (kdown & (KEY_DOWN | KEY_CPAD_DOWN)) {
            if (currentSettings.pageSize > minPageSize) {
                currentSettings.pageSize = max(minPageSize, currentSettings.pageSize - pageSizeStep);
                pages = paginateFile(wrappedFullText, currentSettings.pageSize);
                if (currentPage >= (int)pages.size()) currentPage = (int)pages.size() - 1;
                if (currentPage < 0) currentPage = 0;
                displayPage(pages, currentPage);
            }
        }
        
        gspWaitForVBlank();
    }
}

void displaySettingsMenu() {
    int selectedSetting = 0;
    const int numSettings = 2;
    bool needsRedraw = true; 

    while (aptMainLoop()) {
        if (needsRedraw) {
            consoleClear();
            printf("--- Settings ---\n\n");

            printf("%s Page Size: %zu\n",
                   (selectedSetting == 0 ? ">" : " "),
                   currentSettings.pageSize);

            printf("%s Text Color: %s\n",
                   (selectedSetting == 1 ? ">" : " "),
                   getColourName(currentSettings.currentTextColor).c_str());

            printf("\n\nUse D-Pad UP/DOWN to select.\n");
            printf("Use D-Pad LEFT/RIGHT to change value.\n");
            printf("L/R buttons for large page size adjustment.\n");
            printf("B to save and go back.\n");

            gfxFlushBuffers();
            gfxSwapBuffers();
            needsRedraw = false;
        }

        hidScanInput();
        u32 kDown = hidKeysDown();

        if (kDown & KEY_B) {
            saveSettings(currentSettings);
            break;
        }

        if (kDown & (KEY_DOWN | KEY_CPAD_DOWN)) {
            selectedSetting = (selectedSetting + 1) % numSettings;
            needsRedraw = true;
        }
        if (kDown & (KEY_UP | KEY_CPAD_UP)) {
            selectedSetting = (selectedSetting - 1 + numSettings) % numSettings;
            needsRedraw = true;
        }

        if (selectedSetting == 0) { // Page Size
            if ((kDown & (KEY_DLEFT | KEY_CPAD_LEFT)) && currentSettings.pageSize > minPageSize) {
                currentSettings.pageSize = max(minPageSize, currentSettings.pageSize - pageSizeStep);
                needsRedraw = true;
            }
            if ((kDown & KEY_L) && currentSettings.pageSize > minPageSize) {
                currentSettings.pageSize = max(minPageSize, currentSettings.pageSize - largePageSizeStep);
                needsRedraw = true;
            }
            if ((kDown & (KEY_DRIGHT | KEY_CPAD_RIGHT)) && currentSettings.pageSize < maxPageSize) {
                currentSettings.pageSize = min(maxPageSize, currentSettings.pageSize + pageSizeStep);
                needsRedraw = true;
            }
            if ((kDown & KEY_R) && currentSettings.pageSize < maxPageSize) {
                currentSettings.pageSize = min(maxPageSize, currentSettings.pageSize + largePageSizeStep);
                needsRedraw = true;
            }
        } else if (selectedSetting == 1) { // Text Color
            if (kDown & (KEY_DRIGHT | KEY_CPAD_RIGHT)) {
                currentSettings.currentTextColor = (Colour)((currentSettings.currentTextColor + 1) % (CYAN + 1));
                needsRedraw = true;
            }
            if (kDown & (KEY_DLEFT | KEY_CPAD_LEFT)) {
                currentSettings.currentTextColor = (Colour)((currentSettings.currentTextColor - 1 + (CYAN + 1)) % (CYAN + 1));
                needsRedraw = true;
            }
        }

        gspWaitForVBlank();
    }
}

void drawDirectoryMenu(const std::vector<std::string>& dirs, int selectedIndex) {
    consoleClear();
    printf("Select a Directory:\n\n");

    for (size_t i = 0; i < dirs.size(); i++) {
        std::string displayName = dirs[i];
        if (dirs[i] == "sdmc:/ebooks") {
            displayName = "Root 'ebooks' Folder";
        } else {
            size_t lastSlash = dirs[i].find_last_of('/');
            if (lastSlash != std::string::npos) {
                displayName = dirs[i].substr(lastSlash + 1);
            }
        }
        
        if ((int)i == selectedIndex) {
            printf(" > %s\n", displayName.c_str());
        } else {
            printf("   %s\n", displayName.c_str());
        }
    }

    printf("\nUse D-Pad to navigate.\n");
    printf("A: Select | START: Exit | SELECT: Settings\n");
}

void drawBookList(const std::string& dirName, const std::vector<std::string>& books, int selectedIndex) {
    consoleClear();
    
    std::string displayName = dirName;
    if (dirName == "sdmc:/ebooks") {
        displayName = "Root 'ebooks' Folder";
    } else {
        size_t lastSlash = dirName.find_last_of('/');
        if (lastSlash != std::string::npos) {
            displayName = dirName.substr(lastSlash + 1);
        }
    }
    printf("Directory: %s\n\n", displayName.c_str());

    if (books.empty()) {
        printf("No books found in this directory.\n");
    } else {
        for (size_t i = 0; i < books.size(); i++) {
            if ((int)i == selectedIndex) {
                printf(" > %s\n", books[i].c_str());
            } else {
                printf("   %s\n", books[i].c_str());
            }
        }
    }

    printf("\nUse D-Pad to navigate books.\n");
    printf("A: Select Book | B: Back to Directories\n");
}

void displayBookMenu(const std::string& dirPath, const std::vector<std::string>& books) {
    int selectedBook = 0;
    bool needsRedraw = true;

    while (aptMainLoop()) {
        if (needsRedraw) {
            drawBookList(dirPath, books, selectedBook);
            gfxFlushBuffers();
            gfxSwapBuffers();
            needsRedraw = false;
        }

        hidScanInput();
        u32 kDown = hidKeysDown();

        if (kDown & KEY_B) {
            break; 
        }

        if (kDown & (KEY_DOWN | KEY_CPAD_DOWN)) {
            if (!books.empty()) {
                selectedBook = (selectedBook + 1) % books.size();
                needsRedraw = true;
            }
        }

        if (kDown & (KEY_UP | KEY_CPAD_UP)) {
            if (!books.empty()) {
                selectedBook = (selectedBook - 1 + books.size()) % books.size();
                needsRedraw = true;
            }
        }

        if (kDown & KEY_A) {
            if (!books.empty()) {
                std::string path = dirPath + "/" + books[selectedBook];
                readAndDisplayBook(path.c_str()); // <-- Changed to call the new function
                needsRedraw = true; 
            }
        }
        
        gspWaitForVBlank();
    }
}


int main(int argc, char** argv) {
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    loadSettings(currentSettings);

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

    std::map<std::string, std::vector<std::string>> ePubsByDirectory;
    recursiveSearchEPub(ebookDir, ePubsByDirectory);
    
    if (ePubsByDirectory.empty()) {
        consoleClear();
        printf("No .epub files found in '%s' or its subdirectories.\n", ebookDir);
        printf("\nPress START to exit.\n");
        while (aptMainLoop()) {
            hidScanInput();
            if (hidKeysDown() & KEY_START) break;
            gspWaitForVBlank();
        }
        gfxExit();
        return 0;
    }
    
    std::vector<std::string> directories;
    for (const auto& pair : ePubsByDirectory) {
        directories.push_back(pair.first);
    }
    std::sort(directories.begin(), directories.end());

    int selectedDir = 0;
    
    drawDirectoryMenu(directories, selectedDir);
    gfxFlushBuffers();
    gfxSwapBuffers();

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();

        bool needsRedraw = false;

        if (kDown & KEY_START) break;

        if (kDown & (KEY_DOWN | KEY_CPAD_DOWN)) {
            selectedDir = (selectedDir + 1) % directories.size();
            needsRedraw = true;
        }
        if (kDown & (KEY_UP | KEY_CPAD_UP)) {
            selectedDir = (selectedDir - 1 + directories.size()) % directories.size();
            needsRedraw = true;
        }
        if (kDown & KEY_A) {
            const std::string& selectedDirPath = directories[selectedDir];
            const std::vector<std::string>& booksInDir = ePubsByDirectory.at(selectedDirPath);
            displayBookMenu(selectedDirPath, booksInDir);
            needsRedraw = true;
        }
        if (kDown & KEY_SELECT) {
            displaySettingsMenu();
            needsRedraw = true;
        }

        if (needsRedraw) {
            drawDirectoryMenu(directories, selectedDir);
            gfxFlushBuffers();
            gfxSwapBuffers();
        }
        
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}