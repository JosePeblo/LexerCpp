#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <string>
#include <regex>
#include <pthread.h>
#include <chrono>

#define THREADS 8

using namespace std::chrono;

typedef struct {
    std::regex expr;
    std::string replace;
} RegexReplacer;

typedef struct {
    std::stringstream buff;
    std::string res;
} Block;

std::vector<RegexReplacer> expressions {
    {std::regex("//.*$"),                                       "\\$c\\$&\\$\\"}, // Comments
    {std::regex("\"[^\"]*\""),                                  "\\$s\\$&\\$\\"}, // Strings
    {std::regex("[^\\w]\\d+(\\.\\d*)*f?"),                            "\\$n\\$&\\$\\"}, // Numbers
    {std::regex("\\{|\\}|\\(|\\)|\\[|\\]"),                     "\\$d\\$&\\$\\"}, // Delimiters
    {std::regex("\\+{1,2}=?|\\*=?|-{1,2}=?|[^/]/[^/]|%|<{1,2}=?|>{1,2}=?|\\?|:|&{1,2}|\\|{1,2}|!=?|={1,2}"), "\\$o\\$&\\$\\"}, // Operators
    {std::regex("(using|namespace|public|class|private|readonly|int|char|float|static|this|out|if|else|for|while|string|return|foreach|bool|do|enum|is|new|struct|protected|trow|goto|override|uint|virtual|void|double|case|const|ulong|short|try|catch)(?!\\w)"), "\\$k\\$&\\$\\"} // Keywords
};

std::vector<RegexReplacer> decoder {
    {std::regex("\\\\\\$c\\\\"), "<span class='c'>"},
    {std::regex("\\\\\\$s\\\\"), "<span class='s'>"},
    {std::regex("\\\\\\$n\\\\"), "<span class='n'>"},
    {std::regex("\\\\\\$d\\\\"), "<span class='d'>"},
    {std::regex("\\\\\\$o\\\\"), "<span class='o'>"},
    {std::regex("\\\\\\$k\\\\"), "<span class='k'>"},
    {std::regex("\\\\\\$\\\\"),  "</span>"}
};

std::string head = "<html><head><link rel=\"stylesheet\" href=\"style.css\"><head><body>\n";
std::string foot = "</body></html>";

std::string ReplaceElements(std::string str, std::vector<RegexReplacer> vec)
{
    for(auto elem : vec)
    {
        str = std::regex_replace(str, elem.expr, elem.replace);
    }
    return str;
}

milliseconds synchronous (const std::string& input, const std::string& output) 
{
    std::ifstream inputFile(input);
    std::ofstream outputFile(output);

    
    std::stringstream ss;
    std::string line;

    high_resolution_clock::time_point start, end;
    start = high_resolution_clock::now();

    outputFile << head;

    while(std::getline(inputFile, line))
    {
        ss << "<pre>" << ReplaceElements(line, expressions) << '\n';
    }

    outputFile << ReplaceElements(ss.str(), decoder);
    outputFile << foot;
    outputFile.close();

    end = high_resolution_clock::now();
    auto durationSingle = duration_cast<milliseconds>(end - start);
    std::cout << "Time single: " << durationSingle.count() << '\n';

    return durationSingle;
}

void* asyncParse (void* param)
{
    Block* block;
    block = (Block*) param;

    std::stringstream ss;
    std::string line;
    while(std::getline(block->buff, line))
    {
        ss << "<pre>" << ReplaceElements(line, expressions) << '\n';
    }

    block->res = ReplaceElements(ss.str(), decoder);

    pthread_exit(0);
}

milliseconds asynchronous (const std::string& input, const std::string& output) 
{
    std::ifstream lineCount(input);
    std::ofstream outputFile(output);
    int lines = 0;

    std::string line;

    high_resolution_clock::time_point start, end;
    start = high_resolution_clock::now();

    while(std::getline(lineCount, line))
        lines++;

    lineCount.close();

    std::ifstream inputFile(input);

    Block blocks [THREADS];
    pthread_t tids [THREADS];

    for(int i = 0; i < THREADS; i++)
    {
        int linesPerThread = lines / THREADS;
        while(linesPerThread > 0 && std::getline(inputFile, line))
        {
            blocks[i].buff << line << '\n';
            linesPerThread--;
        }
        if(i == THREADS - 1)
        {
            while(std::getline(inputFile, line))
            {
                blocks[i].buff << line << '\n';
            }
        }
        pthread_create(&tids[i], NULL, asyncParse, (void*) &blocks[i]);
    }
    
    outputFile << head;

    for(int i = 0; i < THREADS; i++) 
    {
        pthread_join(tids[i], NULL);
    }

    for(int i = 0; i < THREADS; i++) 
    {
        outputFile << blocks[i].res;
    }
    
    outputFile << foot;
    outputFile.close();

    end = high_resolution_clock::now();
    auto durationMulti = duration_cast<milliseconds>(end - start);
    std::cout << "Time multi: " << durationMulti.count() << '\n';

    return durationMulti;
}

int main(int argc, char* argv[]) 
{
    if(argc < 3)
    {
        std::cout << "Usage is \n>program.exe input.cs output.html";
        std::exit(1);
    }
    std::string input = argv[1];
    std::string output = argv[2];

    auto timeSync = synchronous(input, output);
    auto timeAsync = asynchronous(input, "async.html");
    
    float speedup = timeSync /  timeAsync;
    std::cout << "Speedup with " << THREADS << " processors: " << speedup << '\n';

    return 0;
}
