#include <filesystem>
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
    int start;
    int end;
    char** files;
} Block;

std::vector<RegexReplacer> expressions {
    {std::regex("//.*$"),                                       "\\$c\\$&\\$\\"}, // Comments
    {std::regex("\"[^\"]*\""),                                  "\\$s\\$&\\$\\"}, // Strings
    {std::regex("'[^']*'"),                                  "\\$s\\$&\\$\\"}, // Strings
    {std::regex("[^\\w\\{]\\d+(\\.\\d*)*f?"),                   "\\$n\\$&\\$\\"}, // Numbers
    {std::regex("\\w* *(?=\\()"),                               "\\$f\\$&\\$\\"}, // Functions
    {std::regex("\\{|\\}|\\(|\\)|\\[|\\]"),                     "\\$d\\$&\\$\\"}, // Delimiters
    {std::regex("\\+{1,2}=?|\\*=?|-{1,2}=?|[^/]/(?!/)|%|<{1,2}=?|>{1,2}=?|&{1,2}|\\|{1,2}|!=?|={1,2}"), "\\$o\\$&\\$\\"}, // Operators
    {std::regex("(using|namespace|public|class|private|readonly|int|char|float|static|this|out|if|else|for|while|string|return|foreach|bool|do|enum|new|struct|protected|trow|goto|override|uint|virtual|void|double|case|const|ulong|short|try|catch)(?!\\w)"), "\\$k\\$&\\$\\"} // Keywords
};

std::vector<RegexReplacer> decoder {
    {std::regex("\\\\\\$c\\\\"), "<span class='c'>"},
    {std::regex("\\\\\\$s\\\\"), "<span class='s'>"},
    {std::regex("\\\\\\$n\\\\"), "<span class='n'>"},
    {std::regex("\\\\\\$d\\\\"), "<span class='d'>"},
    {std::regex("\\\\\\$o\\\\"), "<span class='o'>"},
    {std::regex("\\\\\\$k\\\\"), "<span class='k'>"},
    {std::regex("\\\\\\$f\\\\"), "<span class='f'>"},
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

void* asyncParse (void* param)
{
    Block* block;
    block = (Block*) param;

    for(int i = block->start; i <= block->end; i++)
    {
        std::string path (block->files[i]);
        std::ifstream inputFile(path);
        std::ofstream outputFile(path.substr(12, path.length()) + ".multi.html");
        std::stringstream ss;
        std::string line;

        outputFile << head;

        while(std::getline(inputFile, line))
        {
            ss << "<pre>" << ReplaceElements(line, expressions) << '\n';
        }

        outputFile << ReplaceElements(ss.str(), decoder);
        outputFile << foot;
        outputFile.close();
    }
    pthread_exit(0);
}

int main(int argc, char* argv[]) 
{
    high_resolution_clock::time_point start, end;
    start = high_resolution_clock::now();

    for(int i = 1; i < argc; i++) 
    {
        std::string path (argv[i]);
        std::ifstream inputFile(path);
        std::ofstream outputFile(path.substr(12, path.length()) + ".single.html");

        std::stringstream ss;
        std::string line;

        outputFile << head;

        while(std::getline(inputFile, line))
        {
            ss << "<pre>" << ReplaceElements(line, expressions) << '\n';
        }

        outputFile << ReplaceElements(ss.str(), decoder);
        outputFile << foot;
        outputFile.close();
    }

    end = high_resolution_clock::now();
    auto durationSingle = duration_cast<milliseconds>(end - start).count();
    std::cout << "Time single: " << durationSingle << "ms" << '\n';


    start = high_resolution_clock::now();

    Block blocks [THREADS];
    pthread_t tids [THREADS];

    int blockSize = (argc - 1) / THREADS;
    for(int i = 0; i < THREADS; i++)
    {
        blocks[i].files = argv;
        blocks[i].start = i * blockSize + 1;
        blocks[i].end = (i != (THREADS - 1))? (i + 1) * blockSize : argc - 1;

        

        pthread_create(&tids[i], NULL, asyncParse, (void*) &blocks[i]);
    }
    
    for(int i = 0; i < THREADS; i++) 
    {
        pthread_join(tids[i], NULL);
    }

    end = high_resolution_clock::now();
    auto durationMulti = duration_cast<milliseconds>(end - start).count();
    std::cout << "Time multi: " << durationMulti << '\n';  

    float speedup = durationSingle /  durationMulti;
    std::cout << "Speedup with " << THREADS << " processors: " << speedup << '\n';


    return 0;
}
