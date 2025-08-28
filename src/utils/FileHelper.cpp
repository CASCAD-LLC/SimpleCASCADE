#include <string>
#include <cstdio>

struct FileHelper {
    static std::string exec(const std::string& cmd) {
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) return "ERROR";
        char buffer[128];
        std::string result;
        while (fgets(buffer, sizeof(buffer), pipe)) result += buffer;
        pclose(pipe);
        return result;
    }
};
